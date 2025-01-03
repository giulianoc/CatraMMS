/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   CURL.cpp
 * Author: giuliano
 *
 * Created on March 29, 2018, 6:27 AM
 */

#include "CurlWrapper.h"

#include "JSONUtils.h"
#include "catralibraries/Convert.h"
#include "catralibraries/StringUtils.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <curl/curl.h>
#include <regex>
#include <sstream>

#include <list>
#include <stdexcept>
#include <utility>

// https://everything.curl.dev/libcurl/callbacks/read
// https://github.com/chrisvana/curlpp_copy/blob/master/include/curlpp/Options.hpp
// https://curl.se/libcurl/c/CURLOPT_POST.html

void CurlWrapper::globalInitialize() { curl_global_init(CURL_GLOBAL_DEFAULT); }

void CurlWrapper::globalTerminate() { curl_global_cleanup(); }

string CurlWrapper::escape(const string &url)
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

string CurlWrapper::unescape(const string &url)
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

json CurlWrapper::httpGetJson(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, vector<string> otherHeaders,
	string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string response = CurlWrapper::httpGet(
		url, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, otherHeaders, referenceToLog, maxRetryNumber,
		secondsToWaitBeforeToRetry
	);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

pair<string, string> CurlWrapper::httpPostString(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string requestType = "POST";

	return CurlWrapper::httpPostPutString(
		url, requestType, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, body,
		contentType, // i.e.: application/json
		otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry
	);
}

string CurlWrapper::httpPutString(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string requestType = "PUT";

	pair<string, string> responseDetails = CurlWrapper::httpPostPutString(
		url, requestType, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, body,
		contentType, // i.e.: application/json
		otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry
	);

	return responseDetails.second;
}

json CurlWrapper::httpPostStringAndGetJson(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string response = CurlWrapper::httpPostString(
						  url, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, body, contentType, otherHeaders,
						  referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry
	)
						  .second;

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

json CurlWrapper::httpPutStringAndGetJson(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string response = CurlWrapper::httpPutString(
		url, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, body, contentType, otherHeaders, referenceToLog, maxRetryNumber,
		secondsToWaitBeforeToRetry
	);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

string CurlWrapper::httpPostFile(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
	int64_t fileSizeInBytes, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string requestType = "POST";

	return CurlWrapper::httpPostPutFile(
		url, requestType, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, pathFileName, fileSizeInBytes, referenceToLog,
		maxRetryNumber, secondsToWaitBeforeToRetry, contentRangeStart, contentRangeEnd_Excluded
	);
}

string CurlWrapper::httpPutFile(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
	int64_t fileSizeInBytes, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string requestType = "PUT";

	return CurlWrapper::httpPostPutFile(
		url, requestType, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, pathFileName, fileSizeInBytes, referenceToLog,
		maxRetryNumber, secondsToWaitBeforeToRetry, contentRangeStart, contentRangeEnd_Excluded
	);
}

json CurlWrapper::httpPostFileAndGetJson(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
	int64_t fileSizeInBytes, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string response = CurlWrapper::httpPostFile(
		url, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, pathFileName, fileSizeInBytes, referenceToLog, maxRetryNumber,
		secondsToWaitBeforeToRetry, contentRangeStart, contentRangeEnd_Excluded
	);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

json CurlWrapper::httpPutFileAndGetJson(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
	int64_t fileSizeInBytes, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string response = CurlWrapper::httpPutFile(
		url, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, pathFileName, fileSizeInBytes, referenceToLog, maxRetryNumber,
		secondsToWaitBeforeToRetry, contentRangeStart, contentRangeEnd_Excluded
	);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

string CurlWrapper::httpPostFileSplittingInChunks(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
	int64_t fileSizeInBytes, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	int64_t chunkSize = 100 * 1000 * 1000;

	if (fileSizeInBytes <= chunkSize)
		return httpPostFile(
			url, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, pathFileName, fileSizeInBytes, referenceToLog,
			maxRetryNumber, secondsToWaitBeforeToRetry
		);

	int chunksNumber = fileSizeInBytes / chunkSize;
	if (fileSizeInBytes % chunkSize != 0)
		chunksNumber++;

	string lastHttpReturn;
	for (int chunkIndex = 0; chunkIndex < chunksNumber; chunkIndex++)
	{
		int64_t contentRangeStart = chunkIndex * chunkSize;
		int64_t contentRangeEnd_Excluded = chunkIndex + 1 < chunksNumber ? (chunkIndex + 1) * chunkSize : fileSizeInBytes;

		/*
		Commentato perchè ho fissato il problema. Prima impiegava troppo tempo
		a causa del manageTar.... ora non piu
		if (chunkIndex + 1 == chunksNumber
			&& url.find("https://mms-binary.") != string::npos
			&& url.find(":443") != string::npos
		)
		{
			// 2023-02-24: a causa del bilanciatore hetzner dove non è possibile
			//	settare il timeout, facciamo puntare direttamente ad un server
		senza passare
			//	dal bilanciatore
			string tmpURL = regex_replace(url, regex("https://mms-binary."),
		"http://mms-binary-tmp."); tmpURL = regex_replace(tmpURL, regex(":443"),
		":8090");

			lastHttpReturn = httpPostFile(
				tmpURL,
				timeoutInSeconds,
				basicAuthenticationUser,
				basicAuthenticationPassword,
				pathFileName,
				fileSizeInBytes,
				maxRetryNumber,
				secondsToWaitBeforeToRetry,
				contentRangeStart,
				contentRangeEnd_Excluded
			);
		}
		else
		*/
		{
			lastHttpReturn = httpPostFile(
				url, timeoutInSeconds, basicAuthenticationUser, basicAuthenticationPassword, pathFileName, fileSizeInBytes, referenceToLog,
				maxRetryNumber, secondsToWaitBeforeToRetry, contentRangeStart, contentRangeEnd_Excluded
			);
		}
	}

	return lastHttpReturn;
}

string CurlWrapper::httpPostFormData(
	string url, vector<pair<string, string>> formData, long timeoutInSeconds, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string requestType = "POST";

	return CurlWrapper::httpPostPutFormData(url, formData, requestType, timeoutInSeconds, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry);
}

string CurlWrapper::httpPutFormData(
	string url, vector<pair<string, string>> formData, long timeoutInSeconds, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string requestType = "PUT";

	return CurlWrapper::httpPostPutFormData(url, formData, requestType, timeoutInSeconds, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry);
}

json CurlWrapper::httpPostFormDataAndGetJson(
	string url, vector<pair<string, string>> formData, long timeoutInSeconds, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string response = CurlWrapper::httpPostFormData(url, formData, timeoutInSeconds, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

json CurlWrapper::httpPutFormDataAndGetJson(
	string url, vector<pair<string, string>> formData, long timeoutInSeconds, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string response = CurlWrapper::httpPutFormData(url, formData, timeoutInSeconds, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

string CurlWrapper::httpPostFileByFormData(
	string url, vector<pair<string, string>> formData, long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes, string mediaContentType,
	string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string requestType = "POST";

	return CurlWrapper::httpPostPutFileByFormData(
		url, formData, requestType, timeoutInSeconds, pathFileName, fileSizeInBytes, mediaContentType, referenceToLog, maxRetryNumber,
		secondsToWaitBeforeToRetry, contentRangeStart, contentRangeEnd_Excluded
	);
}

string CurlWrapper::httpPutFileByFormData(
	string url, vector<pair<string, string>> formData, long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes, string mediaContentType,
	string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string requestType = "PUT";

	return CurlWrapper::httpPostPutFileByFormData(
		url, formData, requestType, timeoutInSeconds, pathFileName, fileSizeInBytes, mediaContentType, referenceToLog, maxRetryNumber,
		secondsToWaitBeforeToRetry, contentRangeStart, contentRangeEnd_Excluded
	);
}

json CurlWrapper::httpPostFileByFormDataAndGetJson(
	string url, vector<pair<string, string>> formData, long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes, string mediaContentType,
	string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string response = CurlWrapper::httpPostFileByFormData(
		url, formData, timeoutInSeconds, pathFileName, fileSizeInBytes, mediaContentType, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry,
		contentRangeStart, contentRangeEnd_Excluded
	);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

json CurlWrapper::httpPutFileByFormDataAndGetJson(
	string url, vector<pair<string, string>> formData, long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes, string mediaContentType,
	string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string response = CurlWrapper::httpPutFileByFormData(
		url, formData, timeoutInSeconds, pathFileName, fileSizeInBytes, mediaContentType, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry,
		contentRangeStart, contentRangeEnd_Excluded
	);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

size_t curlDownloadCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
	SPDLOG_INFO(
		"BOOOOOOOOO"
		", size: {}"
		", nmemb: {}",
		size, nmemb
	);
	CurlWrapper::CurlDownloadData *curlDownloadData = (CurlWrapper::CurlDownloadData *)f;

	if (curlDownloadData->currentChunkNumber == 0)
	{
		(curlDownloadData->mediaSourceFileStream).open(curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::trunc);
		if (!curlDownloadData->mediaSourceFileStream)
		{
			SPDLOG_ERROR(
				"open file failed"
				"{}"
				", destBinaryPathName: {}",
				curlDownloadData->referenceToLog, curlDownloadData->destBinaryPathName
			);

			// throw runtime_error(message);
		}
		curlDownloadData->currentChunkNumber += 1;

		SPDLOG_INFO(
			"Opening binary file"
			"{}"
			", curlDownloadData -> destBinaryPathName: {}"
			", curlDownloadData->currentChunkNumber: {}"
			", curlDownloadData->currentTotalSize: {}"
			", curlDownloadData->maxChunkFileSize: {}",
			curlDownloadData->referenceToLog, curlDownloadData->destBinaryPathName, curlDownloadData->currentChunkNumber,
			curlDownloadData->currentTotalSize, curlDownloadData->maxChunkFileSize
		);
	}
	else if (curlDownloadData->currentTotalSize >= curlDownloadData->currentChunkNumber * curlDownloadData->maxChunkFileSize)
	{
		if (curlDownloadData->mediaSourceFileStream)
			(curlDownloadData->mediaSourceFileStream).close();

		// (curlDownloadData->mediaSourceFileStream).open(localPathFileName,
		// ios::binary | ios::out | ios::trunc);
		(curlDownloadData->mediaSourceFileStream).open(curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::app);
		if (!curlDownloadData->mediaSourceFileStream)
		{
			SPDLOG_ERROR(
				"open file failed"
				"{}"
				", destBinaryPathName: {}",
				curlDownloadData->referenceToLog, curlDownloadData->destBinaryPathName
			);

			// throw runtime_error(message);
		}
		curlDownloadData->currentChunkNumber += 1;

		SPDLOG_INFO(
			"Opening binary file"
			"{}"
			", curlDownloadData->destBinaryPathName: {}"
			", curlDownloadData->currentChunkNumber: {}"
			", curlDownloadData->currentTotalSize: {}"
			", curlDownloadData->maxChunkFileSize: {}",
			curlDownloadData->referenceToLog, curlDownloadData->destBinaryPathName, curlDownloadData->currentChunkNumber,
			curlDownloadData->currentTotalSize, curlDownloadData->maxChunkFileSize
		);
	}

	if (curlDownloadData->mediaSourceFileStream)
		curlDownloadData->mediaSourceFileStream.write(ptr, size * nmemb);
	curlDownloadData->currentTotalSize += (size * nmemb);

	return size * nmemb;
};

size_t curlUploadCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
	CurlWrapper::CurlUploadData *curlUploadData = (CurlWrapper::CurlUploadData *)f;

	int64_t currentFilePosition = curlUploadData->mediaSourceFileStream.tellg();

	/*
	logger->info(__FILEREF__ + "curlUploadCallback"
		+ ", currentFilePosition: " + to_string(currentFilePosition)
		+ ", size: " + to_string(size)
		+ ", nmemb: " + to_string(nmemb)
		+ ", curlUploadData->fileSizeInBytes: " +
	to_string(curlUploadData->fileSizeInBytes)
	);
	*/

	if (currentFilePosition + (size * nmemb) <= curlUploadData->upToByte_Excluded)
		curlUploadData->mediaSourceFileStream.read(ptr, size * nmemb);
	else
		curlUploadData->mediaSourceFileStream.read(ptr, curlUploadData->upToByte_Excluded - currentFilePosition);

	int64_t charsRead = curlUploadData->mediaSourceFileStream.gcount();

	curlUploadData->payloadBytesSent += charsRead;

	// Docs: Returning 0 will signal end-of-file to the library and cause it to
	// stop the current transfer
	return charsRead;
};

size_t curlUploadFormDataCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
	CurlWrapper::CurlUploadFormData *curlUploadFormData = (CurlWrapper::CurlUploadFormData *)f;

	int64_t currentFilePosition = curlUploadFormData->mediaSourceFileStream.tellg();

	// Docs: Returning 0 will signal end-of-file to the library and cause it to
	// stop the current transfer
	if (curlUploadFormData->endOfFormDataSent && currentFilePosition == curlUploadFormData->upToByte_Excluded)
		return 0;

	if (!curlUploadFormData->formDataSent)
	{
		if (curlUploadFormData->formData.size() > size * nmemb)
		{
			SPDLOG_ERROR(
				"Not enougth memory!!!"
				", curlUploadFormData->formDataSent: {}"
				", curlUploadFormData->formData: {}"
				", curlUploadFormData->endOfFormDataSent: {}"
				", curlUploadFormData->endOfFormData: {}"
				", curlUploadFormData->upToByte_Excluded: {}"
				", curlUploadFormData->formData.size(): {}"
				", size * nmemb: {}",
				curlUploadFormData->formDataSent, curlUploadFormData->formData, curlUploadFormData->endOfFormDataSent,
				curlUploadFormData->endOfFormData, curlUploadFormData->upToByte_Excluded, curlUploadFormData->formData.size(), size * nmemb
			);

			return CURL_READFUNC_ABORT;
		}

		strcpy(ptr, curlUploadFormData->formData.c_str());

		curlUploadFormData->formDataSent = true;

		// this is not payload
		// curlUploadFormData->payloadBytesSent +=
		// curlUploadFormData->formData.size();

		// logger->info(__FILEREF__ + "First read"
		// 	+ ", curlUploadFormData->formData.size(): " +
		// to_string(curlUploadFormData->formData.size())
		// 	+ ", curlUploadFormData->payloadBytesSent: " +
		// to_string(curlUploadFormData->payloadBytesSent)
		// );

		return curlUploadFormData->formData.size();
	}
	else if (currentFilePosition == curlUploadFormData->upToByte_Excluded)
	{
		if (!curlUploadFormData->endOfFormDataSent)
		{
			if (curlUploadFormData->endOfFormData.size() > size * nmemb)
			{
				SPDLOG_ERROR(
					"Not enougth memory!!!"
					", curlUploadFormData->formDataSent: {}"
					", curlUploadFormData->formData: {}"
					", curlUploadFormData->endOfFormDataSent: {}"
					", curlUploadFormData->endOfFormData: {}"
					", curlUploadFormData->upToByte_Excluded: {}"
					", curlUploadFormData->endOfFormData.size(): {}"
					", size * nmemb: {}",
					curlUploadFormData->formDataSent, curlUploadFormData->formData, curlUploadFormData->endOfFormDataSent,
					curlUploadFormData->endOfFormData, curlUploadFormData->upToByte_Excluded, curlUploadFormData->endOfFormData.size(), size * nmemb
				);

				return CURL_READFUNC_ABORT;
			}

			strcpy(ptr, curlUploadFormData->endOfFormData.c_str());

			curlUploadFormData->endOfFormDataSent = true;

			// this is not payload
			// curlUploadFormData->payloadBytesSent +=
			// curlUploadFormData->endOfFormData.size();

			// logger->info(__FILEREF__ + "Last read"
			// 	+ ", curlUploadFormData->endOfFormData.size(): " +
			// to_string(curlUploadFormData->endOfFormData.size())
			// 	+ ", curlUploadFormData->payloadBytesSent: " +
			// to_string(curlUploadFormData->payloadBytesSent)
			// );

			return curlUploadFormData->endOfFormData.size();
		}
		else
		{
			SPDLOG_ERROR(
				"This scenario should never happen"
				", curlUploadFormData->formDataSent: {}"
				", curlUploadFormData->formData: {}"
				", curlUploadFormData->endOfFormDataSent: {}"
				", curlUploadFormData->endOfFormData: {}"
				", curlUploadFormData->upToByte_Excluded: {}"
				", curlUploadFormData->endOfFormData.size(): {}",
				curlUploadFormData->formDataSent, curlUploadFormData->formData, curlUploadFormData->endOfFormDataSent,
				curlUploadFormData->endOfFormData, curlUploadFormData->upToByte_Excluded, curlUploadFormData->endOfFormData.size()
			);

			return CURL_READFUNC_ABORT;
		}
	}

	if (currentFilePosition + (size * nmemb) <= curlUploadFormData->upToByte_Excluded)
		curlUploadFormData->mediaSourceFileStream.read(ptr, size * nmemb);
	else
		curlUploadFormData->mediaSourceFileStream.read(ptr, curlUploadFormData->upToByte_Excluded - currentFilePosition);

	int64_t charsRead = curlUploadFormData->mediaSourceFileStream.gcount();

	curlUploadFormData->payloadBytesSent += charsRead;

	// logger->info(__FILEREF__ + "curlUploadFormDataCallback"
	//     + ", currentFilePosition: " + to_string(currentFilePosition)
	//     + ", charsRead: " + to_string(charsRead)
	// 	+ ", curlUploadFormData->payloadBytesSent: " +
	// to_string(curlUploadFormData->payloadBytesSent)
	// );

	// Docs: Returning 0 will signal end-of-file to the library and cause it to
	// stop the current transfer
	return charsRead;
};

/*
string MMSCURL::httpGet(
	shared_ptr<spdlog::logger> logger, int64_t ingestionJobKey, string url, long timeoutInSeconds, string basicAuthenticationUser,
	string basicAuthenticationPassword, vector<string> otherHeaders, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string sResponse;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		ostringstream response;
		bool responseInitialized = false;
		try
		{
			curlpp::Cleanup cleaner;
			curlpp::Easy request;

			list<string> headers;
			if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
			{
				string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
				string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

				headers.push_back(basicAuthorization);
			}
			headers.insert(headers.end(), otherHeaders.begin(), otherHeaders.end());

			request.setOpt(new curlpp::options::Url(url));

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() &&
			// 	0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{

// disconnect if we can't validate server's cert
bool bSslVerifyPeer = false;
curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
request.setOpt(sslVerifyPeer);

curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
request.setOpt(sslVerifyHost);

// request.setOpt(new curlpp::options::SslEngineDefault());
}

request.setOpt(new curlpp::options::HttpHeader(headers));

request.setOpt(new curlpp::options::WriteStream(&response));

logger->info(__FILEREF__ + "HTTP GET" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url);

// store response headers in the response
// You simply have to set next option to prefix the header to the
// normal body output. request.setOpt(new
// curlpp::options::Header(true));

responseInitialized = true;
chrono::system_clock::time_point start = chrono::system_clock::now();
request.perform();
chrono::system_clock::time_point end = chrono::system_clock::now();

sResponse = response.str();
// LF and CR create problems to the json parser...
while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
	sResponse.pop_back();

long responseCode = curlpp::infos::ResponseCode::get(request);
if (responseCode == 200)
{
	string message = __FILEREF__ + "httpGet" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", @MMS statistics@ - elapsed (secs): @" +
					 to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@" +
					 ", sResponse: " + regex_replace(sResponse, regex("\n"), " ");
	logger->info(message);
}
else
{
	string message = __FILEREF__ + "httpGet failed, wrong return status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					 ", @MMS statistics@ - elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@" +
					 ", sResponse: " + sResponse + ", responseCode: " + to_string(responseCode);
	logger->error(message);

	throw runtime_error(message);
}

// return sResponse;
break;
}
catch (curlpp::LogicError &e)
{
	logger->error(
		__FILEREF__ + "httpGet failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
		", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
	);

	if (retryNumber < maxRetryNumber)
	{
		logger->info(
			__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
			", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
		);
		this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
	}
	else
		throw runtime_error(e.what());
}
catch (curlpp::RuntimeError &e)
{
	SPDLOG_ERROR(
		"httpGet failed (RuntimeError)"
		", ingestionJobKey: {}"
		", url: {}"
		", exception: {}"
		", response.str(): {}",
		ingestionJobKey, url, e.what(), responseInitialized ? response.str() : ""
	);

	if (retryNumber < maxRetryNumber)
	{
		logger->info(
			__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
			", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
		);
		this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
	}
	else
		throw runtime_error(e.what());
}
catch (runtime_error e)
{
	if (responseInitialized && response.str().find("502 Bad Gateway") != string::npos)
	{
		logger->error(
			__FILEREF__ + "Server is not reachable, is it down?" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
			", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		if (retryNumber < maxRetryNumber)
		{
			logger->info(
				__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
				", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
			);
			this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
		}
		else
			throw ServerNotReachable();
	}
	else
	{
		logger->error(
			__FILEREF__ + "httpGet failed (exception)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
			", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		if (retryNumber < maxRetryNumber)
		{
			logger->info(
				__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
				", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
			);
			this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
		}
		else
			throw e;
	}
}
catch (exception e)
{
	logger->error(
		__FILEREF__ + "httpGet failed (exception)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
		", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
	);

	if (retryNumber < maxRetryNumber)
	{
		logger->info(
			__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
			", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
		);
		this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
	}
	else
		throw e;
}
}

return sResponse;
}
*/

size_t curlWriteResponseCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
	try
	{
		string *response = (string *)f;

		response->append(ptr, size * nmemb);

		return size * nmemb;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"curlWriteResponseCallback failed"
			", size: {}"
			", nmemb: {}"
			", ptr: {}"
			", exception: {}",
			size, nmemb, ptr, e.what()
		);
		// Docs: Returning 0 will signal end-of-file to the library and cause it to
		// stop the current transfer
		return 0;
	}
};

string CurlWrapper::httpGet(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, vector<string> otherHeaders,
	string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string api = "httpGet";

	string response;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		CURL *curl = nullptr;
		struct curl_slist *headersList = nullptr;

		try
		{
			// curlpp::Cleanup cleaner;
			// curlpp::Easy request;

			curl = curl_easy_init();
			if (!curl)
			{
				string errorMessage = fmt::format("{}. curl_easy_init failed", api);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// request.setOpt(new curlpp::options::Url(url));
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			// request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutInSeconds);

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() &&
			// 	0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				/*
				typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD>
				SslCertPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEY> SslKey; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEYTYPE> SslKeyType; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD>
				SslKeyPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLENGINE> SslEngine; typedef
				curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
				SslEngineDefault; typedef curlpp::OptionTrait<long,
				CURLOPT_SSLVERSION> SslVersion; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath; typedef
				curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE>
				RandomFile; typedef curlpp::OptionTrait<std::string,
				CURLOPT_EGDSOCKET> EgdSocket; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST>
				SslCipherList; typedef curlpp::OptionTrait<std::string,
				CURLOPT_KRB4LEVEL> Krb4Level;
				*/

				/*
				// cert is stored PEM coded in file...
				// since PEM is default, we needn't set it for PEM
				// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
				curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE>
				sslCertType("PEM"); equest.setOpt(sslCertType);

				// set the cert for client authentication
				// "testcert.pem"
				// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
				curlpp::OptionTrait<string, CURLOPT_SSLCERT>
				sslCert("cert.pem"); request.setOpt(sslCert);
				*/

				/*
				// sorry, for engine we must set the passphrase
				//   (if the key has one...)
				// const char *pPassphrase = NULL;
				if(pPassphrase)
					curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

				// if we use a key stored in a crypto engine,
				//   we must set the key type to "ENG"
				// pKeyType  = "PEM";
				curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

				// set the private key (file or ID in engine)
				// pKeyName  = "testkey.pem";
				curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

				// set the file with the certs vaildating the server
				// *pCACertFile = "cacert.pem";
				curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
				*/

				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				// request.setOpt(sslVerifyPeer);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				// request.setOpt(sslVerifyHost);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

				// request.setOpt(new curlpp::options::SslEngineDefault());
			}

			/*
			list<string> headers;
			if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
			{
				string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
				string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

				headers.push_back(basicAuthorization);
			}
			headers.insert(headers.end(), otherHeaders.begin(), otherHeaders.end());
			request.setOpt(new curlpp::options::HttpHeader(headers));
			*/
			{
				if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
				{
					string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
					string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

					headersList = curl_slist_append(headersList, basicAuthorization.c_str());
				}

				for (string header : otherHeaders)
					headersList = curl_slist_append(headersList, header.c_str());

				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
			}

			// request.setOpt(new curlpp::options::WriteStream(&response));
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteResponseCallback);

			SPDLOG_INFO(
				"{} details"
				"{}"
				", url: {}"
				", basicAuthenticationUser: {}"
				", otherHeaders.size: {}",
				api, referenceToLog, url, basicAuthenticationUser, otherHeaders.size()
			);

			// store response headers in the response
			// You simply have to set next option to prefix the header to the
			// normal body output. request.setOpt(new
			// curlpp::options::Header(true));

			chrono::system_clock::time_point start = chrono::system_clock::now();
			// request.perform();
			CURLcode curlCode = curl_easy_perform(curl);
			chrono::system_clock::time_point end = chrono::system_clock::now();
			if (curlCode != CURLE_OK)
			{
				string errorMessage = fmt::format(
					"{}. curl_easy_perform failed"
					", curlCode message: {}"
					", url: {}",
					api, curl_easy_strerror(curlCode), url
				);
				SPDLOG_ERROR(errorMessage);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				throw runtime_error(errorMessage);
			}

			// sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (response.size() > 0 && (response.back() == 10 || response.back() == 13))
				response.pop_back();

			// long responseCode = curlpp::infos::ResponseCode::get(request);
			long responseCode;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
			if (responseCode == 200)
			{
				SPDLOG_INFO(
					"{} success"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", response: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " ")
				);
			}
			else
			{
				string message = fmt::format(
					"{} failed, wrong return status"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", response: {}"
					", responseCode: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " "),
					responseCode
				);
				SPDLOG_ERROR(message);

				throw runtime_error(message);
			}

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			// return sResponse;
			break;
		}
		catch (runtime_error e)
		{
			if (response.find("502 Bad Gateway") != string::npos)
			{
				SPDLOG_ERROR(
					"{}. Server is not reachable, is it down?"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed (exception)"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", url: {}"
				", exception: {}"
				", response.str(): {}",
				api, referenceToLog, url, e.what(), response
			);

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return response;
}

/*
string MMSCURL::httpDelete(
	shared_ptr<spdlog::logger> logger, int64_t ingestionJobKey, string url, long timeoutInSeconds, string basicAuthenticationUser,
	string basicAuthenticationPassword, vector<string> otherHeaders, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string sResponse;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		ostringstream response;
		bool responseInitialized = false;
		try
		{
			curlpp::Cleanup cleaner;
			curlpp::Easy request;

			list<string> headers;
			if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
			{
				string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
				string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

				headers.push_back(basicAuthorization);
			}
			headers.insert(headers.end(), otherHeaders.begin(), otherHeaders.end());

			request.setOpt(new curlpp::options::Url(url));
			request.setOpt(new curlpp::options::CustomRequest("DELETE"));

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() && 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				request.setOpt(sslVerifyPeer);

				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				request.setOpt(sslVerifyHost);

				// request.setOpt(new curlpp::options::SslEngineDefault());
			}

			request.setOpt(new curlpp::options::HttpHeader(headers));

			request.setOpt(new curlpp::options::WriteStream(&response));

			logger->info(__FILEREF__ + "HTTP GET" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url);

			// store response headers in the response
			// You simply have to set next option to prefix the header to the
			// normal body output. request.setOpt(new
			// curlpp::options::Header(true));

			responseInitialized = true;
			chrono::system_clock::time_point start = chrono::system_clock::now();
			request.perform();
			chrono::system_clock::time_point end = chrono::system_clock::now();

			sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
				sResponse.pop_back();

			long responseCode = curlpp::infos::ResponseCode::get(request);
			if (responseCode == 200)
			{
				string message = __FILEREF__ + "httpDelete" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", @MMS statistics@ - elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) +
								 "@" + ", sResponse: " + sResponse;
				logger->info(message);
			}
			else
			{
				string message = __FILEREF__ + "httpDelete failed, wrong return status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", @MMS statistics@ - elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) +
								 "@" + ", sResponse: " + sResponse + ", responseCode: " + to_string(responseCode);
				logger->error(message);

				throw runtime_error(message);
			}

			// return sResponse;
			break;
		}
		catch (curlpp::LogicError &e)
		{
			logger->error(
				__FILEREF__ + "httpDelete failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (curlpp::RuntimeError &e)
		{
			logger->error(
				__FILEREF__ + "httpDelete failed (RuntimeError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (runtime_error e)
		{
			if (responseInitialized && response.str().find("502 Bad Gateway") != string::npos)
			{
				logger->error(
					__FILEREF__ + "Server is not reachable, is it down?" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
					", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				logger->error(
					__FILEREF__ + "httpDelete failed (exception)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
					", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			logger->error(
				__FILEREF__ + "httpDelete failed (exception)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return sResponse;
}
*/

string CurlWrapper::httpDelete(
	string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, vector<string> otherHeaders,
	string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string api = "httpDelete";

	string response;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		CURL *curl = nullptr;
		struct curl_slist *headersList = nullptr;

		try
		{
			// curlpp::Cleanup cleaner;
			// curlpp::Easy request;

			curl = curl_easy_init();
			if (!curl)
			{
				string errorMessage = fmt::format("{}. curl_easy_init failed", api);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// request.setOpt(new curlpp::options::Url(url));
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			// request.setOpt(new curlpp::options::CustomRequest("DELETE"));
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			// request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutInSeconds);

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() &&
			// 	0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				/*
				typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD>
				SslCertPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEY> SslKey; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEYTYPE> SslKeyType; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD>
				SslKeyPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLENGINE> SslEngine; typedef
				curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
				SslEngineDefault; typedef curlpp::OptionTrait<long,
				CURLOPT_SSLVERSION> SslVersion; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath; typedef
				curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE>
				RandomFile; typedef curlpp::OptionTrait<std::string,
				CURLOPT_EGDSOCKET> EgdSocket; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST>
				SslCipherList; typedef curlpp::OptionTrait<std::string,
				CURLOPT_KRB4LEVEL> Krb4Level;
				*/

				/*
				// cert is stored PEM coded in file...
				// since PEM is default, we needn't set it for PEM
				// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
				curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE>
				sslCertType("PEM"); equest.setOpt(sslCertType);

				// set the cert for client authentication
				// "testcert.pem"
				// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
				curlpp::OptionTrait<string, CURLOPT_SSLCERT>
				sslCert("cert.pem"); request.setOpt(sslCert);
				*/

				/*
				// sorry, for engine we must set the passphrase
				//   (if the key has one...)
				// const char *pPassphrase = NULL;
				if(pPassphrase)
					curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

				// if we use a key stored in a crypto engine,
				//   we must set the key type to "ENG"
				// pKeyType  = "PEM";
				curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

				// set the private key (file or ID in engine)
				// pKeyName  = "testkey.pem";
				curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

				// set the file with the certs vaildating the server
				// *pCACertFile = "cacert.pem";
				curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
				*/

				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				// request.setOpt(sslVerifyPeer);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				// request.setOpt(sslVerifyHost);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

				// request.setOpt(new curlpp::options::SslEngineDefault());
			}

			/*
			list<string> headers;
			if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
			{
				string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
				string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

				headers.push_back(basicAuthorization);
			}
			headers.insert(headers.end(), otherHeaders.begin(), otherHeaders.end());
			request.setOpt(new curlpp::options::HttpHeader(headers));
			*/
			{
				if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
				{
					string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
					string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

					headersList = curl_slist_append(headersList, basicAuthorization.c_str());
				}

				for (string header : otherHeaders)
					headersList = curl_slist_append(headersList, header.c_str());

				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
			}

			// request.setOpt(new curlpp::options::WriteStream(&response));
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteResponseCallback);

			SPDLOG_INFO(
				"{} details"
				"{}"
				", url: {}"
				", basicAuthenticationUser: {}"
				", otherHeaders.size: {}",
				api, referenceToLog, url, basicAuthenticationUser, otherHeaders.size()
			);

			// store response headers in the response
			// You simply have to set next option to prefix the header to the
			// normal body output. request.setOpt(new
			// curlpp::options::Header(true));

			chrono::system_clock::time_point start = chrono::system_clock::now();
			// request.perform();
			CURLcode curlCode = curl_easy_perform(curl);
			chrono::system_clock::time_point end = chrono::system_clock::now();
			if (curlCode != CURLE_OK)
			{
				string errorMessage = fmt::format(
					"{}. curl_easy_perform failed"
					", curlCode message: {}"
					", url: {}",
					api, curl_easy_strerror(curlCode), url
				);
				SPDLOG_ERROR(errorMessage);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				throw runtime_error(errorMessage);
			}

			// sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (response.size() > 0 && (response.back() == 10 || response.back() == 13))
				response.pop_back();

			// long responseCode = curlpp::infos::ResponseCode::get(request);
			long responseCode;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
			if (responseCode == 200)
			{
				SPDLOG_INFO(
					"{} success"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", response: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " ")
				);
			}
			else
			{
				string message = fmt::format(
					"{} failed, wrong return status"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", response: {}"
					", responseCode: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " "),
					responseCode
				);
				SPDLOG_ERROR(message);

				throw runtime_error(message);
			}

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			// return sResponse;
			break;
		}
		catch (runtime_error e)
		{
			if (response.find("502 Bad Gateway") != string::npos)
			{
				SPDLOG_ERROR(
					"{}. Server is not reachable, is it down?"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed (exception)"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", url: {}"
				", exception: {}"
				", response.str(): {}",
				api, referenceToLog, url, e.what(), response
			);

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return response;
}

/*
pair<string, string> MMSCURL::httpPostPutString(
	shared_ptr<spdlog::logger> logger, int64_t ingestionJobKey, string url,
	string requestType, // POST or PUT
	long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string sHeaderResponse;
	string sBodyResponse;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		ostringstream response;
		bool responseInitialized = false;
		try
		{
			curlpp::Cleanup cleaner;
			curlpp::Easy request;

			list<string> headers;
			if (contentType != "")
				headers.push_back(string("Content-Type: ") + contentType);
			if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
			{
				// string userPasswordEncoded =
				// Convert::base64_encode(_mmsAPIUser + ":" + _mmsAPIPassword);
				string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
				string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

				headers.push_back(basicAuthorization);
			}
			headers.insert(headers.end(), otherHeaders.begin(), otherHeaders.end());

			request.setOpt(new curlpp::options::Url(url));

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() && 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				bool bSslVerifyPeer = false;
				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				request.setOpt(sslVerifyPeer);

				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				request.setOpt(sslVerifyHost);
			}

			request.setOpt(new curlpp::options::HttpHeader(headers));

			request.setOpt(new curlpp::options::CustomRequest(requestType));
			request.setOpt(new curlpp::options::PostFields(body));
			request.setOpt(new curlpp::options::PostFieldSize(body.length()));

			request.setOpt(new curlpp::options::WriteStream(&response));

			logger->info(
				__FILEREF__ + "httpPostPutString (" + requestType + ")" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", contentType: " + contentType + ", body: " + regex_replace(body, regex("\n"), " ")
			);

			// store response headers in the response
			// You simply have to set next option to prefix the header to the
			// normal body output.
			request.setOpt(new curlpp::options::Header(true));

			responseInitialized = true;
			chrono::system_clock::time_point start = chrono::system_clock::now();
			request.perform();
			chrono::system_clock::time_point end = chrono::system_clock::now();

			string sHeaderAndBodyResponse = response.str();

			long responseCode = curlpp::infos::ResponseCode::get(request);
			if (responseCode != 200 && responseCode != 201)
			{
				string message = __FILEREF__ + "httpPostPutString failed, wrong return status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", url: " + url + ", @MMS statistics@ - elapsed (secs): @" +
								 to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@" +
								 ", response.str(): " + regex_replace(response.str(), regex("\n"), " ") +
								 ", responseCode: " + to_string(responseCode);
				logger->error(message);

				throw runtime_error(message);
			}

			{
				string message = __FILEREF__ + "httpPostPutString success" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", @MMS statistics@ - elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) +
								 "@" + ", sHeaderAndBodyResponse: " + sHeaderAndBodyResponse;
				logger->info(message);
			}

			// 2023-01-09: eventuali HTTP/1.1 100 Continue\r\n\r\n vengono
			// scartati
			string prefix("HTTP/1.1 100 Continue\r\n\r\n");
			// while (sHeaderAndBodyResponse.size() >= prefix.size() && 0 == sHeaderAndBodyResponse.compare(0, prefix.size(), prefix))
			while (sHeaderAndBodyResponse.starts_with(prefix))
			{
				sHeaderAndBodyResponse = sHeaderAndBodyResponse.substr(prefix.size());
			}

			size_t beginOfHeaderBodySeparatorIndex;
			if ((beginOfHeaderBodySeparatorIndex = sHeaderAndBodyResponse.find("\r\n\r\n")) == string::npos)
			{
				string errorMessage = __FILEREF__ + "response is wrong" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
									  ", sHeaderAndBodyResponse: " + sHeaderAndBodyResponse;
				logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sHeaderResponse = sHeaderAndBodyResponse.substr(0, beginOfHeaderBodySeparatorIndex);
			sBodyResponse = sHeaderAndBodyResponse.substr(beginOfHeaderBodySeparatorIndex + 4);

			// LF and CR create problems to the json parser...
			while (sBodyResponse.size() > 0 && (sBodyResponse.back() == 10 || sBodyResponse.back() == 13))
				sBodyResponse.pop_back();

			{
				string message = __FILEREF__ + "httpPostPutString success test" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", sHeaderResponse: " + regex_replace(sHeaderResponse, regex("\n"), " ") +
								 ", sBodyResponse: " + regex_replace(sBodyResponse, regex("\n"), " ");
				logger->info(message);
			}

			break;
		}
		catch (curlpp::LogicError &e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutString failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (curlpp::RuntimeError &e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutString failed (RuntimeError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (runtime_error e)
		{
			if (responseInitialized && response.str().find("502 Bad Gateway") != string::npos)
			{
				logger->error(__FILEREF__ + "Server is not reachable, is it down?" + ", exception: " + e.what());

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				logger->error(__FILEREF__ + "httpPostPutString failed" + ", exception: " + e.what());

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutString failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return make_pair(sHeaderResponse, sBodyResponse);
}
*/

pair<string, string> CurlWrapper::httpPostPutString(
	string url,
	string requestType, // POST or PUT
	long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string api = "httpPostPutString";

	string responseHeaderAndBody;
	string responseHeader;
	string responseBody;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		CURL *curl = nullptr;
		struct curl_slist *headersList = nullptr;

		try
		{
			// curlpp::Cleanup cleaner;
			// curlpp::Easy request;

			curl = curl_easy_init();
			if (!curl)
			{
				string errorMessage = fmt::format("{}. curl_easy_init failed", api);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// request.setOpt(new curlpp::options::Url(url));
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			// request.setOpt(new curlpp::options::CustomRequest(requestType));
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, requestType.c_str());

			// request.setOpt(new curlpp::options::PostFields(body));
			// request.setOpt(new curlpp::options::PostFieldSize(body.length()));
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			// request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutInSeconds);

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() &&
			// 	0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				/*
				typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD>
				SslCertPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEY> SslKey; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEYTYPE> SslKeyType; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD>
				SslKeyPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLENGINE> SslEngine; typedef
				curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
				SslEngineDefault; typedef curlpp::OptionTrait<long,
				CURLOPT_SSLVERSION> SslVersion; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath; typedef
				curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE>
				RandomFile; typedef curlpp::OptionTrait<std::string,
				CURLOPT_EGDSOCKET> EgdSocket; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST>
				SslCipherList; typedef curlpp::OptionTrait<std::string,
				CURLOPT_KRB4LEVEL> Krb4Level;
				*/

				/*
				// cert is stored PEM coded in file...
				// since PEM is default, we needn't set it for PEM
				// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
				curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE>
				sslCertType("PEM"); equest.setOpt(sslCertType);

				// set the cert for client authentication
				// "testcert.pem"
				// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
				curlpp::OptionTrait<string, CURLOPT_SSLCERT>
				sslCert("cert.pem"); request.setOpt(sslCert);
				*/

				/*
				// sorry, for engine we must set the passphrase
				//   (if the key has one...)
				// const char *pPassphrase = NULL;
				if(pPassphrase)
					curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

				// if we use a key stored in a crypto engine,
				//   we must set the key type to "ENG"
				// pKeyType  = "PEM";
				curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

				// set the private key (file or ID in engine)
				// pKeyName  = "testkey.pem";
				curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

				// set the file with the certs vaildating the server
				// *pCACertFile = "cacert.pem";
				curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
				*/

				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				// request.setOpt(sslVerifyPeer);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				// request.setOpt(sslVerifyHost);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

				// request.setOpt(new curlpp::options::SslEngineDefault());
			}

			/*
			list<string> headers;
			if (contentType != "")
				headers.push_back(string("Content-Type: ") + contentType);
			if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
			{
				string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
				string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

				headers.push_back(basicAuthorization);
			}
			headers.insert(headers.end(), otherHeaders.begin(), otherHeaders.end());
			request.setOpt(new curlpp::options::HttpHeader(headers));
			*/
			{
				// if (contentType != "")
				// headers.push_back(string("Content-Type: ") + contentType);
				if (contentType != "")
					headersList = curl_slist_append(headersList, fmt::format("Content-Type: {}", contentType).c_str());
				if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
				{
					string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
					string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

					headersList = curl_slist_append(headersList, basicAuthorization.c_str());
				}

				for (string header : otherHeaders)
					headersList = curl_slist_append(headersList, header.c_str());

				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
			}

			// request.setOpt(new curlpp::options::WriteStream(&response));
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&responseHeaderAndBody);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteResponseCallback);

			// store response headers in the response, You simply have to set next option to prefix the header to the
			// normal body output.
			// request.setOpt(new curlpp::options::Header(true));
			curl_easy_setopt(curl, CURLOPT_HEADER, 1L);

			SPDLOG_INFO(
				"{} details"
				"{}"
				", url: {}"
				", contentType: {}"
				", basicAuthenticationUser: {}"
				", otherHeaders.size: {}"
				", body: {}",
				api, referenceToLog, url, contentType, basicAuthenticationUser, otherHeaders.size(), regex_replace(body, regex("\n"), " ")
			);

			chrono::system_clock::time_point start = chrono::system_clock::now();
			// request.perform();
			CURLcode curlCode = curl_easy_perform(curl);
			chrono::system_clock::time_point end = chrono::system_clock::now();
			if (curlCode != CURLE_OK)
			{
				string errorMessage = fmt::format(
					"{}. curl_easy_perform failed"
					", curlCode message: {}"
					", url: {}",
					api, curl_easy_strerror(curlCode), url
				);
				SPDLOG_ERROR(errorMessage);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				throw runtime_error(errorMessage);
			}

			// long responseCode = curlpp::infos::ResponseCode::get(request);
			long responseCode;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
			if (responseCode == 200 || responseCode == 201)
			{
				SPDLOG_INFO(
					"{} success"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", responseHeaderAndBody: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), responseHeaderAndBody
				);
			}
			else
			{
				string message = fmt::format(
					"{} failed, wrong return status"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", responseHeaderAndBody: {}"
					", responseCode: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), responseHeaderAndBody, responseCode
				);
				SPDLOG_ERROR(message);

				throw runtime_error(message);
			}

			// 2023-01-09: eventuali HTTP/1.1 100 Continue\r\n\r\n vengono scartate
			string prefix("HTTP/1.1 100 Continue\r\n\r\n");
			while (responseHeaderAndBody.starts_with(prefix))
				responseHeaderAndBody = responseHeaderAndBody.substr(prefix.size());

			size_t beginOfHeaderBodySeparatorIndex;
			if ((beginOfHeaderBodySeparatorIndex = responseHeaderAndBody.find("\r\n\r\n")) == string::npos)
			{
				string errorMessage = fmt::format(
					"response is wrong"
					"{}"
					", url: {}"
					", responseHeaderAndBody: {}",
					referenceToLog, url, responseHeaderAndBody
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			responseHeader = responseHeaderAndBody.substr(0, beginOfHeaderBodySeparatorIndex);
			responseBody = responseHeaderAndBody.substr(beginOfHeaderBodySeparatorIndex + 4);

			// sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (responseBody.size() > 0 && (responseBody.back() == 10 || responseBody.back() == 13))
				responseBody.pop_back();

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			// return sResponse;
			break;
		}
		catch (runtime_error e)
		{
			if (responseHeaderAndBody.find("502 Bad Gateway") != string::npos)
			{
				SPDLOG_ERROR(
					"{}. Server is not reachable, is it down?"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), responseHeaderAndBody
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed (exception)"
					"{}"
					", url: {}"
					", exception: {}"
					", responseHeaderAndBody: {}",
					api, referenceToLog, url, e.what(), responseHeaderAndBody
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", url: {}"
				", exception: {}"
				", responseHeaderAndBody: {}",
				api, referenceToLog, url, e.what(), responseHeaderAndBody
			);

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return make_pair(responseHeader, responseBody);
}

/*
string MMSCURL::httpPostPutFile(
	shared_ptr<spdlog::logger> logger, int64_t ingestionJobKey, string url,
	string requestType, // POST or PUT
	long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName, int64_t fileSizeInBytes,
	int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string sResponse;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		ostringstream response;
		bool responseInitialized = false;
		try
		{
			CurlUploadData curlUploadData;
			curlUploadData.loggerName = logger->name();
			curlUploadData.mediaSourceFileStream.open(pathFileName, ios::binary);
			if (!curlUploadData.mediaSourceFileStream)
			{
				string message =
					__FILEREF__ + "open file failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", pathFileName: " + pathFileName;
				logger->error(message);

				throw runtime_error(message);
			}
			if (contentRangeStart > 0)
				curlUploadData.mediaSourceFileStream.seekg(contentRangeStart, ios::beg);
			curlUploadData.payloadBytesSent = 0;
			if (contentRangeEnd_Excluded > 0)
				curlUploadData.upToByte_Excluded = contentRangeEnd_Excluded;
			else
				curlUploadData.upToByte_Excluded = fileSizeInBytes;

			curlpp::Cleanup cleaner;
			curlpp::Easy request;

			{
				curlpp::options::ReadFunctionCurlFunction curlUploadCallbackFunction(curlUploadCallback);
				curlpp::OptionTrait<void *, CURLOPT_READDATA> curlUploadDataData(&curlUploadData);
				request.setOpt(curlUploadCallbackFunction);
				request.setOpt(curlUploadDataData);

				bool upload = true;
				request.setOpt(new curlpp::options::Upload(upload));
			}

			list<string> header;

			string contentLengthOrRangeHeader;
			if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
			{
				// Content-Range: bytes
				// $contentRangeStart-$contentRangeEnd/$binaryFileSize

				contentLengthOrRangeHeader = string("Content-Range: bytes ") + to_string(contentRangeStart) + "-" +
											 to_string(contentRangeEnd_Excluded - 1) + "/" + to_string(fileSizeInBytes);
			}
			else
			{
				contentLengthOrRangeHeader = string("Content-Length: ") + to_string(fileSizeInBytes);
			}
			header.push_back(contentLengthOrRangeHeader);

			{
				// string userPasswordEncoded =
				// Convert::base64_encode(_mmsAPIUser + ":" + _mmsAPIPassword);
				string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
				string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

				header.push_back(basicAuthorization);
			}

			request.setOpt(new curlpp::options::CustomRequest(requestType));
			if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
				request.setOpt(new curlpp::options::PostFieldSizeLarge(contentRangeEnd_Excluded - contentRangeStart));
			else
				request.setOpt(new curlpp::options::PostFieldSizeLarge(fileSizeInBytes));

			// Setting the URL to retrive.
			request.setOpt(new curlpp::options::Url(url));

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() && 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				bool bSslVerifyPeer = false;
				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				request.setOpt(sslVerifyPeer);

				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				request.setOpt(sslVerifyHost);
			}

			request.setOpt(new curlpp::options::HttpHeader(header));

			request.setOpt(new curlpp::options::WriteStream(&response));

			logger->info(
				__FILEREF__ + "httpPostPutFile (" + requestType + ")" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", contentLengthOrRangeHeader: " + contentLengthOrRangeHeader + ", timeoutInSeconds: " + to_string(timeoutInSeconds) +
				", pathFileName: " + pathFileName
			);

			// store response headers in the response
			// You simply have to set next option to prefix the header to the
			// normal body output. request.setOpt(new
			// curlpp::options::Header(true));

			responseInitialized = true;
			chrono::system_clock::time_point start = chrono::system_clock::now();
			request.perform();
			chrono::system_clock::time_point end = chrono::system_clock::now();

			(curlUploadData.mediaSourceFileStream).close();

			sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
				sResponse.pop_back();

			long responseCode = curlpp::infos::ResponseCode::get(request);
			if (responseCode == 201)
			{
				string message = __FILEREF__ + "httpPostPutFile success" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", responseCode: " + to_string(responseCode) + ", @MMS statistics@ - elapsed (secs): @" +
								 to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@" + ", sResponse: " + sResponse;
				logger->info(message);
			}
			else
			{
				string message = __FILEREF__ + "httpPostPutFile failed, wrong return status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", @MMS statistics@ - elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) +
								 "@" + ", sResponse: " + sResponse + ", responseCode: " + to_string(responseCode);
				logger->error(message);

				throw runtime_error(message);
			}

			// return sResponse;
			break;
		}
		catch (curlpp::LogicError &e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutFile failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " +
				requestType + ", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (curlpp::RuntimeError &e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutFile failed (RuntimeError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " +
				requestType + ", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (runtime_error e)
		{
			if (responseInitialized && response.str().find("502 Bad Gateway") != string::npos)
			{
				logger->error(
					__FILEREF__ + "Server is not reachable, is it down?" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " +
					requestType + ", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				logger->error(
					__FILEREF__ + "httpPostPutFile failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " + requestType +
					", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutFile failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " + requestType +
				", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return sResponse;
}
*/

string CurlWrapper::httpPostPutFile(
	string url,
	string requestType, // POST or PUT
	long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName, int64_t fileSizeInBytes,
	string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string api = "httpPostPutFile";

	string response;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		CURL *curl = nullptr;
		struct curl_slist *headersList = nullptr;

		try
		{
			CurlUploadData curlUploadData;
			curlUploadData.mediaSourceFileStream.open(pathFileName, ios::binary);
			if (!curlUploadData.mediaSourceFileStream)
			{
				string message = fmt::format(
					"open file failed"
					"{}"
					", pathFileName: {}",
					referenceToLog, pathFileName
				);
				SPDLOG_ERROR(message);

				throw runtime_error(message);
			}
			if (contentRangeStart > 0)
				curlUploadData.mediaSourceFileStream.seekg(contentRangeStart, ios::beg);
			curlUploadData.payloadBytesSent = 0;
			if (contentRangeEnd_Excluded > 0)
				curlUploadData.upToByte_Excluded = contentRangeEnd_Excluded;
			else
				curlUploadData.upToByte_Excluded = fileSizeInBytes;

			// curlpp::Cleanup cleaner;
			// curlpp::Easy request;

			curl = curl_easy_init();
			if (!curl)
			{
				string errorMessage = fmt::format("{}. curl_easy_init failed", api);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			{
				// curlpp::options::ReadFunctionCurlFunction curlUploadCallbackFunction(curlUploadCallback);
				// curlpp::OptionTrait<void *, CURLOPT_READDATA> curlUploadDataData(&curlUploadData);
				// request.setOpt(curlUploadCallbackFunction);
				// request.setOpt(curlUploadDataData);
				curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&curlUploadData);
				curl_easy_setopt(curl, CURLOPT_READFUNCTION, curlUploadCallback);

				// bool upload = true;
				// request.setOpt(new curlpp::options::Upload(upload));
				curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
			}

			// request.setOpt(new curlpp::options::Url(url));
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			// request.setOpt(new curlpp::options::CustomRequest(requestType));
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, requestType.c_str());

			/*
			if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
				request.setOpt(new curlpp::options::PostFieldSizeLarge(contentRangeEnd_Excluded - contentRangeStart));
			else
				request.setOpt(new curlpp::options::PostFieldSizeLarge(fileSizeInBytes));
			*/
			if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, contentRangeEnd_Excluded - contentRangeStart);
			else
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, fileSizeInBytes);

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			// request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutInSeconds);

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() &&
			// 	0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				/*
				typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD>
				SslCertPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEY> SslKey; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEYTYPE> SslKeyType; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD>
				SslKeyPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLENGINE> SslEngine; typedef
				curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
				SslEngineDefault; typedef curlpp::OptionTrait<long,
				CURLOPT_SSLVERSION> SslVersion; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath; typedef
				curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE>
				RandomFile; typedef curlpp::OptionTrait<std::string,
				CURLOPT_EGDSOCKET> EgdSocket; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST>
				SslCipherList; typedef curlpp::OptionTrait<std::string,
				CURLOPT_KRB4LEVEL> Krb4Level;
				*/

				/*
				// cert is stored PEM coded in file...
				// since PEM is default, we needn't set it for PEM
				// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
				curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE>
				sslCertType("PEM"); equest.setOpt(sslCertType);

				// set the cert for client authentication
				// "testcert.pem"
				// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
				curlpp::OptionTrait<string, CURLOPT_SSLCERT>
				sslCert("cert.pem"); request.setOpt(sslCert);
				*/

				/*
				// sorry, for engine we must set the passphrase
				//   (if the key has one...)
				// const char *pPassphrase = NULL;
				if(pPassphrase)
					curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

				// if we use a key stored in a crypto engine,
				//   we must set the key type to "ENG"
				// pKeyType  = "PEM";
				curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

				// set the private key (file or ID in engine)
				// pKeyName  = "testkey.pem";
				curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

				// set the file with the certs vaildating the server
				// *pCACertFile = "cacert.pem";
				curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
				*/

				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				// request.setOpt(sslVerifyPeer);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				// request.setOpt(sslVerifyHost);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

				// request.setOpt(new curlpp::options::SslEngineDefault());
			}

			/*
			list<string> header;

			string contentLengthOrRangeHeader;
			if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
			{
				// Content-Range: bytes
				// $contentRangeStart-$contentRangeEnd/$binaryFileSize

				contentLengthOrRangeHeader = string("Content-Range: bytes ") + to_string(contentRangeStart) + "-" +
											 to_string(contentRangeEnd_Excluded - 1) + "/" + to_string(fileSizeInBytes);
			}
			else
			{
				contentLengthOrRangeHeader = string("Content-Length: ") + to_string(fileSizeInBytes);
			}
			header.push_back(contentLengthOrRangeHeader);

			{
				// string userPasswordEncoded =
				// Convert::base64_encode(_mmsAPIUser + ":" + _mmsAPIPassword);
				string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
				string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

				header.push_back(basicAuthorization);
			}

			*/
			string contentLengthOrRangeHeader;
			{
				if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
				{
					// Content-Range: bytes
					// $contentRangeStart-$contentRangeEnd/$binaryFileSize

					contentLengthOrRangeHeader =
						fmt::format("Content-Range: bytes {}-{}/{}", contentRangeStart, contentRangeEnd_Excluded - 1, fileSizeInBytes);
				}
				else
					contentLengthOrRangeHeader = fmt::format("Content-Length: {}", fileSizeInBytes);
				headersList = curl_slist_append(headersList, contentLengthOrRangeHeader.c_str());
				if (basicAuthenticationUser != "" && basicAuthenticationPassword != "")
				{
					string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":" + basicAuthenticationPassword);
					string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

					headersList = curl_slist_append(headersList, basicAuthorization.c_str());
				}

				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
			}

			// request.setOpt(new curlpp::options::WriteStream(&response));
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteResponseCallback);

			SPDLOG_INFO(
				"{} details"
				"{}"
				", url: {}"
				", basicAuthenticationUser: {}"
				", contentLengthOrRangeHeader: {}"
				", pathFileName: {}",
				api, referenceToLog, url, basicAuthenticationUser, contentLengthOrRangeHeader, pathFileName
			);

			chrono::system_clock::time_point start = chrono::system_clock::now();
			// request.perform();
			CURLcode curlCode = curl_easy_perform(curl);
			chrono::system_clock::time_point end = chrono::system_clock::now();
			if (curlCode != CURLE_OK)
			{
				string errorMessage = fmt::format(
					"{}. curl_easy_perform failed"
					", curlCode message: {}"
					", url: {}",
					api, curl_easy_strerror(curlCode), url
				);
				SPDLOG_ERROR(errorMessage);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				throw runtime_error(errorMessage);
			}

			(curlUploadData.mediaSourceFileStream).close();

			// sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (response.size() > 0 && (response.back() == 10 || response.back() == 13))
				response.pop_back();

			// long responseCode = curlpp::infos::ResponseCode::get(request);
			long responseCode;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
			if (responseCode == 200 || responseCode == 201)
			{
				SPDLOG_INFO(
					"{} success"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", response: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " ")
				);
			}
			else
			{
				string message = fmt::format(
					"{} failed, wrong return status"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", response: {}"
					", responseCode: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " "),
					responseCode
				);
				SPDLOG_ERROR(message);

				throw runtime_error(message);
			}

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			// return sResponse;
			break;
		}
		catch (runtime_error e)
		{
			if (response.find("502 Bad Gateway") != string::npos)
			{
				SPDLOG_ERROR(
					"{}. Server is not reachable, is it down?"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed (exception)"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", url: {}"
				", exception: {}"
				", response.str(): {}",
				api, referenceToLog, url, e.what(), response
			);

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return response;
}

/*
string MMSCURL::httpPostPutFormData(
	shared_ptr<spdlog::logger> logger, int64_t ingestionJobKey, string url, vector<pair<string, string>> formData,
	string requestType, // POST or PUT
	long timeoutInSeconds, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	// per vedere cosa manda curl
	// curl --trace-ascii - "https://....."

	string sResponse;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		ostringstream response;
		bool responseInitialized = false;
		try
		{
			// we could apply md5 to utc time
			string boundary = to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()));

			string endOfLine = "\r\n";

			// fill in formData
			string sFormData;
			for (pair<string, string> data : formData)
			{
				sFormData += ("--" + boundary + endOfLine);
				sFormData += ("Content-Disposition: form-data; name=\"" + data.first + "\"" + endOfLine + endOfLine + data.second + endOfLine);
			}
			sFormData += ("--" + boundary + "--" + endOfLine + endOfLine);

			curlpp::Cleanup cleaner;
			curlpp::Easy request;

			list<string> headers;
			headers.push_back("Content-Type: multipart/form-data; boundary=\"" + boundary + "\"");

			request.setOpt(new curlpp::options::Url(url));

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() && 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				bool bSslVerifyPeer = false;
				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				request.setOpt(sslVerifyPeer);

				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				request.setOpt(sslVerifyHost);
			}

			request.setOpt(new curlpp::options::HttpHeader(headers));

			request.setOpt(new curlpp::options::CustomRequest(requestType));
			request.setOpt(new curlpp::options::PostFields(sFormData));
			request.setOpt(new curlpp::options::PostFieldSize(sFormData.length()));

			request.setOpt(new curlpp::options::WriteStream(&response));

			logger->info(
				__FILEREF__ + "httpPostPutFile (" + requestType + ")" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", sFormData: " + sFormData
			);

			// store response headers in the response
			// You simply have to set next option to prefix the header to the
			// normal body output. request.setOpt(new
			// curlpp::options::Header(true));

			responseInitialized = true;
			chrono::system_clock::time_point start = chrono::system_clock::now();
			request.perform();
			chrono::system_clock::time_point end = chrono::system_clock::now();

			sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
				sResponse.pop_back();

			long responseCode = curlpp::infos::ResponseCode::get(request);
			if (responseCode == 200 || responseCode == 201)
			{
				string message = __FILEREF__ + "httpPostPutFile success" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", responseCode: " + to_string(responseCode) + ", @MMS statistics@ - elapsed (secs): @" +
								 to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@" + ", sFormData: " + sFormData +
								 ", sResponse: " + sResponse;
				logger->info(message);
			}
			else
			{
				string message = __FILEREF__ + "httpPostPutFile failed, wrong return status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", @MMS statistics@ - elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) +
								 "@" + ", sResponse: " + sResponse + ", responseCode: " + to_string(responseCode);
				logger->error(message);

				throw runtime_error(message);
			}

			// return sResponse;
			break;
		}
		catch (curlpp::LogicError &e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutFile failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " +
				requestType + ", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (curlpp::RuntimeError &e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutFile failed (RuntimeError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " +
				requestType + ", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (runtime_error e)
		{
			if (responseInitialized && response.str().find("502 Bad Gateway") != string::npos)
			{
				logger->error(
					__FILEREF__ + "Server is not reachable, is it down?" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " +
					requestType + ", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				logger->error(
					__FILEREF__ + "httpPostPutFile failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " + requestType +
					", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutFile failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " + requestType +
				", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return sResponse;
}
*/

string CurlWrapper::httpPostPutFormData(
	string url, vector<pair<string, string>> formData,
	string requestType, // POST or PUT
	long timeoutInSeconds, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	// per vedere cosa manda curl
	// curl --trace-ascii - "https://....."

	string api = "httpPostPutFormData";

	string response;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		CURL *curl = nullptr;
		struct curl_slist *headersList = nullptr;

		try
		{
			// we could apply md5 to utc time
			string boundary = to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()));

			string endOfLine = "\r\n";

			// fill in formData
			string sFormData;
			for (pair<string, string> data : formData)
			{
				sFormData += ("--" + boundary + endOfLine);
				sFormData += ("Content-Disposition: form-data; name=\"" + data.first + "\"" + endOfLine + endOfLine + data.second + endOfLine);
			}
			sFormData += ("--" + boundary + "--" + endOfLine + endOfLine);

			// curlpp::Cleanup cleaner;
			// curlpp::Easy request;

			curl = curl_easy_init();
			if (!curl)
			{
				string errorMessage = fmt::format("{}. curl_easy_init failed", api);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// request.setOpt(new curlpp::options::Url(url));
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			// request.setOpt(new curlpp::options::CustomRequest(requestType));
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, requestType.c_str());

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			// request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutInSeconds);

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() &&
			// 	0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				/*
				typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD>
				SslCertPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEY> SslKey; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEYTYPE> SslKeyType; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD>
				SslKeyPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLENGINE> SslEngine; typedef
				curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
				SslEngineDefault; typedef curlpp::OptionTrait<long,
				CURLOPT_SSLVERSION> SslVersion; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath; typedef
				curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE>
				RandomFile; typedef curlpp::OptionTrait<std::string,
				CURLOPT_EGDSOCKET> EgdSocket; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST>
				SslCipherList; typedef curlpp::OptionTrait<std::string,
				CURLOPT_KRB4LEVEL> Krb4Level;
				*/

				/*
				// cert is stored PEM coded in file...
				// since PEM is default, we needn't set it for PEM
				// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
				curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE>
				sslCertType("PEM"); equest.setOpt(sslCertType);

				// set the cert for client authentication
				// "testcert.pem"
				// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
				curlpp::OptionTrait<string, CURLOPT_SSLCERT>
				sslCert("cert.pem"); request.setOpt(sslCert);
				*/

				/*
				// sorry, for engine we must set the passphrase
				//   (if the key has one...)
				// const char *pPassphrase = NULL;
				if(pPassphrase)
					curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

				// if we use a key stored in a crypto engine,
				//   we must set the key type to "ENG"
				// pKeyType  = "PEM";
				curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

				// set the private key (file or ID in engine)
				// pKeyName  = "testkey.pem";
				curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

				// set the file with the certs vaildating the server
				// *pCACertFile = "cacert.pem";
				curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
				*/

				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				// request.setOpt(sslVerifyPeer);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				// request.setOpt(sslVerifyHost);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

				// request.setOpt(new curlpp::options::SslEngineDefault());
			}

			// list<string> headers;
			// headers.push_back("Content-Type: multipart/form-data; boundary=\"" + boundary + "\"");
			headersList = curl_slist_append(headersList, fmt::format("Content-Type: multipart/form-data; boundary=\"{}\"", boundary).c_str());

			// request.setOpt(new curlpp::options::PostFields(sFormData));
			// request.setOpt(new curlpp::options::PostFieldSize(sFormData.length()));
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, sFormData.length());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, sFormData.c_str());

			// request.setOpt(new curlpp::options::WriteStream(&response));
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteResponseCallback);

			SPDLOG_INFO(
				"{} details"
				"{}"
				", url: {}"
				", sFormData: {}",
				api, referenceToLog, url, sFormData
			);

			chrono::system_clock::time_point start = chrono::system_clock::now();
			// request.perform();
			CURLcode curlCode = curl_easy_perform(curl);
			chrono::system_clock::time_point end = chrono::system_clock::now();
			if (curlCode != CURLE_OK)
			{
				string errorMessage = fmt::format(
					"{}. curl_easy_perform failed"
					", curlCode message: {}"
					", url: {}",
					api, curl_easy_strerror(curlCode), url
				);
				SPDLOG_ERROR(errorMessage);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				throw runtime_error(errorMessage);
			}

			// sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (response.size() > 0 && (response.back() == 10 || response.back() == 13))
				response.pop_back();

			// long responseCode = curlpp::infos::ResponseCode::get(request);
			long responseCode;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
			if (responseCode == 200 || responseCode == 201)
			{
				SPDLOG_INFO(
					"{} success"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", response: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " ")
				);
			}
			else
			{
				string message = fmt::format(
					"{} failed, wrong return status"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", response: {}"
					", responseCode: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " "),
					responseCode
				);
				SPDLOG_ERROR(message);

				throw runtime_error(message);
			}

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			// return sResponse;
			break;
		}
		catch (runtime_error e)
		{
			if (response.find("502 Bad Gateway") != string::npos)
			{
				SPDLOG_ERROR(
					"{}. Server is not reachable, is it down?"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed (exception)"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", url: {}"
				", exception: {}"
				", response.str(): {}",
				api, referenceToLog, url, e.what(), response
			);

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return response;
}

/*
string MMSCURL::httpPostPutFileByFormData(
	shared_ptr<spdlog::logger> logger, int64_t ingestionJobKey, string url, vector<pair<string, string>> formData,
	string requestType, // POST or PUT
	long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes, string mediaContentType, int maxRetryNumber, int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string sResponse;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		ostringstream response;
		bool responseInitialized = false;
		try
		{
			CurlUploadFormData curlUploadFormData;
			curlUploadFormData.loggerName = logger->name();
			curlUploadFormData.mediaSourceFileStream.open(pathFileName, ios::binary);
			if (!curlUploadFormData.mediaSourceFileStream)
			{
				string message =
					__FILEREF__ + "open file failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", pathFileName: " + pathFileName;
				logger->error(message);

				throw runtime_error(message);
			}
			if (contentRangeStart > 0)
				curlUploadFormData.mediaSourceFileStream.seekg(contentRangeStart, ios::beg);
			curlUploadFormData.payloadBytesSent = 0;
			if (contentRangeEnd_Excluded > 0)
				curlUploadFormData.upToByte_Excluded = contentRangeEnd_Excluded;
			else
				curlUploadFormData.upToByte_Excluded = fileSizeInBytes;
			curlUploadFormData.formDataSent = false;
			curlUploadFormData.endOfFormDataSent = false;

			// we could apply md5 to utc time
			string boundary = to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()));

			string endOfLine = "\r\n";

			// fill in formData
			{
				for (pair<string, string> data : formData)
				{
					curlUploadFormData.formData += ("--" + boundary + endOfLine);
					curlUploadFormData.formData +=
						("Content-Disposition: form-data; name=\"" + data.first + "\"" + endOfLine + endOfLine + data.second + endOfLine);
				}

				if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
				{
					curlUploadFormData.formData += ("--" + boundary + endOfLine);
					// 2023-01-06: il caricamento del video su facebook fallisce
					// senza il campo filename
					curlUploadFormData.formData +=
						("Content-Disposition: form-data; "
						 "name=\"video_file_chunk\"; filename=\"" +
						 to_string(contentRangeStart) + "\"" + endOfLine + "Content-Type: " + mediaContentType + endOfLine +
						 "Content-Length: " + (to_string(contentRangeEnd_Excluded - contentRangeStart)) + endOfLine + endOfLine);
				}
				else
				{
					curlUploadFormData.formData += ("--" + boundary + endOfLine);
					curlUploadFormData.formData +=
						("Content-Disposition: form-data; name=\"source\"" + endOfLine + "Content-Type: " + mediaContentType + endOfLine +
						 "Content-Length: " + (to_string(fileSizeInBytes)) + endOfLine + endOfLine);
				}
			}
			curlUploadFormData.endOfFormData = endOfLine + "--" + boundary + "--" + endOfLine + endOfLine;

			curlpp::Cleanup cleaner;
			curlpp::Easy request;

			{
				curlpp::options::ReadFunctionCurlFunction curlUploadCallbackFunction(curlUploadFormDataCallback);
				curlpp::OptionTrait<void *, CURLOPT_READDATA> curlUploadDataData(&curlUploadFormData);
				request.setOpt(curlUploadCallbackFunction);
				request.setOpt(curlUploadDataData);

				bool upload = true;
				request.setOpt(new curlpp::options::Upload(upload));
			}

			request.setOpt(new curlpp::options::CustomRequest(requestType));
			int64_t postSize;
			if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
				postSize =
					(contentRangeEnd_Excluded - contentRangeStart) + curlUploadFormData.formData.size() + curlUploadFormData.endOfFormData.size();
			else
				postSize = fileSizeInBytes + curlUploadFormData.formData.size() + curlUploadFormData.endOfFormData.size();
			request.setOpt(new curlpp::options::PostFieldSizeLarge(postSize));

			list<string> header;

			string contentTypeHeader = "Content-Type: multipart/form-data; boundary=\"" + boundary + "\"";
			header.push_back(contentTypeHeader);

			// Setting the URL to retrive.
			request.setOpt(new curlpp::options::Url(url));

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() && 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				bool bSslVerifyPeer = false;
				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				request.setOpt(sslVerifyPeer);

				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				request.setOpt(sslVerifyHost);
			}

			request.setOpt(new curlpp::options::HttpHeader(header));

			request.setOpt(new curlpp::options::WriteStream(&response));

			logger->info(
				__FILEREF__ + "httpPostPutFile (" + requestType + ")" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", pathFileName: " + pathFileName + ", postSize: " + to_string(postSize) + ", curlUploadFormData.formData: " +
				curlUploadFormData.formData + ", curlUploadFormData.endOfFormData: " + curlUploadFormData.endOfFormData
			);

			// store response headers in the response
			// You simply have to set next option to prefix the header to the
			// normal body output. request.setOpt(new
			// curlpp::options::Header(true));

			responseInitialized = true;
			chrono::system_clock::time_point start = chrono::system_clock::now();
			request.perform();
			chrono::system_clock::time_point end = chrono::system_clock::now();

			(curlUploadFormData.mediaSourceFileStream).close();

			sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
				sResponse.pop_back();

			long responseCode = curlpp::infos::ResponseCode::get(request);
			if (responseCode == 200 || responseCode == 201)
			{
				string message = __FILEREF__ + "httpPostPutFile success" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", responseCode: " + to_string(responseCode) + ", @MMS statistics@ - elapsed (secs): @" +
								 to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) +
								 "@"
								 // + ", curlUploadFormData.formData: " +
								 // curlUploadFormData.formData
								 // + ", curlUploadFormData.endOfFormData: " +
								 // curlUploadFormData.endOfFormData
								 + ", curlUploadFormData.payloadBytesSent: " + to_string(curlUploadFormData.payloadBytesSent) +
								 ", sResponse: " + sResponse;
				logger->info(message);
			}
			else
			{
				string message = __FILEREF__ + "httpPostPutFile failed, wrong return status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								 ", @MMS statistics@ - elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) +
								 "@" + ", curlUploadFormData.formData: " + curlUploadFormData.formData +
								 ", curlUploadFormData.endOfFormData: " + curlUploadFormData.endOfFormData + ", sResponse: " + sResponse +
								 ", responseCode: " + to_string(responseCode);
				logger->error(message);

				throw runtime_error(message);
			}

			// return sResponse;
			break;
		}
		catch (curlpp::LogicError &e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutFile failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " +
				requestType + ", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (curlpp::RuntimeError &e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutFile failed (RuntimeError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " +
				requestType + ", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (runtime_error e)
		{
			if (responseInitialized && response.str().find("502 Bad Gateway") != string::npos)
			{
				logger->error(
					__FILEREF__ + "Server is not reachable, is it down?" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " +
					requestType + ", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				logger->error(
					__FILEREF__ + "httpPostPutFile failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " + requestType +
					", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(
						__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
						", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			logger->error(
				__FILEREF__ + "httpPostPutFile failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", requestType: " + requestType +
				", url: " + url + ", exception: " + e.what() + ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return sResponse;
}
*/

string CurlWrapper::httpPostPutFileByFormData(
	string url, vector<pair<string, string>> formData,
	string requestType, // POST or PUT
	long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes, string mediaContentType, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string api = "httpPostPutFileByFormData";

	string response;
	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		CURL *curl = nullptr;
		struct curl_slist *headersList = nullptr;

		try
		{
			CurlUploadFormData curlUploadFormData;
			curlUploadFormData.mediaSourceFileStream.open(pathFileName, ios::binary);
			if (!curlUploadFormData.mediaSourceFileStream)
			{
				string message = fmt::format(
					"open file failed"
					"{}"
					", pathFileName: {}",
					referenceToLog, pathFileName
				);
				SPDLOG_ERROR(message);

				throw runtime_error(message);
			}
			if (contentRangeStart > 0)
				curlUploadFormData.mediaSourceFileStream.seekg(contentRangeStart, ios::beg);
			curlUploadFormData.payloadBytesSent = 0;
			if (contentRangeEnd_Excluded > 0)
				curlUploadFormData.upToByte_Excluded = contentRangeEnd_Excluded;
			else
				curlUploadFormData.upToByte_Excluded = fileSizeInBytes;
			curlUploadFormData.formDataSent = false;
			curlUploadFormData.endOfFormDataSent = false;

			// we could apply md5 to utc time
			string boundary = to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()));

			string endOfLine = "\r\n";

			// fill in formData
			{
				for (pair<string, string> data : formData)
				{
					curlUploadFormData.formData += ("--" + boundary + endOfLine);
					curlUploadFormData.formData +=
						("Content-Disposition: form-data; name=\"" + data.first + "\"" + endOfLine + endOfLine + data.second + endOfLine);
				}

				if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
				{
					curlUploadFormData.formData += ("--" + boundary + endOfLine);
					// 2023-01-06: il caricamento del video su facebook fallisce
					// senza il campo filename
					curlUploadFormData.formData +=
						("Content-Disposition: form-data; "
						 "name=\"video_file_chunk\"; filename=\"" +
						 to_string(contentRangeStart) + "\"" + endOfLine + "Content-Type: " + mediaContentType + endOfLine +
						 "Content-Length: " + (to_string(contentRangeEnd_Excluded - contentRangeStart)) + endOfLine + endOfLine);
				}
				else
				{
					curlUploadFormData.formData += ("--" + boundary + endOfLine);
					curlUploadFormData.formData +=
						("Content-Disposition: form-data; name=\"source\"" + endOfLine + "Content-Type: " + mediaContentType + endOfLine +
						 "Content-Length: " + (to_string(fileSizeInBytes)) + endOfLine + endOfLine);
				}
			}
			curlUploadFormData.endOfFormData = endOfLine + "--" + boundary + "--" + endOfLine + endOfLine;

			// curlpp::Cleanup cleaner;
			// curlpp::Easy request;

			curl = curl_easy_init();
			if (!curl)
			{
				string errorMessage = fmt::format("{}. curl_easy_init failed", api);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			{
				// curlpp::options::ReadFunctionCurlFunction curlUploadCallbackFunction(curlUploadFormDataCallback);
				// curlpp::OptionTrait<void *, CURLOPT_READDATA> curlUploadDataData(&curlUploadFormData);
				// request.setOpt(curlUploadCallbackFunction);
				// request.setOpt(curlUploadDataData);
				curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&curlUploadFormData);
				curl_easy_setopt(curl, CURLOPT_READFUNCTION, curlUploadFormDataCallback);

				// bool upload = true;
				// request.setOpt(new curlpp::options::Upload(upload));
				curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
			}

			// request.setOpt(new curlpp::options::Url(url));
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			// request.setOpt(new curlpp::options::CustomRequest(requestType));
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, requestType.c_str());

			{
				int64_t postSize;
				if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
					postSize =
						(contentRangeEnd_Excluded - contentRangeStart) + curlUploadFormData.formData.size() + curlUploadFormData.endOfFormData.size();
				else
					postSize = fileSizeInBytes + curlUploadFormData.formData.size() + curlUploadFormData.endOfFormData.size();
				// request.setOpt(new curlpp::options::PostFieldSizeLarge(postSize));
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, postSize);
			}

			// timeout consistent with nginx configuration
			// (fastcgi_read_timeout)
			// request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutInSeconds);

			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() &&
			// 	0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				/*
				typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD>
				SslCertPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEY> SslKey; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEYTYPE> SslKeyType; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD>
				SslKeyPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLENGINE> SslEngine; typedef
				curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
				SslEngineDefault; typedef curlpp::OptionTrait<long,
				CURLOPT_SSLVERSION> SslVersion; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath; typedef
				curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE>
				RandomFile; typedef curlpp::OptionTrait<std::string,
				CURLOPT_EGDSOCKET> EgdSocket; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST>
				SslCipherList; typedef curlpp::OptionTrait<std::string,
				CURLOPT_KRB4LEVEL> Krb4Level;
				*/

				/*
				// cert is stored PEM coded in file...
				// since PEM is default, we needn't set it for PEM
				// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
				curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE>
				sslCertType("PEM"); equest.setOpt(sslCertType);

				// set the cert for client authentication
				// "testcert.pem"
				// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
				curlpp::OptionTrait<string, CURLOPT_SSLCERT>
				sslCert("cert.pem"); request.setOpt(sslCert);
				*/

				/*
				// sorry, for engine we must set the passphrase
				//   (if the key has one...)
				// const char *pPassphrase = NULL;
				if(pPassphrase)
					curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

				// if we use a key stored in a crypto engine,
				//   we must set the key type to "ENG"
				// pKeyType  = "PEM";
				curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

				// set the private key (file or ID in engine)
				// pKeyName  = "testkey.pem";
				curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

				// set the file with the certs vaildating the server
				// *pCACertFile = "cacert.pem";
				curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
				*/

				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				// request.setOpt(sslVerifyPeer);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				// request.setOpt(sslVerifyHost);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

				// request.setOpt(new curlpp::options::SslEngineDefault());
			}

			/*
			list<string> header;

			string contentTypeHeader = "Content-Type: multipart/form-data; boundary=\"" + boundary + "\"";
			header.push_back(contentTypeHeader);
			*/
			headersList = curl_slist_append(headersList, fmt::format("Content-Type: multipart/form-data; boundary=\"{}\"", boundary).c_str());
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);

			// request.setOpt(new curlpp::options::WriteStream(&response));
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteResponseCallback);

			SPDLOG_INFO(
				"{} details"
				"{}"
				", url: {}"
				", pathFileName: {}"
				", curlUploadFormData.formData: {}"
				", curlUploadFormData.endOfFormData: {}",
				api, referenceToLog, url, pathFileName, curlUploadFormData.formData, curlUploadFormData.endOfFormData
			);

			chrono::system_clock::time_point start = chrono::system_clock::now();
			// request.perform();
			CURLcode curlCode = curl_easy_perform(curl);
			chrono::system_clock::time_point end = chrono::system_clock::now();
			if (curlCode != CURLE_OK)
			{
				string errorMessage = fmt::format(
					"{}. curl_easy_perform failed"
					", curlCode message: {}"
					", url: {}",
					api, curl_easy_strerror(curlCode), url
				);
				SPDLOG_ERROR(errorMessage);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				throw runtime_error(errorMessage);
			}

			(curlUploadFormData.mediaSourceFileStream).close();

			// sResponse = response.str();
			// LF and CR create problems to the json parser...
			while (response.size() > 0 && (response.back() == 10 || response.back() == 13))
				response.pop_back();

			// long responseCode = curlpp::infos::ResponseCode::get(request);
			long responseCode;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
			if (responseCode == 200 || responseCode == 201)
			{
				SPDLOG_INFO(
					"{} success"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", curlUploadFormData.payloadBytesSent: {}"
					", response: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), curlUploadFormData.payloadBytesSent,
					regex_replace(response, regex("\n"), " ")
				);
			}
			else
			{
				string message = fmt::format(
					"{} failed, wrong return status"
					"{}"
					", @MMS statistics@ - elapsed (secs): @{}@"
					", curlUploadFormData.formData: {}"
					", curlUploadFormData.endOfFormData: {}"
					", response: {}"
					", responseCode: {}",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), curlUploadFormData.formData,
					curlUploadFormData.endOfFormData, regex_replace(response, regex("\n"), " "), responseCode
				);
				SPDLOG_ERROR(message);

				throw runtime_error(message);
			}

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			// return sResponse;
			break;
		}
		catch (runtime_error e)
		{
			if (response.find("502 Bad Gateway") != string::npos)
			{
				SPDLOG_ERROR(
					"{}. Server is not reachable, is it down?"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed (exception)"
					"{}"
					", url: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, e.what(), response
				);

				if (headersList)
				{
					curl_slist_free_all(headersList); /* free the list */
					headersList = nullptr;
				}
				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				if (retryNumber < maxRetryNumber)
				{
					SPDLOG_INFO(
						"{}. Sleep before trying again"
						"{}"
						", retryNumber: {}"
						", maxRetryNumber: {}"
						", secondsToWaitBeforeToRetry: {}",
						api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", url: {}"
				", exception: {}"
				", response.str(): {}",
				api, referenceToLog, url, e.what(), response
			);

			if (headersList)
			{
				curl_slist_free_all(headersList); /* free the list */
				headersList = nullptr;
			}
			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return response;
}

/*
void MMSCURL::downloadFile(
	shared_ptr<spdlog::logger> logger, int64_t ingestionJobKey, string url, string destBinaryPathName, curlpp::types::ProgressFunctionFunctor functor,
	int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	int retryNumber = -1;

	// Ci sarebbe la possibilità di fare un resume in caso di errore, guarda
	// l'implementazione in MMSEngineProcessor.cpp. Quella implementazione si
	// dovrebbe anche spostare in questa lib
	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		try
		{
			long downloadChunkSizeInMegaBytes = 500;

			CurlDownloadData curlDownloadData;
			curlDownloadData.loggerName = logger->name();
			curlDownloadData.ingestionJobKey = ingestionJobKey;
			curlDownloadData.currentChunkNumber = 0;
			curlDownloadData.currentTotalSize = 0;
			curlDownloadData.destBinaryPathName = destBinaryPathName;
			curlDownloadData.maxChunkFileSize = downloadChunkSizeInMegaBytes * 1000000;

			// fstream mediaSourceFileStream(destBinaryPathName, ios::binary |
			// ios::out); mediaSourceFileStream.exceptions(ios::badbit |
			// ios::failbit);   // setting the exception mask FILE
			// *mediaSourceFileStream = fopen(destBinaryPathName.c_str(), "wb");

			curlpp::Cleanup cleaner;
			curlpp::Easy request;

			// Set the writer callback to enable cURL
			// to write result in a memory area
			// request.setOpt(new
			// curlpp::options::WriteStream(&mediaSourceFileStream));

			// which timeout we have to use here???
			// request.setOpt(new
			// curlpp::options::Timeout(curlTimeoutInSeconds));

			curlpp::options::WriteFunctionCurlFunction curlDownloadCallbackFunction(curlDownloadCallback);
			curlpp::OptionTrait<void *, CURLOPT_WRITEDATA> curlDownloadDataData(&curlDownloadData);
			request.setOpt(curlDownloadCallbackFunction);
			request.setOpt(curlDownloadDataData);

			// Setting the URL to retrive.
			request.setOpt(new curlpp::options::Url(url));
			// string httpsPrefix("https");
			// if (url.size() >= httpsPrefix.size() && 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
			if (url.starts_with("https"))
			{
				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				request.setOpt(sslVerifyPeer);

				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				request.setOpt(sslVerifyHost);
			}

			// chrono::system_clock::time_point lastProgressUpdate =
			// chrono::system_clock::now(); double lastPercentageUpdated = -1.0;
			// curlpp::types::ProgressFunctionFunctor functor =
			// bind(&MMSEngineProcessor::progressDownloadCallback, this,
			// 	ingestionJobKey, lastProgressUpdate, lastPercentageUpdated,
			// downloadingStoppedByUser, 	placeholders::_1, placeholders::_2,
			// placeholders::_3, placeholders::_4);
			request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
			request.setOpt(new curlpp::options::NoProgress(0L));

			logger->info(
				__FILEREF__ + "Downloading media file" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", destBinaryPathName: " + destBinaryPathName + ", retryNumber: " + to_string(retryNumber)
			);

			// store response headers in the response
			// You simply have to set next option to prefix the header to the
			// normal body output. request.setOpt(new
			// curlpp::options::Header(true));

			chrono::system_clock::time_point start = chrono::system_clock::now();
			request.perform();
			chrono::system_clock::time_point end = chrono::system_clock::now();

			(curlDownloadData.mediaSourceFileStream).close();

			string message = __FILEREF__ + "download finished" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							 ", @MMS statistics@ - elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@";
			logger->info(message);

			break;
		}
		catch (curlpp::LogicError &e)
		{
			logger->error(
				__FILEREF__ + "Download failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what()
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
		catch (curlpp::RuntimeError &e)
		{
			logger->error(
				__FILEREF__ + "Download failed (RuntimeError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what()
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
		catch (runtime_error e)
		{
			logger->error(
				__FILEREF__ + "Download failed (runtime_error)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what()
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
		catch (exception e)
		{
			logger->error(
				__FILEREF__ + "Download failed (exception)" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", url: " + url +
				", exception: " + e.what()
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(
					__FILEREF__ + "sleep before trying again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", retryNumber: " + to_string(retryNumber) + ", maxRetryNumber: " + to_string(maxRetryNumber) +
					", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}
}
*/

void CurlWrapper::downloadFile(
	string url, string destBinaryPathName, function<int(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)> progressCallback, void *progressData,
	long downloadChunkSizeInMegaBytes, string referenceToLog, int maxRetryNumber, bool resumeActive, int secondsToWaitBeforeToRetry
)
{
	string api = "downloadFile";

	/*
		- aggiungere un timeout nel caso nessun pacchetto è ricevuto entro XXXX
	seconds
		- per il resume:
			l'apertura dello stream of dovrà essere fatta in append in questo
	caso usare l'opzione CURLOPT_RESUME_FROM o CURLOPT_RESUME_FROM_LARGE (>2GB)
	per dire da dove ripartire per ftp vedere
	https://raw.githubusercontent.com/curl/curl/master/docs/examples/ftpuploadresume.c

	RESUMING FILE TRANSFERS

	 To continue a file transfer where it was previously aborted, curl supports
	 resume on http(s) downloads as well as ftp uploads and downloads.

	 Continue downloading a document:

			curl -C - -o file ftp://ftp.server.com/path/file

	 Continue uploading a document(*1):

			curl -C - -T file ftp://ftp.server.com/path/file

	 Continue downloading a document from a web server(*2):

			curl -C - -o file http://www.server.com/

	 (*1) = This requires that the ftp server supports the non-standard command
			SIZE. If it doesn't, curl will say so.

	 (*2) = This requires that the web server supports at least HTTP/1.1. If it
			doesn't, curl will say so.
	 */

	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		CURL *curl = nullptr;

		try
		{
			long long resumeFileSize = 0;
			bool resumeScenario;
			if (retryNumber == 0 || !resumeActive)
				resumeScenario = false;
			else
				resumeScenario = true;

			CurlDownloadData curlDownloadData;
			if (!resumeScenario)
			{
				// primo tentativo

				curlDownloadData.referenceToLog = referenceToLog;
				curlDownloadData.currentChunkNumber = 0;
				curlDownloadData.currentTotalSize = 0;
				curlDownloadData.destBinaryPathName = destBinaryPathName;
				curlDownloadData.maxChunkFileSize = downloadChunkSizeInMegaBytes * 1000000;
			}
			else
			{
				// resume scenario

				// FILE *mediaSourceFileStream =
				// fopen(destBinaryPathName.c_str(), "wb+");
				{
					ofstream mediaSourceFileStream(destBinaryPathName, ofstream::binary | ofstream::app);
					resumeFileSize = mediaSourceFileStream.tellp();
					mediaSourceFileStream.close();
				}

				curlDownloadData.referenceToLog = referenceToLog;
				curlDownloadData.destBinaryPathName = destBinaryPathName;
				curlDownloadData.maxChunkFileSize = downloadChunkSizeInMegaBytes * 1000000;

				curlDownloadData.currentChunkNumber = resumeFileSize % curlDownloadData.maxChunkFileSize;
				// fileSize = curlDownloadData.currentChunkNumber *
				// curlDownloadData.maxChunkFileSize;
				curlDownloadData.currentTotalSize = resumeFileSize;

				SPDLOG_INFO(
					"Coming from a download failure, trying to Resume"
					"{}"
					", destBinaryPathName: {}"
					", curlDownloadData.currentTotalSize/resumeFileSize: {}"
					", curlDownloadData.currentChunkNumber: {}",
					referenceToLog, destBinaryPathName, resumeFileSize, curlDownloadData.currentChunkNumber
				);
			}

			// curlpp::Cleanup cleaner;
			// curlpp::Easy request;

			curl = curl_easy_init();
			if (!curl)
			{
				string errorMessage = fmt::format("{}. curl_easy_init failed", api);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// Set the writer callback to enable cURL to write result in a memory area
			// request.setOpt(new curlpp::options::WriteStream(&mediaSourceFileStream));

			// which timeout we have to use here???
			// request.setOpt(new curlpp::options::Timeout(curlTimeoutInSeconds));

			/*
			curlpp::options::WriteFunctionCurlFunction curlDownloadCallbackFunction(curlDownloadCallback);
			curlpp::OptionTrait<void *, CURLOPT_WRITEDATA> curlDownloadDataData(&curlDownloadData);
			request.setOpt(curlDownloadCallbackFunction);
			request.setOpt(curlDownloadDataData);
			*/
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&curlDownloadData);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlDownloadCallback);

			// request.setOpt(new curlpp::options::Url(url));
			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

			if (url.starts_with("https"))
			{
				/*
				typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD>
				SslCertPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEY> SslKey; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLKEYTYPE> SslKeyType; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD>
				SslKeyPasswd; typedef curlpp::OptionTrait<std::string,
				CURLOPT_SSLENGINE> SslEngine; typedef
				curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
				SslEngineDefault; typedef curlpp::OptionTrait<long,
				CURLOPT_SSLVERSION> SslVersion; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo; typedef
				curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath; typedef
				curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE>
				RandomFile; typedef curlpp::OptionTrait<std::string,
				CURLOPT_EGDSOCKET> EgdSocket; typedef
				curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST>
				SslCipherList; typedef curlpp::OptionTrait<std::string,
				CURLOPT_KRB4LEVEL> Krb4Level;
				*/

				/*
				// cert is stored PEM coded in file...
				// since PEM is default, we needn't set it for PEM
				// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
				curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE>
				sslCertType("PEM"); equest.setOpt(sslCertType);

				// set the cert for client authentication
				// "testcert.pem"
				// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
				curlpp::OptionTrait<string, CURLOPT_SSLCERT>
				sslCert("cert.pem"); request.setOpt(sslCert);
				*/

				/*
				// sorry, for engine we must set the passphrase
				//   (if the key has one...)
				// const char *pPassphrase = NULL;
				if(pPassphrase)
					curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

				// if we use a key stored in a crypto engine,
				//   we must set the key type to "ENG"
				// pKeyType  = "PEM";
				curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

				// set the private key (file or ID in engine)
				// pKeyName  = "testkey.pem";
				curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

				// set the file with the certs vaildating the server
				// *pCACertFile = "cacert.pem";
				curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
				*/

				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
				// request.setOpt(sslVerifyPeer);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

				// curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
				// request.setOpt(sslVerifyHost);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

				// request.setOpt(new curlpp::options::SslEngineDefault());
			}

			// chrono::system_clock::time_point lastProgressUpdate =
			// chrono::system_clock::now(); double lastPercentageUpdated = -1.0;
			// curlpp::types::ProgressFunctionFunctor functor =
			// bind(&MMSEngineProcessor::progressDownloadCallback, this,
			// 	, lastProgressUpdate, lastPercentageUpdated,
			// downloadingStoppedByUser, 	placeholders::_1, placeholders::_2,
			// placeholders::_3, placeholders::_4);
			// request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
			// request.setOpt(new curlpp::options::NoProgress(0L));
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &progressCallback);
			curl_easy_setopt(curl, CURLOPT_XFERINFODATA, progressData);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

			if (resumeScenario)
			{
				// resume scenario

				if (resumeFileSize > 2 * 1000 * 1000 * 1000)
					curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, resumeFileSize);
				else
					curl_easy_setopt(curl, CURLOPT_RESUME_FROM, resumeFileSize);
			}

			SPDLOG_INFO(
				"{} details"
				"{}"
				", url: {}",
				api, referenceToLog, url
			);

			chrono::system_clock::time_point start = chrono::system_clock::now();
			// request.perform();
			CURLcode curlCode = curl_easy_perform(curl);
			chrono::system_clock::time_point end = chrono::system_clock::now();
			if (curlCode != CURLE_OK)
			{
				string errorMessage = fmt::format(
					"{}. curl_easy_perform failed"
					", curlCode message: {}"
					", url: {}",
					api, curl_easy_strerror(curlCode), url
				);
				SPDLOG_ERROR(errorMessage);

				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				throw runtime_error(errorMessage);
			}

			(curlDownloadData.mediaSourceFileStream).close();

			SPDLOG_INFO(
				"{} success"
				"{}"
				", @MMS statistics@ - elapsed (secs): @{}@",
				api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count()
			);

			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			break;
		}
		catch (runtime_error e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", url: {}"
				", exception: {}",
				api, referenceToLog, url, e.what()
			);

			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", url: {}"
				", exception: {}",
				api, referenceToLog, url, e.what()
			);

			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}
}

