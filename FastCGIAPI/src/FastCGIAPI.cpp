
#include "Compressor.h"
#include <deque>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <sys/utsname.h>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "JSONUtils.h"
#include "spdlog/spdlog.h"
#include <curl/curl.h>
#include "FastCGIAPI.h" // has to be the last one otherwise errors...

extern char **environ;

FastCGIAPI::FastCGIAPI(json configurationRoot, mutex *fcgiAcceptMutex) { init(configurationRoot, fcgiAcceptMutex); }

FastCGIAPI::~FastCGIAPI() = default;

string FastCGIAPI::escape(const string &url)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		throw runtime_error("curl_easy_init failed");

	char *encoded = curl_easy_escape(curl, url.c_str(), url.size());
	if (!encoded)
	{
		curl_easy_cleanup(curl);
		throw runtime_error("curl_easy_escape failed");
	}

	string buffer = encoded;

	curl_free(encoded);

	curl_easy_cleanup(curl);

	return buffer;
}

string FastCGIAPI::unescape(const string &url)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		throw runtime_error("curl_easy_init failed");

	int decodelen;
	char *decoded = curl_easy_unescape(curl, url.c_str(), url.size(), &decodelen);
	if (!decoded)
	{
		curl_easy_cleanup(curl);
		throw runtime_error("curl_easy_unescape failed");
	}

	string buffer = decoded;

	curl_free(decoded);

	curl_easy_cleanup(curl);

	return buffer;
}

void FastCGIAPI::init(json configurationRoot, mutex *fcgiAcceptMutex)
{
	_shutdown = false;
	// _configurationRoot = configurationRoot;
	_fcgiAcceptMutex = fcgiAcceptMutex;

	_fcgxFinishDone = false;

	{
		struct utsname unUtsname;
		if (uname(&unUtsname) != -1)
			_hostName = unUtsname.nodename;
	}

	_requestIdentifier = 0;

	loadConfiguration(configurationRoot);
}

void FastCGIAPI::loadConfiguration(json configurationRoot)
{
	_maxAPIContentLength = JSONUtils::asInt64(configurationRoot["api"], "maxContentLength", 0);
	SPDLOG_TRACE(
		"Configuration item"
		", api->maxContentLength: {}",
		_maxAPIContentLength
	);
}

