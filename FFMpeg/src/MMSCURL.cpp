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

// https://everything.curl.dev/libcurl/callbacks/read
// https://github.com/chrisvana/curlpp_copy/blob/master/include/curlpp/Options.hpp
// https://curl.se/libcurl/c/CURLOPT_POST.html

string MMSCURL::httpGet(
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	shared_ptr<spdlog::logger> logger
)
{
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		curlpp::Cleanup cleaner;
		curlpp::Easy request;

        list<string> header;

		{
			string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser + ":"
				+ basicAuthenticationPassword);
			string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

			header.push_back(basicAuthorization);
		}

		request.setOpt(new curlpp::options::Url(url));

		// timeout consistent with nginx configuration (fastcgi_read_timeout)
		request.setOpt(new curlpp::options::Timeout(timeoutInSeconds));

		string httpsPrefix("https");
		if (url.size() >= httpsPrefix.size()
			&& 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
        {
			/*
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
			typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
			typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
			typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
			typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
			typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
			typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
			typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
			*/


			/*
			// cert is stored PEM coded in file... 
			// since PEM is default, we needn't set it for PEM 
			// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
			curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
			equest.setOpt(sslCertType);

			// set the cert for client authentication
			// "testcert.pem"
			// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
			curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
			request.setOpt(sslCert);
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
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
			request.setOpt(sslVerifyPeer);

			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
			request.setOpt(sslVerifyHost);

			// request.setOpt(new curlpp::options::SslEngineDefault());                                              
		}

		request.setOpt(new curlpp::options::HttpHeader(header));

		request.setOpt(new curlpp::options::WriteStream(&response));

		logger->info(__FILEREF__ + "HTTP GET"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", url: " + url
		);

		responseInitialized = true;
		chrono::system_clock::time_point start = chrono::system_clock::now();
		request.perform();
		chrono::system_clock::time_point end = chrono::system_clock::now();

		string sResponse = response.str();
		// LF and CR create problems to the json parser...
		while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
			sResponse.pop_back();

		long responseCode = curlpp::infos::ResponseCode::get(request);
		if (responseCode == 200)
		{
			string message = __FILEREF__ + "httpGet"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", @MMS statistics@ - elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
				+ ", sResponse: " + sResponse
			;
			logger->info(message);
		}
		else
		{
			string message = __FILEREF__ + "httpGet failed, wrong return status"
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
        logger->error(__FILEREF__ + "httpGet failed (LogicError)"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", url: " + url 
            + ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
        );

        throw e;
    }
    catch (curlpp::RuntimeError & e) 
    { 
        string errorMessage = string("httpGet failed (RuntimeError)")
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", url: " + url 
            + ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
        ;

        logger->error(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);
    }
    catch (runtime_error e)
    {
		if (responseInitialized
			&& response.str().find("502 Bad Gateway") != string::npos)
		{
			logger->error(__FILEREF__ + "Server is not reachable, is it down?"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url 
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			throw ServerNotReachable();
		}
		else
		{
			logger->error(__FILEREF__ + "httpGet failed (exception)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url 
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			throw e;
		}
    }
    catch (exception e)
    {
        logger->error(__FILEREF__ + "httpGet failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", url: " + url 
            + ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
        );

        throw e;
    }
}

string MMSCURL::httpPostPutString(
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
	bool responseInitialized = false;
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

		string httpsPrefix("https");
		if (url.size() >= httpsPrefix.size()
			&& 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
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

		logger->info(__FILEREF__ + "httpPostPutString"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", url: " + url
			+ ", body: " + body
		);
		responseInitialized = true;
		chrono::system_clock::time_point start = chrono::system_clock::now();
		request.perform();
		chrono::system_clock::time_point end = chrono::system_clock::now();

		string sResponse = response.str();
		// LF and CR create problems to the json parser...
		while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
			sResponse.pop_back();

		long responseCode = curlpp::infos::ResponseCode::get(request);
		if (responseCode == 200 || responseCode == 201)
		{
			string message = __FILEREF__ + "httpPostPutString"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", @MMS statistics@ - elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
				+ ", sResponse: " + sResponse
			;
			logger->info(message);
		}
		else
		{
			string message = __FILEREF__ + "httpPostPutString failed, wrong return status"
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
		logger->error(__FILEREF__ + "httpPostPutString failed (LogicError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);
            
		throw runtime_error(e.what());
	}
	catch (curlpp::RuntimeError & e) 
	{
		logger->error(__FILEREF__ + "httpPostPutString failed (RuntimeError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw runtime_error(e.what());
	}
	catch (runtime_error e)
	{
		if (responseInitialized
			&& response.str().find("502 Bad Gateway") != string::npos)
		{
			logger->error(__FILEREF__ + "Server is not reachable, is it down?"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url 
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			throw ServerNotReachable();
		}
		else
		{
			logger->error(__FILEREF__ + "httpPostPutString failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			throw e;
		}
	}
	catch (exception e)
	{
		logger->error(__FILEREF__ + "httpPostPutString failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
}

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

string MMSCURL::httpPostPutFile(
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
	bool responseInitialized = false;
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

		string httpsPrefix("https");
		if (url.size() >= httpsPrefix.size()
			&& 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
		{
			bool bSslVerifyPeer = false;
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
			request.setOpt(sslVerifyPeer);
              
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
			request.setOpt(sslVerifyHost);
		}

		request.setOpt(new curlpp::options::HttpHeader(header));

		request.setOpt(new curlpp::options::WriteStream(&response));

		logger->info(__FILEREF__ + "httpPostPutFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", url: " + url
			+ ", pathFileName: " + pathFileName
		);
		responseInitialized = true;
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
			string message = __FILEREF__ + "httpPostPutFile success"
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
			string message = __FILEREF__ + "httpPostPutFile failed, wrong return status"
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
		logger->error(__FILEREF__ + "httpPostPutFile failed (LogicError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);
            
		throw runtime_error(e.what());
	}
	catch (curlpp::RuntimeError & e) 
	{
		logger->error(__FILEREF__ + "httpPostPutFile failed (RuntimeError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw runtime_error(e.what());
	}
	catch (runtime_error e)
	{
		if (responseInitialized
			&& response.str().find("502 Bad Gateway") != string::npos)
		{
			logger->error(__FILEREF__ + "Server is not reachable, is it down?"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url 
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			throw ServerNotReachable();
		}
		else
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			throw e;
		}
	}
	catch (exception e)
	{
		logger->error(__FILEREF__ + "httpPostPutFile failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
}

size_t curlDownloadCallback(char* ptr, size_t size, size_t nmemb, void *f)
{
	MMSCURL::CurlDownloadData* curlDownloadData = (MMSCURL::CurlDownloadData*) f;
    
	auto logger = spdlog::get(curlDownloadData->loggerName);

	if (curlDownloadData->currentChunkNumber == 0)
	{
		(curlDownloadData->mediaSourceFileStream).open(
			curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::trunc);
		curlDownloadData->currentChunkNumber += 1;
        
		logger->info(__FILEREF__ + "Opening binary file"
			+ ", curlDownloadData -> destBinaryPathName: " + curlDownloadData -> destBinaryPathName
			+ ", curlDownloadData->currentChunkNumber: " + to_string(curlDownloadData->currentChunkNumber)
			+ ", curlDownloadData->currentTotalSize: " + to_string(curlDownloadData->currentTotalSize)
			+ ", curlDownloadData->maxChunkFileSize: " + to_string(curlDownloadData->maxChunkFileSize)
		);
	}
	else if (curlDownloadData->currentTotalSize >= 
		curlDownloadData->currentChunkNumber * curlDownloadData->maxChunkFileSize)
	{
		(curlDownloadData->mediaSourceFileStream).close();

		// (curlDownloadData->mediaSourceFileStream).open(localPathFileName, ios::binary | ios::out | ios::trunc);
		(curlDownloadData->mediaSourceFileStream).open(curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::app);
		curlDownloadData->currentChunkNumber += 1;

		logger->info(__FILEREF__ + "Opening binary file"
			+ ", curlDownloadData->destBinaryPathName: " + curlDownloadData->destBinaryPathName
			+ ", curlDownloadData->currentChunkNumber: " + to_string(curlDownloadData->currentChunkNumber)
			+ ", curlDownloadData->currentTotalSize: " + to_string(curlDownloadData->currentTotalSize)
			+ ", curlDownloadData->maxChunkFileSize: " + to_string(curlDownloadData->maxChunkFileSize)
		);
	}

	curlDownloadData->mediaSourceFileStream.write(ptr, size * nmemb);
	curlDownloadData->currentTotalSize += (size * nmemb);


	return size * nmemb;        
};

void MMSCURL::downloadFile(
	int64_t ingestionJobKey,
	string url,
	string destBinaryPathName,
	shared_ptr<spdlog::logger> logger
)
{
	try 
	{
		long downloadChunkSizeInMegaBytes = 500;


		CurlDownloadData curlDownloadData;
		curlDownloadData.loggerName = logger->name();
		curlDownloadData.currentChunkNumber = 0;
		curlDownloadData.currentTotalSize = 0;
		curlDownloadData.destBinaryPathName   = destBinaryPathName;
		curlDownloadData.maxChunkFileSize    = downloadChunkSizeInMegaBytes * 1000000;

		// fstream mediaSourceFileStream(destBinaryPathName, ios::binary | ios::out);
		// mediaSourceFileStream.exceptions(ios::badbit | ios::failbit);   // setting the exception mask
		// FILE *mediaSourceFileStream = fopen(destBinaryPathName.c_str(), "wb");

		curlpp::Cleanup cleaner;
		curlpp::Easy request;

		// Set the writer callback to enable cURL 
		// to write result in a memory area
		// request.setOpt(new curlpp::options::WriteStream(&mediaSourceFileStream));
             
		// which timeout we have to use here???
		// request.setOpt(new curlpp::options::Timeout(curlTimeoutInSeconds));

		curlpp::options::WriteFunctionCurlFunction curlDownloadCallbackFunction(curlDownloadCallback);
		curlpp::OptionTrait<void *, CURLOPT_WRITEDATA> curlDownloadDataData(&curlDownloadData);
		request.setOpt(curlDownloadCallbackFunction);
		request.setOpt(curlDownloadDataData);

		// Setting the URL to retrive.
		request.setOpt(new curlpp::options::Url(url));
		string httpsPrefix("https");
		if (url.size() >= httpsPrefix.size()
			&& 0 == url.compare(0, httpsPrefix.size(), httpsPrefix))
		{
			// disconnect if we can't validate server's cert
			bool bSslVerifyPeer = false;
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
			request.setOpt(sslVerifyPeer);

			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
			request.setOpt(sslVerifyHost);
		}

		// chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
		// double lastPercentageUpdated = -1.0;
		// curlpp::types::ProgressFunctionFunctor functor = bind(&MMSEngineProcessor::progressDownloadCallback, this,
		// 	ingestionJobKey, lastProgressUpdate, lastPercentageUpdated, downloadingStoppedByUser,
		// 	placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
		// request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
		// request.setOpt(new curlpp::options::NoProgress(0L));

		logger->info(__FILEREF__ + "Downloading media file"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", url: " + url
			+ ", destBinaryPathName: " + destBinaryPathName
		);
		chrono::system_clock::time_point start = chrono::system_clock::now();
		request.perform();
		chrono::system_clock::time_point end = chrono::system_clock::now();

		(curlDownloadData.mediaSourceFileStream).close();

		string message = __FILEREF__ + "download finished"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", @MMS statistics@ - elapsed (secs): @" + to_string(
				chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
		;
		logger->info(message);
	}
	catch (curlpp::LogicError & e) 
	{
		logger->error(__FILEREF__ + "Download failed (LogicError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url 
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{
		logger->error(__FILEREF__ + "Download failed (RuntimeError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url 
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (runtime_error e)
	{
		logger->error(__FILEREF__ + "Download failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url 
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		logger->error(__FILEREF__ + "Download failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", url: " + url 
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