/*
void MMSCURL::ftpFile(
	shared_ptr<spdlog::logger> logger, int64_t ingestionJobKey, string filePathName, string fileName, int64_t sizeInBytes, string ftpServer,
	int ftpPort, string ftpUserName, string ftpPassword, string ftpRemoteDirectory, string ftpRemoteFileName,
	curlpp::types::ProgressFunctionFunctor functor
)
{

	// curl -T localfile.ext
	// ftp://username:password@ftp.server.com/remotedir/remotefile.zip

	try
	{
		logger->info(__FILEREF__ + "ftpFile" + ", ingestionJobKey: " + to_string(ingestionJobKey));

		string ftpUrl = "ftp://" + ftpUserName + ":" + ftpPassword + "@" + ftpServer + ":" + to_string(ftpPort) + ftpRemoteDirectory;

		if (ftpRemoteDirectory.size() == 0 || ftpRemoteDirectory.back() != '/')
			ftpUrl += "/";

		if (ftpRemoteFileName == "")
			ftpUrl += fileName;
		else
			ftpUrl += ftpRemoteFileName;

		logger->info(
			__FILEREF__ + "FTP Uploading" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", filePathName: " + filePathName +
			", sizeInBytes: " + to_string(sizeInBytes) + ", ftpUrl: " + ftpUrl
		);

		ifstream mmsAssetStream(filePathName, ifstream::binary);
		// FILE *mediaSourceFileStream =
		// fopen(workspaceIngestionBinaryPathName.c_str(), "wb");

		// 1. PORT-mode FTP (Active) - NO Firewall friendly
		//  - FTP client: Sends a request to open a command channel from its TCP
		//  port (i.e.: 6000) to the FTP server’s TCP port 21
		//  - FTP client: Sends a data request (PORT command) to the FTP server.
		//  The FTP client includes in the PORT command the data port number
		//      it opened to receive data. In this example, the FTP client has
		//      opened TCP port 6001 to receive the data.
		//  - FTP server opens a new inbound connection to the FTP client on the
		//  port indicated by the FTP client in the PORT command.
		//      The FTP server source port is TCP port 20. In this example, the
		//      FTP server sends data from its own TCP port 20 to the FTP
		//      client’s TCP port 6001.
		//  In this conversation, two connections were established: an outbound
		//  connection initiated by the FTP client and an inbound connection
		//  established by the FTP server.
		// 2. PASV-mode FTP (Passive) - Firewall friendly
		//  - FTP client sends a request to open a command channel from its TCP
		//  port (i.e.: 6000) to the FTP server’s TCP port 21
		//  - FTP client sends a PASV command requesting that the FTP server
		//  open a port number that the FTP client can connect to establish the
		//  data channel.
		//      FTP serve sends over the command channel the TCP port number
		//      that the FTP client can initiate a connection to establish the
		//      data channel (i.e.: 7000)
		//  - FTP client opens a new connection from its own response port TCP
		//  6001 to the FTP server’s data channel 7000. Data transfer takes
		//  place through this channel.

		// Active/Passive... see the next URL, section 'FTP Peculiarities We
		// Need' https://curl.haxx.se/libcurl/c/libcurl-tutorial.html

		// https://curl.haxx.se/libcurl/c/ftpupload.html
		curlpp::Cleanup cleaner;
		curlpp::Easy request;

		request.setOpt(new curlpp::options::Url(ftpUrl));
		request.setOpt(new curlpp::options::Verbose(false));
		request.setOpt(new curlpp::options::Upload(true));

		// which timeout we have to use here???
		// request.setOpt(new curlpp::options::Timeout(curlTimeoutInSeconds));

		request.setOpt(new curlpp::options::ReadStream(&mmsAssetStream));
		request.setOpt(new curlpp::options::InfileSizeLarge((curl_off_t)sizeInBytes));

		bool bFtpUseEpsv = false;
		curlpp::OptionTrait<bool, CURLOPT_FTP_USE_EPSV> ftpUseEpsv(bFtpUseEpsv);
		request.setOpt(ftpUseEpsv);

		// curl will default to binary transfer mode for FTP,
		// and you ask for ascii mode instead with -B, --use-ascii or
		// by making sure the URL ends with ;type=A.

		// timeout (CURLOPT_FTP_RESPONSE_TIMEOUT)

		bool bCreatingMissingDir = true;
		curlpp::OptionTrait<bool, CURLOPT_FTP_CREATE_MISSING_DIRS> creatingMissingDir(bCreatingMissingDir);
		request.setOpt(creatingMissingDir);

		// string ftpsPrefix("ftps");
		// if (ftpUrl.size() >= ftpsPrefix.size() && 0 == ftpUrl.compare(0, ftpsPrefix.size(), ftpsPrefix))
		if (ftpUrl.starts_with("ftps"))
		{
		}

		// FTP progress works only in case of FTP Passive
		// chrono::system_clock::time_point lastProgressUpdate =
		// chrono::system_clock::now(); double lastPercentageUpdated = -1.0;
		// bool uploadingStoppedByUser = false;
		// curlpp::types::ProgressFunctionFunctor functor =
		// bind(&MMSEngineProcessor::progressUploadCallback, this,
		// 	ingestionJobKey, lastProgressUpdate, lastPercentageUpdated,
		// uploadingStoppedByUser, 	placeholders::_1, placeholders::_2,
		// placeholders::_3, placeholders::_4);
		{
			request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
			request.setOpt(new curlpp::options::NoProgress(0L));
		}

		logger->info(
			__FILEREF__ + "FTP Uploading media file" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", filePathName: " + filePathName +
			", sizeInBytes: " + to_string(sizeInBytes)
		);
		request.perform();
	}
	catch (curlpp::LogicError &e)
	{
		string errorMessage = __FILEREF__ + "Download failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", filePathName: " + filePathName + ", exception: " + e.what();
		logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (curlpp::RuntimeError &e)
	{
		string errorMessage = __FILEREF__ + "Download failed (RuntimeError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", filePathName: " + filePathName + ", exception: " + e.what();
		logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = __FILEREF__ + "Download failed (exception)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", filePathName: " + filePathName + ", exception: " + e.what();
		logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
}
*/