int FastCGIAPI::operator()()
{
	string sThreadId;
	{
		thread::id threadId = this_thread::get_id();
		stringstream ss;
		ss << threadId;
		sThreadId = ss.str();
	}

	FCGX_Request request;

	// 0 is file number for STDIN by default
	// The fastcgi process is launched by spawn-fcgi (see scripts/mmsApi.sh
	// scripts/mmsEncoder.sh) specifying the port to be used to listen to nginx
	// calls The nginx process is configured to proxy the requests to
	// 127.0.0.1:<port> specified by spawn-fcgi
	int sock_fd = 0;
	SPDLOG_TRACE(
		"FastCGIAPI::FCGX_OpenSocket"
		", threadId: {}"
		", sock_fd: {}",
		sThreadId, sock_fd
	);
	FCGX_InitRequest(&request, sock_fd, 0);

	while (!_shutdown)
	{
		_requestIdentifier++;

		int returnAcceptCode;
		{
			SPDLOG_TRACE(
				"FastCGIAPI::ready"
				", _requestIdentifier: {}"
				", threadId: {}",
				_requestIdentifier, sThreadId
			);
			lock_guard<mutex> locker(*_fcgiAcceptMutex);

			SPDLOG_TRACE(
				"FastCGIAPI::listen"
				", _requestIdentifier: {}"
				", threadId: {}",
				_requestIdentifier, sThreadId
			);

			if (_shutdown)
				continue;

			returnAcceptCode = FCGX_Accept_r(&request);
		}
		SPDLOG_TRACE(
			"FCGX_Accept_r"
			", _requestIdentifier: {}"
			", threadId: {}"
			", returnAcceptCode: {}",
			_requestIdentifier, sThreadId, returnAcceptCode
		);

		if (returnAcceptCode != 0)
		{
			_shutdown = true;

			FCGX_Finish_r(&request);

			continue;
		}

		_fcgxFinishDone = false;

		SPDLOG_TRACE(
			"Request to be managed"
			", _requestIdentifier: {}"
			", threadId: {}",
			_requestIdentifier, sThreadId
		);

		unordered_map<string, string> requestDetails;
		unordered_map<string, string> queryParameters;
		string requestBody;
		unsigned long contentLength = 0;
		try
		{
			fillEnvironmentDetails(request.envp, requestDetails);
			fillEnvironmentDetails(environ, requestDetails);

			{
				unordered_map<string, string>::iterator it;

				if ((it = requestDetails.find("QUERY_STRING")) != requestDetails.end())
					fillQueryString(it->second, queryParameters);
			}

			{
				unordered_map<string, string>::iterator it;
				if ((it = requestDetails.find("REQUEST_METHOD")) != requestDetails.end() && (it->second == "POST" || it->second == "PUT"))
				{
					if ((it = requestDetails.find("CONTENT_LENGTH")) != requestDetails.end())
					{
						if (it->second != "")
						{
							contentLength = stol(it->second);
							if (contentLength > _maxAPIContentLength)
							{
								string errorMessage = std::format(
									"ContentLength too long"
									", _requestIdentifier: {}"
									", threadId: {}"
									", contentLength: {}"
									", _maxAPIContentLength: {}",
									_requestIdentifier, sThreadId, contentLength, _maxAPIContentLength
								);

								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else
						{
							contentLength = 0;
						}
					}
					else
					{
						contentLength = 0;
					}

					if (contentLength > 0)
					{
						char *content = new char[contentLength];

						contentLength = FCGX_GetStr(content, contentLength, request.in);

						requestBody.assign(content, contentLength);

						delete[] content;
					}
				}
			}
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(e.what());

			sendError(request, 500, e.what());

			if (!_fcgxFinishDone)
				FCGX_Finish_r(&request);

			// throw runtime_error(errorMessage);
			continue;
		}
		catch (exception &e)
		{
			string errorMessage = "Internal server error";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			if (!_fcgxFinishDone)
				FCGX_Finish_r(&request);

			// throw runtime_error(errorMessage);
			continue;
		}

		string requestURI;
		{
			unordered_map<string, string>::iterator it;

			if ((it = requestDetails.find("REQUEST_URI")) != requestDetails.end())
				requestURI = it->second;
		}

		json permissionsRoot;
		bool authorizationPresent = basicAuthenticationRequired(requestURI, queryParameters);
		string userName;
		string password;
		if (authorizationPresent)
		{
			try
			{
				unordered_map<string, string>::iterator it;

				if ((it = requestDetails.find("HTTP_AUTHORIZATION")) == requestDetails.end())
				{
					SPDLOG_ERROR("No 'Basic' authorization is present into the request");

					throw CheckAuthorizationFailed();
				}

				string authorizationPrefix = "Basic ";
				if (!(it->second.size() >= authorizationPrefix.size() && 0 == it->second.compare(0, authorizationPrefix.size(), authorizationPrefix)))
				{
					SPDLOG_ERROR(
						"No 'Basic' authorization is present into the request"
						", _requestIdentifier: {}"
						", threadId: {}"
						", Authorization: {}",
						_requestIdentifier, sThreadId, it->second
					);

					throw CheckAuthorizationFailed();
				}

				string usernameAndPasswordBase64 = it->second.substr(authorizationPrefix.length());
				string usernameAndPassword = base64_decode(usernameAndPasswordBase64);
				size_t userNameSeparator = usernameAndPassword.find(":");
				if (userNameSeparator == string::npos)
				{
					SPDLOG_ERROR(
						"Wrong Authorization format"
						", _requestIdentifier: {}"
						", threadId: {}"
						", usernameAndPasswordBase64: {}"
						", usernameAndPassword: {}",
						_requestIdentifier, sThreadId, usernameAndPasswordBase64, usernameAndPassword
					);

					throw CheckAuthorizationFailed();
				}

				userName = usernameAndPassword.substr(0, userNameSeparator);
				password = usernameAndPassword.substr(userNameSeparator + 1);

				checkAuthorization(sThreadId, userName, password);
			}
			catch (CheckAuthorizationFailed &e)
			{
				SPDLOG_ERROR(
					"checkAuthorization failed"
					", _requestIdentifier: {}"
					", threadId: {}"
					", e.what(): {}",
					_requestIdentifier, sThreadId, e.what()
				);

				string errorMessage = e.what();
				SPDLOG_ERROR(errorMessage);

				sendError(request, 401, errorMessage); // unauthorized

				if (!_fcgxFinishDone)
					FCGX_Finish_r(&request);

				//  throw runtime_error(errorMessage);
				continue;
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					"checkAuthorization failed"
					", _requestIdentifier: {}"
					", threadId: {}"
					", e.what(): {}",
					_requestIdentifier, sThreadId, e.what()
				);

				string errorMessage = string("Internal server error");
				SPDLOG_ERROR(errorMessage);

				sendError(request, 500, errorMessage);

				if (!_fcgxFinishDone)
					FCGX_Finish_r(&request);

				// throw runtime_error(errorMessage);
				continue;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					"checkAuthorization failed"
					", _requestIdentifier: {}"
					", threadId: {}"
					", e.what(): {}",
					_requestIdentifier, sThreadId, e.what()
				);

				string errorMessage = "Internal server error";
				SPDLOG_ERROR(errorMessage);

				sendError(request, 500, errorMessage);

				if (!_fcgxFinishDone)
					FCGX_Finish_r(&request);

				//  throw runtime_error(errorMessage);
				continue;
			}
		}

		chrono::system_clock::time_point startManageRequest = chrono::system_clock::now();
		try
		{
			unordered_map<string, string>::iterator it;

			string requestMethod;
			if ((it = requestDetails.find("REQUEST_METHOD")) != requestDetails.end())
				requestMethod = it->second;

			bool responseBodyCompressed = false;
			{
				unordered_map<string, string>::iterator it;

				if ((it = requestDetails.find("HTTP_X_RESPONSEBODYCOMPRESSED")) != requestDetails.end() && it->second == "true")
				{
					responseBodyCompressed = true;
				}
			}

			manageRequestAndResponse(
				sThreadId, _requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, queryParameters, authorizationPresent,
				userName, password, contentLength, requestBody, requestDetails
			);
		}
		catch (JsonFieldNotFound &e)
		{
			SPDLOG_ERROR(
				"manageRequestAndResponse failed"
				", _requestIdentifier: {}"
				", threadId: {}"
				", e: {}",
				_requestIdentifier, sThreadId, e.what()
			);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"manageRequestAndResponse failed"
				", _requestIdentifier: {}"
				", threadId: {}"
				", e: {}",
				_requestIdentifier, sThreadId, e.what()
			);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"manageRequestAndResponse failed"
				", _requestIdentifier: {}"
				", threadId: {}"
				", e: {}",
				_requestIdentifier, sThreadId, e.what()
			);
		}
		{
			string method = getQueryParameter(queryParameters, "method", "", false);

			string clientIPAddress = getClientIPAddress(requestDetails);

			chrono::system_clock::time_point endManageRequest = chrono::system_clock::now();
			if (!requestURI.ends_with("/status"))
				SPDLOG_INFO(
					"manageRequestAndResponse"
					", _requestIdentifier: {}"
					", threadId: {}"
					", clientIPAddress: @{}@"
					", method: @{}@"
					", requestURI: {}"
					", authorizationPresent: {}"
					", @MMS statistics@ - manageRequestDuration (millisecs): @{}@",
					_requestIdentifier, sThreadId, clientIPAddress, method, requestURI, authorizationPresent,
					chrono::duration_cast<chrono::milliseconds>(endManageRequest - startManageRequest).count()
				);
		}

		SPDLOG_TRACE(
			"FastCGIAPI::request finished"
			", _requestIdentifier: {}"
			", threadId: {}",
			_requestIdentifier, sThreadId
		);

		if (!_fcgxFinishDone)
			FCGX_Finish_r(&request);

		// Note: the fcgi_streambuf destructor will auto flush
	}

	SPDLOG_INFO(
		"FastCGIAPI shutdown"
		", threadId: {}",
		sThreadId
	);

	return 0;
}

