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

#include "MMSCURL.h"

#include "catralibraries/Convert.h"
#include <sstream>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>

size_t curlUploadCallback(char* ptr, size_t size, size_t nmemb, void *f)
{
	MMSCURL::CurlUploadData* curlUploadData = (MMSCURL::CurlUploadData*) f;

    auto logger = spdlog::get(curlUploadData->loggerName);

    int64_t currentFilePosition = curlUploadData->mediaSourceFileStream.tellg();

	/*
    logger->info(__FILEREF__ + "curlUploadCallback"
        + ", currentFilePosition: " + to_string(currentFilePosition)
        + ", size: " + to_string(size)
        + ", nmemb: " + to_string(nmemb)
        + ", curlUploadData->fileSizeInBytes: " + to_string(curlUploadData->fileSizeInBytes)
    );
	*/

    if(currentFilePosition + (size * nmemb) <= curlUploadData->fileSizeInBytes)
        curlUploadData->mediaSourceFileStream.read(ptr, size * nmemb);
    else
        curlUploadData->mediaSourceFileStream.read(ptr,
			curlUploadData->fileSizeInBytes - currentFilePosition);

    int64_t charsRead = curlUploadData->mediaSourceFileStream.gcount();
    
    return charsRead;        
};

// https://everything.curl.dev/libcurl/callbacks/read
// https://github.com/chrisvana/curlpp_copy/blob/master/include/curlpp/Options.hpp
// https://curl.se/libcurl/c/CURLOPT_POST.html

string MMSCURL::postPutFile(
	int64_t ingestionJobKey,
	string url,
	string requestType,	// POST or PUT
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string pathFileName,
	int64_t fileSizeInBytes,
	shared_ptr<spdlog::logger> logger
)
{

	ostringstream response;
	try
	{
		CurlUploadData curlUploadData;
		curlUploadData.loggerName = logger->name();
		curlUploadData.mediaSourceFileStream.open(pathFileName, ios::binary);
		curlUploadData.lastByteSent = -1;
		curlUploadData.fileSizeInBytes = fileSizeInBytes;

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

		header.push_back(string("Content-Length: ") + to_string(fileSizeInBytes));
		{
			// string userPasswordEncoded = Convert::base64_encode(_mmsAPIUser + ":" + _mmsAPIPassword);
			string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser
				+ ":" + basicAuthenticationPassword);
			string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

			header.push_back(basicAuthorization);
		}

		request.setOpt(new curlpp::options::CustomRequest(requestType));
		request.setOpt(new curlpp::options::PostFieldSizeLarge(fileSizeInBytes));

		// Setting the URL to retrive.
		request.setOpt(new curlpp::options::Url(url));

		// timeout consistent with nginx configuration (fastcgi_read_timeout)
		request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));

		if (url == "https")
		{
			bool bSslVerifyPeer = false;
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
			request.setOpt(sslVerifyPeer);
              
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
			request.setOpt(sslVerifyHost);
		}

		request.setOpt(new curlpp::options::HttpHeader(header));

		request.setOpt(new curlpp::options::WriteStream(&response));

		logger->info(__FILEREF__ + "postPutFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", url: " + url
			+ ", pathFileName: " + pathFileName
		);
		chrono::system_clock::time_point start = chrono::system_clock::now();
		request.perform();
		chrono::system_clock::time_point end = chrono::system_clock::now();

		(curlUploadData.mediaSourceFileStream).close();

		string sResponse = response.str();
		// LF and CR create problems to the json parser...
		while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
			sResponse.pop_back();

		long responseCode = curlpp::infos::ResponseCode::get(request);
		if (responseCode == 201)
		{
			string message = __FILEREF__ + "postPutFile success"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", responseCode: " + to_string(responseCode) 
				+ ", @MMS statistics@ - elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
				+ ", sResponse: " + sResponse
			;
			logger->info(message);
		}
		else
		{
			string message = __FILEREF__ + "postPutFile failed, wrong return status"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", @MMS statistics@ - elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
				+ ", sResponse: " + sResponse
				+ ", responseCode: " + to_string(responseCode)
			;
			logger->error(message);

			throw runtime_error(message);
		}

		return sResponse;
	}
	catch (curlpp::LogicError & e) 
	{
		logger->error(__FILEREF__ + "postPutFile failed (LogicError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);
            
		throw runtime_error(e.what());
	}
	catch (curlpp::RuntimeError & e) 
	{
		logger->error(__FILEREF__ + "postPutFile failed (RuntimeError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw runtime_error(e.what());
	}
	catch (runtime_error e)
	{
		logger->error(__FILEREF__ + "postPutFile failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
	catch (exception e)
	{
		logger->error(__FILEREF__ + "postPutFile failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
}

string MMSCURL::postPutString(
	int64_t ingestionJobKey,
	string url,
	string requestType,	// POST or PUT
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string body,
	string contentType,	// i.e.: application/json
	shared_ptr<spdlog::logger> logger
)
{

	ostringstream response;
	try
	{
		curlpp::Cleanup cleaner;
		curlpp::Easy request;

		list<string> headers;

		headers.push_back(string("Content-Type: ") + contentType);
		{
			// string userPasswordEncoded = Convert::base64_encode(_mmsAPIUser + ":" + _mmsAPIPassword);
			string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser
				+ ":" + basicAuthenticationPassword);
			string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

			headers.push_back(basicAuthorization);
		}

		request.setOpt(new curlpp::options::Url(url));

		// timeout consistent with nginx configuration (fastcgi_read_timeout)
		request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));

		if (url == "https")
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

		logger->info(__FILEREF__ + "postPutString"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", url: " + url
			+ ", body: " + body
		);
		chrono::system_clock::time_point start = chrono::system_clock::now();
		request.perform();
		chrono::system_clock::time_point end = chrono::system_clock::now();

		string sResponse = response.str();
		// LF and CR create problems to the json parser...
		while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
			sResponse.pop_back();

		long responseCode = curlpp::infos::ResponseCode::get(request);
		if (responseCode == 201)
		{
			string message = __FILEREF__ + "postPutString"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", @MMS statistics@ - elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
				+ ", sResponse: " + sResponse
			;
			logger->info(message);
		}
		else
		{
			string message = __FILEREF__ + "postPutString failed, wrong return status"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", @MMS statistics@ - elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
				+ ", sResponse: " + sResponse
				+ ", responseCode: " + to_string(responseCode)
			;
			logger->error(message);

			throw runtime_error(message);
		}

		return sResponse;
	}
	catch (curlpp::LogicError & e) 
	{
		logger->error(__FILEREF__ + "postPutString failed (LogicError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);
            
		throw runtime_error(e.what());
	}
	catch (curlpp::RuntimeError & e) 
	{
		logger->error(__FILEREF__ + "postPutString failed (RuntimeError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw runtime_error(e.what());
	}
	catch (runtime_error e)
	{
		logger->error(__FILEREF__ + "postPutString failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
	catch (exception e)
	{
		logger->error(__FILEREF__ + "postPutString failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
}