void CurlWrapper::ftpFile(
	string filePathName, string fileName, int64_t sizeInBytes, string ftpServer, int ftpPort, string ftpUserName, string ftpPassword,
	string ftpRemoteDirectory, string ftpRemoteFileName, function<int(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)> progressCallback,
	void *progressData, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string api = "ftpFile";

	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		CURL *curl = nullptr;

		string ftpUrl;

		try
		{
			ftpUrl = fmt::format("ftp://{}:{}@{}:{}{}", ftpUserName, ftpPassword, ftpServer, ftpPort, ftpRemoteDirectory);

			if (ftpRemoteDirectory.size() == 0 || ftpRemoteDirectory.back() != '/')
				ftpUrl += "/";

			if (ftpRemoteFileName == "")
				ftpUrl += fileName;
			else
				ftpUrl += ftpRemoteFileName;

			ifstream mmsAssetStream(filePathName, ifstream::binary);
			// FILE *mediaSourceFileStream =
			// fopen(workspaceIngestionBinaryPathName.c_str(), "wb");

			// 1. PORT-mode FTP (Active) - NO Firewall friendly
			//  - FTP client: Sends a request to open a command channel from its TCP
			//  port (i.e.: 6000) to the FTP server’s TCP port 21
			//  - FTP client: Sends a data request (PORT command) to the FTP server.
			//  The FTP client includes in the PORT command the data port number
			//      it opened to receive data. In this example, the FTP client has
			//      opened TCP port 6001 to receive the data.
			//  - FTP server opens a new inbound connection to the FTP client on the
			//  port indicated by the FTP client in the PORT command.
			//      The FTP server source port is TCP port 20. In this example, the
			//      FTP server sends data from its own TCP port 20 to the FTP
			//      client’s TCP port 6001.
			//  In this conversation, two connections were established: an outbound
			//  connection initiated by the FTP client and an inbound connection
			//  established by the FTP server.
			// 2. PASV-mode FTP (Passive) - Firewall friendly
			//  - FTP client sends a request to open a command channel from its TCP
			//  port (i.e.: 6000) to the FTP server’s TCP port 21
			//  - FTP client sends a PASV command requesting that the FTP server
			//  open a port number that the FTP client can connect to establish the
			//  data channel.
			//      FTP serve sends over the command channel the TCP port number
			//      that the FTP client can initiate a connection to establish the
			//      data channel (i.e.: 7000)
			//  - FTP client opens a new connection from its own response port TCP
			//  6001 to the FTP server’s data channel 7000. Data transfer takes
			//  place through this channel.

			// Active/Passive... see the next URL, section 'FTP Peculiarities We
			// Need' https://curl.haxx.se/libcurl/c/libcurl-tutorial.html

			// https://curl.haxx.se/libcurl/c/ftpupload.html

			// curlpp::Cleanup cleaner;
			// curlpp::Easy request;

			curl = curl_easy_init();
			if (!curl)
			{
				string errorMessage = fmt::format("{}. curl_easy_init failed", api);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// request.setOpt(new curlpp::options::Url(ftpUrl));
			curl_easy_setopt(curl, CURLOPT_URL, ftpUrl.c_str());

			// request.setOpt(new curlpp::options::Verbose(false));
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

			// request.setOpt(new curlpp::options::Upload(true));
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

			// which timeout we have to use here???
			// request.setOpt(new curlpp::options::Timeout(curlTimeoutInSeconds));

			// request.setOpt(new curlpp::options::ReadStream(&mmsAssetStream));
			curl_easy_setopt(curl, CURLOPT_READDATA, &mmsAssetStream);

			// request.setOpt(new curlpp::options::InfileSizeLarge((curl_off_t)sizeInBytes));
			curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, sizeInBytes);

			/*
			bool bFtpUseEpsv = false;
			curlpp::OptionTrait<bool, CURLOPT_FTP_USE_EPSV> ftpUseEpsv(bFtpUseEpsv);
			request.setOpt(ftpUseEpsv);
			*/
			curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0L);

			// curl will default to binary transfer mode for FTP,
			// and you ask for ascii mode instead with -B, --use-ascii or
			// by making sure the URL ends with ;type=A.

			// timeout (CURLOPT_FTP_RESPONSE_TIMEOUT)

			/*
			bool bCreatingMissingDir = true;
			curlpp::OptionTrait<bool, CURLOPT_FTP_CREATE_MISSING_DIRS> creatingMissingDir(bCreatingMissingDir);
			request.setOpt(creatingMissingDir);
			*/
			curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);

			// string ftpsPrefix("ftps");
			// if (ftpUrl.size() >= ftpsPrefix.size() && 0 == ftpUrl.compare(0, ftpsPrefix.size(), ftpsPrefix))
			if (ftpUrl.starts_with("ftps"))
			{
				/* Next statements is in case we want ftp protocol to use SSL or TLS
				 * google CURLOPT_FTPSSLAUTH and CURLOPT_FTP_SSL

				// disconnect if we can't validate server's cert
				bool bSslVerifyPeer = false;
				curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER>
				sslVerifyPeer(bSslVerifyPeer); request.setOpt(sslVerifyPeer);

				curlpp::OptionTrait<curl_ftpssl, CURLOPT_FTP_SSL>
				ftpSsl(CURLFTPSSL_TRY); request.setOpt(ftpSsl);

				curlpp::OptionTrait<curl_ftpauth, CURLOPT_FTPSSLAUTH>
				ftpSslAuth(CURLFTPAUTH_TLS); request.setOpt(ftpSslAuth);
				 */
			}

			// FTP progress works only in case of FTP Passive
			// chrono::system_clock::time_point lastProgressUpdate =
			// chrono::system_clock::now(); double lastPercentageUpdated = -1.0;
			// bool uploadingStoppedByUser = false;
			// curlpp::types::ProgressFunctionFunctor functor =
			// bind(&MMSEngineProcessor::progressUploadCallback, this,
			// 	referenceToLog, lastProgressUpdate, lastPercentageUpdated,
			// uploadingStoppedByUser, 	placeholders::_1, placeholders::_2,
			// placeholders::_3, placeholders::_4);
			// request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
			// request.setOpt(new curlpp::options::NoProgress(0L));
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &progressCallback);
			curl_easy_setopt(curl, CURLOPT_XFERINFODATA, progressData);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

			SPDLOG_INFO(
				"{} details"
				"{}"
				", filePathName: {}"
				", sizeInBytes: {}"
				", ftpUrl: {}",
				api, referenceToLog, filePathName, sizeInBytes, ftpUrl
			);

			chrono::system_clock::time_point start = chrono::system_clock::now();
			// request.perform();
			CURLcode curlCode = curl_easy_perform(curl);
			chrono::system_clock::time_point end = chrono::system_clock::now();
			if (curlCode != CURLE_OK)
			{
				string errorMessage = fmt::format(
					"{}. curl_easy_perform failed"
					", curlCode message: {}"
					", ftpUrl: {}",
					api, curl_easy_strerror(curlCode), ftpUrl
				);
				SPDLOG_ERROR(errorMessage);

				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				throw runtime_error(errorMessage);
			}

			SPDLOG_INFO(
				"{} success"
				"{}"
				", @MMS statistics@ - elapsed (secs): @{}@",
				api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count()
			);

			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			break;
		}
		catch (runtime_error e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", ftpUrl: {}"
				", exception: {}",
				api, referenceToLog, ftpUrl, e.what()
			);

			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"{} failed (exception)"
				"{}"
				", ftpUrl: {}"
				", exception: {}",
				api, referenceToLog, ftpUrl, e.what()
			);

			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_INFO(
					"{}. Sleep before trying again"
					"{}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}
}