void FastCGIAPI::stopFastcgi() { _shutdown = true; }

bool FastCGIAPI::basicAuthenticationRequired(string requestURI, unordered_map<string, string> queryParameters)
{
	bool basicAuthenticationRequired = true;

	/*
	auto methodIt = queryParameters.find("method");
	if (methodIt == queryParameters.end())
	{
	  string errorMessage = string("The 'method' parameter is not found");
	  SPDLOG_ERROR(errorMessage);

	  // throw runtime_error(errorMessage);
	  return basicAuthenticationRequired;
	}
	string method = methodIt->second;

	if (method == "status"	// often used as healthy check
	)
	{
	  basicAuthenticationRequired = false;
	}
	*/

	return basicAuthenticationRequired;
}

void FastCGIAPI::sendSuccess(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestURI, string requestMethod,
	int htmlResponseCode, string responseBody, string contentType, string cookieName, string cookieValue, string cookiePath, bool enableCorsGETHeader,
	string originHeader
)
{
	if (_fcgxFinishDone)
	{
		// se viene chiamato due volte
		// sendSuccess/sendRedirect/sendHeadSuccess/sendError la seconda volta
		// provocherebbe un segmentation fault perchè probabilmente request.out è
		// stato resettato nella prima chiamata Questo controllo è una protezione
		// rispetto al segmentation fault
		SPDLOG_ERROR(
			"response was already done"
			", requestIdentifier: {}"
			", threadId: {}"
			", requestURI: {}"
			", requestMethod: {}"
			", responseBody.size: @{}@",
			requestIdentifier, sThreadId, requestURI, requestMethod, responseBody.size()
		);

		return;
	}

	string endLine = "\r\n";

	string httpStatus = std::format("Status: {} {}{}", htmlResponseCode, getHtmlStandardMessage(htmlResponseCode), endLine);

	string localContentType;
	if (responseBody != "")
	{
		if (contentType == "")
			localContentType = std::format("Content-Type: application/json; charset=utf-8{}", endLine);
		else
			localContentType = std::format("{}{}", contentType, endLine);
	}

	string cookieHeader;
	if (cookieName != "" && cookieValue != "")
	{
		cookieHeader = std::format("Set-Cookie: {}={}", cookieName, cookieValue);

		if (cookiePath != "")
			cookieHeader += ("; Path=" + cookiePath);

		cookieHeader += endLine;
	}

	string corsGETHeader;
	if (enableCorsGETHeader)
	{
		string origin = "*";
		if (originHeader != "")
			origin = originHeader;

		corsGETHeader = std::format(
			"Access-Control-Allow-Origin: {}{}"
			"Access-Control-Allow-Methods: GET, POST, OPTIONS{}"
			"Access-Control-Allow-Credentials: true{}"
			"Access-Control-Allow-Headers: "
			"DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,"
			"Content-Type,Range{}"
			"Access-Control-Expose-Headers: Content-Length,Content-Range{}",
			origin, endLine, endLine, endLine, endLine, endLine
		);
	}

	if (responseBodyCompressed)
	{
		string compressedResponseBody = Compressor::compress_string(responseBody);

		long contentLength = compressedResponseBody.size();

		string headResponse = std::format(
			"{}"
			"{}"
			"{}"
			"{}"
			"Content-Length: {}{}"
			"X-CompressedBody: true{}"
			"{}",
			httpStatus, localContentType, cookieHeader, corsGETHeader, contentLength, endLine, endLine, endLine
		);

		FCGX_FPrintF(request.out, headResponse.c_str());

		SPDLOG_INFO(
			"sendSuccess"
			", requestIdentifier: {}"
			", threadId: {}"
			", requestURI: {}"
			", requestMethod: {}"
			", headResponse.size: {}"
			", responseBody.size: @{}@"
			", compressedResponseBody.size: @{}@"
			", headResponse: {}",
			requestIdentifier, sThreadId, requestURI, requestMethod, headResponse.size(), responseBody.size(), contentLength, headResponse
		);

		FCGX_PutStr(compressedResponseBody.data(), compressedResponseBody.size(), request.out);
	}
	else
	{
		string completeHttpResponse;

		// 2020-02-08: content length has to be calculated before the substitution
		// from % to %% because for FCGX_FPrintF (below used) %% is just one
		// character
		long contentLength = responseBody.length();

		// responseBody cannot have the '%' char because FCGX_FPrintF will not work
		if (responseBody.find("%") != string::npos)
		{
			string toBeSearched = "%";
			string replacedWith = "%%";
			string newResponseBody = regex_replace(responseBody, regex(toBeSearched), replacedWith);

			completeHttpResponse = std::format(
				"{}"
				"{}"
				"{}"
				"{}"
				"Content-Length: {}{}"
				"{}"
				"{}",
				httpStatus, localContentType, cookieHeader, corsGETHeader, contentLength, endLine, endLine, newResponseBody
			);
		}
		else
		{
			completeHttpResponse = std::format(
				"{}"
				"{}"
				"{}"
				"{}"
				"Content-Length: {}{}"
				"{}"
				"{}",
				httpStatus, localContentType, cookieHeader, corsGETHeader, contentLength, endLine, endLine, responseBody
			);
		}

		if (!requestURI.ends_with("/status"))
			SPDLOG_INFO(
				"sendSuccess"
				", requestIdentifier: {}"
				", threadId: {}"
				", requestURI: {}"
				", requestMethod: {}"
				", responseBody.size: @{}@"
				", completeHttpResponse: {}",
				requestIdentifier, sThreadId, requestURI, requestMethod, responseBody.size(), completeHttpResponse
			);

		// si potrebbe usare anche FCGX_PutStr, in questo caso
		// non bisogna gestire %% (vedi sopra)
		// FCGX_PutStr(responseBody.data(), responseBody.size(), request.out);
		FCGX_FPrintF(request.out, completeHttpResponse.c_str());
	}

	FCGX_Finish_r(&request);
	_fcgxFinishDone = true;
}

void FastCGIAPI::sendRedirect(FCGX_Request &request, string locationURL)
{
	if (_fcgxFinishDone)
	{
		// se viene chiamato due volte
		// sendSuccess/sendRedirect/sendHeadSuccess/sendError la seconda volta
		// provocherebbe un segmentation fault perchè probabilmente request.out è
		// stato resettato nella prima chiamata Questo controllo è una protezione
		// rispetto al segmentation fault
		SPDLOG_ERROR("response was already done");

		return;
	}

	string endLine = "\r\n";

	int htmlResponseCode = 301;

	string completeHttpResponse = std::format(
		"Status: {} {}{}"
		"Location: {}{}{}",
		htmlResponseCode, getHtmlStandardMessage(htmlResponseCode), endLine, locationURL, endLine, endLine
	);

	SPDLOG_INFO(
		"HTTP Success"
		", response: {}",
		completeHttpResponse
	);

	FCGX_FPrintF(request.out, completeHttpResponse.c_str());

	FCGX_Finish_r(&request);
	_fcgxFinishDone = true;
}

void FastCGIAPI::sendHeadSuccess(FCGX_Request &request, int htmlResponseCode, unsigned long fileSize)
{
	if (_fcgxFinishDone)
	{
		// se viene chiamato due volte
		// sendSuccess/sendRedirect/sendHeadSuccess/sendError la seconda volta
		// provocherebbe un segmentation fault perchè probabilmente request.out è
		// stato resettato nella prima chiamata Questo controllo è una protezione
		// rispetto al segmentation fault
		SPDLOG_ERROR("response was already done");

		return;
	}

	string endLine = "\r\n";

	string httpStatus = std::format("Status: {} {}{}", htmlResponseCode, getHtmlStandardMessage(htmlResponseCode), endLine);

	string completeHttpResponse = std::format(
		"{}"
		"Content-Range: bytes 0-{}{}{}",
		httpStatus, fileSize, endLine, endLine
	);

	SPDLOG_INFO(
		"HTTP HEAD Success"
		", response: {}",
		completeHttpResponse
	);

	FCGX_FPrintF(request.out, completeHttpResponse.c_str());

	FCGX_Finish_r(&request);
	_fcgxFinishDone = true;
}