size_t emailPayloadFeed(void *ptr, size_t size, size_t nmemb, void *f)
{
	CurlWrapper::CurlUploadEmailData *curlUploadEmailData = (CurlWrapper::CurlUploadEmailData *)f;

	if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1))
	{
		return 0;
	}

	// Docs: Returning 0 will signal end-of-file to the library and cause it to
	// stop the current transfer
	if (curlUploadEmailData->emailLines.size() == 0)
		return 0; // no more lines

	string emailLine = curlUploadEmailData->emailLines.front();
	// cout << "emailLine: " << emailLine << endl;

	// logger->info(__FILEREF__ + "email line"
	// 	+ ", emailLine: " + emailLine
	// );

	memcpy(ptr, emailLine.c_str(), emailLine.length());
	curlUploadEmailData->emailLines.pop_front();

	return emailLine.length();
}

void CurlWrapper::sendEmail(
	string emailServerURL, // i.e.: smtps://smtppro.zoho.eu:465
	string from,		   // i.e.: info@catramms-cloud.com
	string tosCommaSeparated, string ccsCommaSeparated, string subject, vector<string> &emailBody,
	// 2023-02-18: usiamo 'from' come username perchè, mi è sembrato che ZOHO
	// blocca l'email
	//	se username e from sono diversi
	// string userName,	// i.e.: info@catramms-cloud.com
	string password
)
{
	// see: https://everything.curl.dev/usingcurl/smtp
	// curl --ssl-reqd --url 'smtps://smtppro.zoho.eu:465' --mail-from
	// 'info@catramms-cloud.com' --mail-rcpt 'giulianocatrambone@gmail.com'
	// --upload-file ./email.txt --user 'info@catramms-cloud.com:<write here the
	// password>'

	CurlUploadEmailData curlUploadEmailData;

	{
		// add From
		curlUploadEmailData.emailLines.push_back(string("From: <") + from + ">" + "\r\n");

		// add To
		{
			string addresses;

			stringstream ssAddresses(tosCommaSeparated);
			string address; // <email>,<email>,...
			char delim = ',';
			while (getline(ssAddresses, address, delim))
			{
				if (!address.empty())
				{
					if (addresses == "")
						addresses = string("<") + address + ">";
					else
						addresses += (string(", <") + address + ">");
				}
			}

			curlUploadEmailData.emailLines.push_back(string("To: ") + addresses + "\r\n");
		}

		// add Cc
		if (ccsCommaSeparated != "")
		{
			string addresses;

			stringstream ssAddresses(ccsCommaSeparated);
			string address; // <email>,<email>,...
			char delim = ',';
			while (getline(ssAddresses, address, delim))
			{
				if (!address.empty())
				{
					if (addresses == "")
						addresses = string("<") + address + ">";
					else
						addresses += (string(", <") + address + ">");
				}
			}

			curlUploadEmailData.emailLines.push_back(string("Cc: ") + addresses + "\r\n");
		}

		curlUploadEmailData.emailLines.push_back(string("Subject: ") + subject + "\r\n");
		curlUploadEmailData.emailLines.push_back(string("Content-Type: text/html; charset=\"UTF-8\"") + "\r\n");
		curlUploadEmailData.emailLines.push_back("\r\n"); // empty line to divide headers from body, see RFC5322
		curlUploadEmailData.emailLines.insert(curlUploadEmailData.emailLines.end(), emailBody.begin(), emailBody.end());
	}

	CURL *curl;
	CURLcode res = CURLE_OK;

	curl = curl_easy_init();
	if (!curl)
	{
		string errorMessage = "curl_easy_init failed";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	curl_easy_setopt(curl, CURLOPT_URL, emailServerURL.c_str());
	if (from != "")
		curl_easy_setopt(curl, CURLOPT_USERNAME, from.c_str());
	if (password != "")
		curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());

	// curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	// curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	/* Note that this option isn't strictly required, omitting it will
	 * result in libcurl sending the MAIL FROM command with empty sender
	 * data. All autoresponses should have an empty reverse-path, and should
	 * be directed to the address in the reverse-path which triggered them.
	 * Otherwise, they could cause an endless loop. See RFC 5321
	 * Section 4.5.5 for more details.
	 */
	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());

	/* Add two recipients, in this particular case they correspond to the
	 * To: and Cc: addressees in the header, but they could be any kind of
	 * recipient. */
	struct curl_slist *recipients = NULL;
	{
		{
			stringstream ssAddresses(tosCommaSeparated);
			string address;
			char delim = ',';
			while (getline(ssAddresses, address, delim))
			{
				if (!address.empty())
					recipients = curl_slist_append(recipients, address.c_str());
			}
		}
		if (ccsCommaSeparated != "")
		{
			stringstream ssAddresses(ccsCommaSeparated);
			string address;
			char delim = ',';
			while (getline(ssAddresses, address, delim))
			{
				if (!address.empty())
					recipients = curl_slist_append(recipients, address.c_str());
			}
		}

		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
	}

	curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

	/* We're using a callback function to specify the payload (the headers
	 * and body of the message). You could just use the CURLOPT_READDATA
	 * option to specify a FILE pointer to read from. */
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, emailPayloadFeed);
	curl_easy_setopt(curl, CURLOPT_READDATA, &curlUploadEmailData);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	// curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	/* Send the message */
	{
		string body;
		for (string emailLine : curlUploadEmailData.emailLines)
			body += emailLine;
		SPDLOG_INFO(
			"Sending email"
			", emailServerURL: {}"
			", from: {}"
			// + ", password: " + password
			", to: {}"
			", cc: {}"
			", subject: {}"
			", body: {}",
			emailServerURL, from, tosCommaSeparated, ccsCommaSeparated, subject, body
		);
	}

	res = curl_easy_perform(curl);

	if (res != CURLE_OK)
	{
		string errorMessage = curl_easy_strerror(res);
		SPDLOG_ERROR(
			"curl_easy_perform() failed"
			", curl_easy_strerror(res): {}",
			errorMessage
		);

		curl_slist_free_all(recipients);
		curl_easy_cleanup(curl);

		throw runtime_error(errorMessage);
	}

	SPDLOG_INFO("Email sent successful");

	/* Free the list of recipients */
	curl_slist_free_all(recipients);

	/* curl won't send the QUIT command until you call cleanup, so you
	 * should be able to re-use this connection for additional messages
	 * (setting CURLOPT_MAIL_FROM and CURLOPT_MAIL_RCPT as required, and
	 * calling curl_easy_perform() again. It may not be a good idea to keep
	 * the connection open for a very long time though (more than a few
	 * minutes may result in the server timing out the connection), and you
	 * do want to clean up in the end.
	 */
	curl_easy_cleanup(curl);
}