void FastCGIAPI::sendHeadSuccess(int htmlResponseCode, unsigned long fileSize)
{
	string endLine = "\r\n";

	string httpStatus = std::format("Status: {} {}{}", htmlResponseCode, getHtmlStandardMessage(htmlResponseCode), endLine);

	string completeHttpResponse = std::format(
		"{}"
		"X-CatraMMS-Resume: {}{}"
		"{}",
		httpStatus, fileSize, endLine, endLine
	);

	SPDLOG_INFO(
		"HTTP HEAD Success"
		", response: {}",
		completeHttpResponse
	);
}

void FastCGIAPI::sendError(FCGX_Request &request, int htmlResponseCode, string responseBody)
{
	if (_fcgxFinishDone)
	{
		// se viene chiamato due volte
		// sendSuccess/sendRedirect/sendHeadSuccess/sendError la seconda volta
		// provocherebbe un segmentation fault perchè probabilmente request.out è
		// stato resettato nella prima chiamata Questo controllo è una protezione
		// rispetto al segmentation fault
		SPDLOG_ERROR("response was already done");

		return;
	}

	string endLine = "\r\n";

	long contentLength;

	// string responseBody;
	// errorMessage cannot have the '%' char because FCGX_FPrintF will not work
	if (responseBody.find("%") != string::npos)
	{
		// json temporaryResponseBodyRoot;
		// temporaryResponseBodyRoot["status"] = to_string(htmlResponseCode);
		// temporaryResponseBodyRoot["error"] = errorMessage;

		// string temporaryResponseBody =
		// JSONUtils::toString(temporaryResponseBodyRoot);

		// 2020-02-08: content length has to be calculated before the substitution
		// from % to %% because for FCGX_FPrintF (below used) %% is just one
		// character
		contentLength = responseBody.length();

		string toBeSearched = "%";
		string replacedWith = "%%";
		responseBody = regex_replace(responseBody, regex(toBeSearched), replacedWith);
	}
	else
	{
		// json responseBodyRoot;
		// responseBodyRoot["status"] = to_string(htmlResponseCode);
		// responseBodyRoot["error"] = errorMessage;

		// responseBody = JSONUtils::toString(responseBodyRoot);

		// 2020-02-08: content length has to be calculated before the substitution
		// from % to %% because for FCGX_FPrintF (below used) %% is just one
		// character
		contentLength = responseBody.length();
	}

	string httpStatus = std::format("Status: {} {}{}", htmlResponseCode, getHtmlStandardMessage(htmlResponseCode), endLine);

	string completeHttpResponse = std::format(
		"{}"
		"Content-Type: application/json; charset=utf-8{}"
		"Content-Length: {}{}"
		"{}"
		"{}",
		httpStatus, endLine, contentLength, endLine, endLine, responseBody
	);

	SPDLOG_INFO(
		"HTTP Error"
		", response: {}",
		completeHttpResponse
	);

	FCGX_FPrintF(request.out, completeHttpResponse.c_str());

	FCGX_Finish_r(&request);
	_fcgxFinishDone = true;
}

/*
void FastCGIAPI::sendError(int htmlResponseCode, string errorMessage)
{
  string endLine = "\r\n";

  long contentLength;

  string responseBody;
  // errorMessage cannot have the '%' char because FCGX_FPrintF will not work
  if (errorMessage.find("%") != string::npos)
  {
	json temporaryResponseBodyRoot;
	temporaryResponseBodyRoot["status"] = to_string(htmlResponseCode);
	temporaryResponseBodyRoot["error"] = errorMessage;

	string temporaryResponseBody =
JSONUtils::toString(temporaryResponseBodyRoot);

	// 2020-02-08: content length has to be calculated before the substitution
from % to %%
	// because for FCGX_FPrintF (below used) %% is just one character
	contentLength = temporaryResponseBody.length();

	string toBeSearched = "%";
	string replacedWith = "%%";
	responseBody = regex_replace(temporaryResponseBody, regex(toBeSearched),
replacedWith);
  }
  else
  {
	json responseBodyRoot;
	responseBodyRoot["status"] = to_string(htmlResponseCode);
	responseBodyRoot["error"] = errorMessage;

	responseBody = JSONUtils::toString(responseBodyRoot);

	// 2020-02-08: content length has to be calculated before the substitution
from % to %%
	// because for FCGX_FPrintF (below used) %% is just one character
	contentLength = responseBody.length();
  }

  string httpStatus = std::format("Status: {} {}{}", htmlResponseCode,
getHtmlStandardMessage(htmlResponseCode), endLine);

  string completeHttpResponse = std::format(
	"{}"
	"Content-Type: application/json; charset=utf-8{}"
	"Content-Length: {}{}"
	"{}"
	"{}",
	httpStatus, endLine, contentLength, endLine, endLine, responseBody
  );

  SPDLOG_INFO(
	"HTTP Error"
	", response: {}",
	completeHttpResponse
  );
}
*/

string FastCGIAPI::getClientIPAddress(unordered_map<string, string> &requestDetails)
{

	string clientIPAddress;

	// REMOTE_ADDR is the address of the load balancer
	// auto remoteAddrIt = requestDetails.find("REMOTE_ADDR");
	auto remoteAddrIt = requestDetails.find("HTTP_X_FORWARDED_FOR");
	if (remoteAddrIt != requestDetails.end())
		clientIPAddress = remoteAddrIt->second;

	return clientIPAddress;
}

string FastCGIAPI::getHtmlStandardMessage(int htmlResponseCode)
{
	switch (htmlResponseCode)
	{
	case 200:
		return string("OK");
	case 201:
		return string("Created");
	case 301:
		return string("Moved Permanently");
	case 302:
		return string("Found");
	case 307:
		return string("Temporary Redirect");
	case 308:
		return string("Permanent Redirect");
	case 403:
		return string("Forbidden");
	case 400:
		return string("Bad Request");
	case 401:
		return string("Unauthorized");
	case 404:
		return string("Not Found");
	case 500:
		return string("Internal Server Error");
	default:
		string errorMessage = std::format(
			"HTTP status code not managed"
			", htmlResponseCode: {}",
			htmlResponseCode
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

int32_t FastCGIAPI::getQueryParameter(
	unordered_map<string, string> &queryParameters, string parameterName, int32_t defaultParameter, bool mandatory, bool *isParamPresent
)
{

	int32_t parameterValue;

	auto it = queryParameters.find(parameterName);
	if (it != queryParameters.end() && it->second != "")
	{
		if (isParamPresent != nullptr)
			*isParamPresent = true;
		parameterValue = stol(it->second);
	}
	else
	{
		if (isParamPresent != nullptr)
			*isParamPresent = false;
		if (mandatory)
		{
			string errorMessage = std::format("The {} query parameter is missing", parameterName);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		parameterValue = defaultParameter;
	}

	return parameterValue;
}

int64_t FastCGIAPI::getQueryParameter(
	unordered_map<string, string> &queryParameters, string parameterName, int64_t defaultParameter, bool mandatory, bool *isParamPresent
)
{

	int64_t parameterValue;

	auto it = queryParameters.find(parameterName);
	if (it != queryParameters.end() && it->second != "")
	{
		if (isParamPresent != nullptr)
			*isParamPresent = true;
		parameterValue = stoll(it->second);
	}
	else
	{
		if (isParamPresent != nullptr)
			*isParamPresent = false;
		if (mandatory)
		{
			string errorMessage = std::format("The {} query parameter is missing", parameterName);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		parameterValue = defaultParameter;
	}

	return parameterValue;
}

bool FastCGIAPI::getQueryParameter(
	unordered_map<string, string> &queryParameters, string parameterName, bool defaultParameter, bool mandatory, bool *isParamPresent
)
{

	bool parameterValue;

	auto it = queryParameters.find(parameterName);
	if (it != queryParameters.end() && it->second != "")
	{
		if (isParamPresent != nullptr)
			*isParamPresent = true;
		parameterValue = it->second == "true";
	}
	else
	{
		if (isParamPresent != nullptr)
			*isParamPresent = false;
		if (mandatory)
		{
			string errorMessage = std::format("The {} query parameter is missing", parameterName);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		parameterValue = defaultParameter;
	}

	return parameterValue;
}

string FastCGIAPI::getQueryParameter(
	unordered_map<string, string> &queryParameters, string parameterName, string defaultParameter, bool mandatory, bool *isParamPresent
)
{

	string parameterValue;

	auto it = queryParameters.find(parameterName);
	if (it != queryParameters.end() && it->second != "")
	{
		if (isParamPresent != nullptr)
			*isParamPresent = true;
		parameterValue = it->second;

		// 2021-01-07: Remark: we have FIRST to replace + in space and then apply
		// unescape
		//	That  because if we have really a + char (%2B into the string), and we
		// do the replace 	after unescape, this char will be changed to space and we
		// do not want it
		string plus = "\\+";
		string plusDecoded = " ";
		string firstDecoding = regex_replace(parameterValue, regex(plus), plusDecoded);

		parameterValue = unescape(firstDecoding);
	}
	else
	{
		if (isParamPresent != nullptr)
			*isParamPresent = false;
		if (mandatory)
		{
			string errorMessage = std::format("The {} query parameter is missing", parameterName);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		parameterValue = defaultParameter;
	}

	return parameterValue;
}

string FastCGIAPI::getQueryParameter(
	unordered_map<string, string> &queryParameters, string parameterName, const char *defaultParameter, bool mandatory, bool *isParamPresent
)
{
	return getQueryParameter(queryParameters, parameterName, string(defaultParameter), mandatory, isParamPresent);
}

vector<int32_t> FastCGIAPI::getQueryParameter(
	unordered_map<string, string> &queryParameters, string parameterName, char delim, vector<int32_t> defaultParameter, bool mandatory,
	bool *isParamPresent
)
{
	vector<int32_t> parameterValue;

	auto it = queryParameters.find(parameterName);
	if (it != queryParameters.end() && it->second != "")
	{
		if (isParamPresent != nullptr)
			*isParamPresent = true;
		stringstream ss(it->second);
		string token;
		while (getline(ss, token, delim))
		{
			if (!token.empty())
				parameterValue.push_back(stol(token));
		}
	}
	else
	{
		if (isParamPresent != nullptr)
			*isParamPresent = false;
		if (mandatory)
		{
			string errorMessage = std::format("The {} query parameter is missing", parameterName);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		parameterValue = defaultParameter;
	}

	return parameterValue;
}

vector<int64_t> FastCGIAPI::getQueryParameter(
	unordered_map<string, string> &queryParameters, string parameterName, char delim, vector<int64_t> defaultParameter, bool mandatory,
	bool *isParamPresent
)
{
	vector<int64_t> parameterValue;

	auto it = queryParameters.find(parameterName);
	if (it != queryParameters.end() && it->second != "")
	{
		if (isParamPresent != nullptr)
			*isParamPresent = true;
		stringstream ss(it->second);
		string token;
		while (getline(ss, token, delim))
		{
			if (!token.empty())
				parameterValue.push_back(stoll(token));
		}
	}
	else
	{
		if (isParamPresent != nullptr)
			*isParamPresent = false;
		if (mandatory)
		{
			string errorMessage = std::format("The {} query parameter is missing", parameterName);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		parameterValue = defaultParameter;
	}

	return parameterValue;
}

vector<string> FastCGIAPI::getQueryParameter(
	unordered_map<string, string> &queryParameters, string parameterName, char delim, vector<string> defaultParameter, bool mandatory,
	bool *isParamPresent
)
{
	vector<string> parameterValue;

	auto it = queryParameters.find(parameterName);
	if (it != queryParameters.end() && it->second != "")
	{
		if (isParamPresent != nullptr)
			*isParamPresent = true;
		stringstream ss(it->second);
		string token;
		while (getline(ss, token, delim))
		{
			if (!token.empty())
				parameterValue.push_back(token);
		}
	}
	else
	{
		if (isParamPresent != nullptr)
			*isParamPresent = false;
		if (mandatory)
		{
			string errorMessage = std::format("The {} query parameter is missing", parameterName);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		parameterValue = defaultParameter;
	}

	return parameterValue;
}

set<string> FastCGIAPI::getQueryParameter(
	unordered_map<string, string> &queryParameters, string parameterName, char delim, set<string> defaultParameter, bool mandatory,
	bool *isParamPresent
)
{
	set<string> parameterValue;

	auto it = queryParameters.find(parameterName);
	if (it != queryParameters.end() && it->second != "")
	{
		if (isParamPresent != nullptr)
			*isParamPresent = true;
		stringstream ss(it->second);
		string token;
		while (getline(ss, token, delim))
		{
			if (!token.empty())
				parameterValue.insert(token);
		}
	}
	else
	{
		if (isParamPresent != nullptr)
			*isParamPresent = false;
		if (mandatory)
		{
			string errorMessage = std::format("The {} query parameter is missing", parameterName);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		parameterValue = defaultParameter;
	}

	return parameterValue;
}

void FastCGIAPI::fillEnvironmentDetails(const char *const *envp, unordered_map<string, string> &requestDetails)
{

	int valueIndex;

	for (; *envp; ++envp)
	{
		string environmentKeyValue = *envp;

		if ((valueIndex = environmentKeyValue.find("=")) == string::npos)
		{
			SPDLOG_ERROR(
				"Unexpected environment variable"
				", environmentKeyValue: {}",
				environmentKeyValue
			);

			continue;
		}

		string key = environmentKeyValue.substr(0, valueIndex);
		string value = environmentKeyValue.substr(valueIndex + 1);

		requestDetails[key] = value;

		if (key == "REQUEST_URI")
			SPDLOG_TRACE(
				"Environment variable"
				", key/Name: {}={}",
				key, value
			);
		else
			SPDLOG_TRACE(
				"Environment variable"
				", key/Name: {}={}",
				key, value
			);
	}
}

void FastCGIAPI::fillQueryString(string queryString, unordered_map<string, string> &queryParameters)
{

	stringstream ss(queryString);
	string token;
	char delim = '&';
	while (getline(ss, token, delim))
	{
		if (!token.empty())
		{
			size_t keySeparator;

			if ((keySeparator = token.find("=")) == string::npos)
			{
				SPDLOG_ERROR(
					"Wrong query parameter format"
					", token: {}",
					token
				);

				continue;
			}

			string key = token.substr(0, keySeparator);
			string value = token.substr(keySeparator + 1);

			queryParameters[key] = value;

			SPDLOG_TRACE(
				"Query parameter"
				", key/Name: {}={}",
				key, value
			);
		}
	}
}

/*
json FastCGIAPI::loadConfigurationFile(const char *configurationPathName)
{
	try
	{
		ifstream configurationFile(configurationPathName, ifstream::binary);

		return json::parse(
			configurationFile,
			nullptr, // callback
			true,	 // allow exceptions
			true	 // ignore_comments
		);
	}
	catch (...)
	{
		string errorMessage = std::format(
			"wrong json configuration format"
			", configurationPathName: {}",
			configurationPathName
		);

		throw runtime_error(errorMessage);
	}
}
*/

// #define BOOTSERVICE_DEBUG_LOG

json FastCGIAPI::loadConfigurationFile(string configurationPathName, string environmentPrefix)
{

#ifdef BOOTSERVICE_DEBUG_LOG
	ofstream of("/tmp/bootservice.log", ofstream::app);
	of << "loadConfigurationFile..." << endl;
#endif

	string sConfigurationFile;
	{
		ifstream configurationFile(configurationPathName, ifstream::binary);
		stringstream buffer;
		buffer << configurationFile.rdbuf();
		if (environmentPrefix == "")
			sConfigurationFile = buffer.str();
		else
			sConfigurationFile = FastCGIAPI::applyEnvironmentToConfiguration(buffer.str(), environmentPrefix);
	}

	json configurationRoot = json::parse(
		sConfigurationFile,
		nullptr, // callback
		true,	 // allow exceptions
		true	 // ignore_comments
	);

	return configurationRoot;
}

string FastCGIAPI::applyEnvironmentToConfiguration(string configuration, string environmentPrefix)
{
	char **s = environ;

#ifdef BOOTSERVICE_DEBUG_LOG
	ofstream of("/tmp/bootservice.log", ofstream::app);
#endif

	int envNumber = 0;
	for (; *s; s++)
	{
		string envVariable = *s;
#ifdef BOOTSERVICE_DEBUG_LOG
//					of << "ENV " << *s << endl;
#endif
		if (envVariable.starts_with(environmentPrefix))
		{
			size_t endOfVarName = envVariable.find("=");
			if (endOfVarName == string::npos)
				continue;

			envNumber++;

			// sarebbe \$\{ZORAC_SOLR_PWD\}
			string envLabel = std::format("\\$\\{{{}\\}}", envVariable.substr(0, endOfVarName));
			string envValue = envVariable.substr(endOfVarName + 1);
#ifdef BOOTSERVICE_DEBUG_LOG
			of << "ENV " << envLabel << ": " << envValue << endl;
#endif
			configuration = regex_replace(configuration, regex(envLabel), envValue);
		}
	}

	return configuration;
}

string FastCGIAPI::base64_encode(const string &in)
{
	string out;

	int val = 0, valb = -6;
	for (unsigned char c : in)
	{
		val = (val << 8) + c;
		valb += 8;
		while (valb >= 0)
		{
			out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val >> valb) & 0x3F]);
			valb -= 6;
		}
	}
	if (valb > -6)
		out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val << 8) >> (valb + 8)) & 0x3F]);
	while (out.size() % 4)
		out.push_back('=');
	return out;
}

string FastCGIAPI::base64_decode(const string &in)
{
	string out;

	vector<int> T(256, -1);
	for (int i = 0; i < 64; i++)
		T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

	int val = 0, valb = -8;
	for (unsigned char c : in)
	{
		if (T[c] == -1)
			break;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0)
		{
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}
