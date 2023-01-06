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
#include "JSONUtils.h"
#include <sstream>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>

// https://everything.curl.dev/libcurl/callbacks/read
// https://github.com/chrisvana/curlpp_copy/blob/master/include/curlpp/Options.hpp
// https://curl.se/libcurl/c/CURLOPT_POST.html


Json::Value MMSCURL::httpGetJson(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string response = MMSCURL::httpGet(
		logger,
		ingestionJobKey,
		url,
		timeoutInSeconds,
		basicAuthenticationUser,
		basicAuthenticationPassword,
		maxRetryNumber,
		secondsToWaitBeforeToRetry);

	Json::Value jsonRoot = JSONUtils::toJson(ingestionJobKey, -1, response);

	return jsonRoot;
}

string MMSCURL::httpPostString(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string body,
	string contentType,	// i.e.: application/json
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string requestType = "POST";

	return MMSCURL::httpPostPutString(
		logger,
		ingestionJobKey,
		url,
		requestType,
		timeoutInSeconds,
		basicAuthenticationUser,
		basicAuthenticationPassword,
		body,
		contentType,	// i.e.: application/json
		maxRetryNumber,
		secondsToWaitBeforeToRetry
	);
}

string MMSCURL::httpPutString(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string body,
	string contentType,	// i.e.: application/json
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string requestType = "PUT";

	return MMSCURL::httpPostPutString(
		logger,
		ingestionJobKey,
		url,
		requestType,
		timeoutInSeconds,
		basicAuthenticationUser,
		basicAuthenticationPassword,
		body,
		contentType,	// i.e.: application/json
		maxRetryNumber,
		secondsToWaitBeforeToRetry
	);
}

Json::Value MMSCURL::httpPostStringAndGetJson(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string body,
	string contentType,	// i.e.: application/json
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string response = MMSCURL::httpPostString(
		logger,
		ingestionJobKey,
		url,
		timeoutInSeconds,
		basicAuthenticationUser,
		basicAuthenticationPassword,
		body,
		contentType,
		maxRetryNumber,
		secondsToWaitBeforeToRetry
	);

	Json::Value jsonRoot = JSONUtils::toJson(ingestionJobKey, -1, response);

	return jsonRoot;
}

Json::Value MMSCURL::httpPutStringAndGetJson(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string body,
	string contentType,	// i.e.: application/json
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string response = MMSCURL::httpPutString(
		logger,
		ingestionJobKey,
		url,
		timeoutInSeconds,
		basicAuthenticationUser,
		basicAuthenticationPassword,
		body,
		contentType,
		maxRetryNumber,
		secondsToWaitBeforeToRetry
	);

	Json::Value jsonRoot = JSONUtils::toJson(ingestionJobKey, -1, response);

	return jsonRoot;
}

string MMSCURL::httpPostFile(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string pathFileName,
	int64_t fileSizeInBytes,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string requestType = "POST";

	return MMSCURL::httpPostPutFile(
		logger,
		ingestionJobKey,
		url,
		requestType,
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

string MMSCURL::httpPutFile(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string pathFileName,
	int64_t fileSizeInBytes,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string requestType = "PUT";

	return MMSCURL::httpPostPutFile(
		logger,
		ingestionJobKey,
		url,
		requestType,
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

Json::Value MMSCURL::httpPostFileAndGetJson(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string pathFileName,
	int64_t fileSizeInBytes,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string response = MMSCURL::httpPostFile(
		logger,
		ingestionJobKey,
		url,
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

	Json::Value jsonRoot = JSONUtils::toJson(ingestionJobKey, -1, response);

	return jsonRoot;
}

Json::Value MMSCURL::httpPutFileAndGetJson(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string pathFileName,
	int64_t fileSizeInBytes,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string response = MMSCURL::httpPutFile(
		logger,
		ingestionJobKey,
		url,
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

	Json::Value jsonRoot = JSONUtils::toJson(ingestionJobKey, -1, response);

	return jsonRoot;
}

string MMSCURL::httpPostFileSplittingInChunks(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string pathFileName,
	int64_t fileSizeInBytes,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	int64_t chunkSize = 100 * 1000 * 1000;

	if (fileSizeInBytes <= chunkSize)
		return httpPostFile(
			logger,
			ingestionJobKey,
			url,
			timeoutInSeconds,
			basicAuthenticationUser,
			basicAuthenticationPassword,
			pathFileName,
			fileSizeInBytes,
			maxRetryNumber,
			secondsToWaitBeforeToRetry
		);

	int chunksNumber = fileSizeInBytes / chunkSize;
	if (fileSizeInBytes % chunkSize != 0)
		chunksNumber++;

	string lastHttpReturn;
	for(int chunkIndex = 0; chunkIndex < chunksNumber; chunkIndex++)
	{
		int64_t contentRangeStart = chunkIndex * chunkSize;
		int64_t contentRangeEnd_Excluded = chunkIndex + 1 < chunksNumber ?
			(chunkIndex + 1) * chunkSize :
			fileSizeInBytes;

		lastHttpReturn = httpPostFile(
			logger,
			ingestionJobKey,
			url,
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

	return lastHttpReturn;
}

string MMSCURL::httpPostFormData(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	long timeoutInSeconds,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string requestType = "POST";

	return MMSCURL::httpPostPutFormData(
		logger,
		ingestionJobKey,
		url,
		formData,
		requestType,
		timeoutInSeconds,
		maxRetryNumber,
		secondsToWaitBeforeToRetry
	);
}

string MMSCURL::httpPutFormData(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	long timeoutInSeconds,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string requestType = "PUT";

	return MMSCURL::httpPostPutFormData(
		logger,
		ingestionJobKey,
		url,
		formData,
		requestType,
		timeoutInSeconds,
		maxRetryNumber,
		secondsToWaitBeforeToRetry
	);
}

Json::Value MMSCURL::httpPostFormDataAndGetJson(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	long timeoutInSeconds,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string response = MMSCURL::httpPostFormData(
		logger,
		ingestionJobKey,
		url,
		formData,
		timeoutInSeconds,
		maxRetryNumber,
		secondsToWaitBeforeToRetry
	);

	Json::Value jsonRoot = JSONUtils::toJson(ingestionJobKey, -1, response);

	return jsonRoot;
}

Json::Value MMSCURL::httpPutFormDataAndGetJson(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	long timeoutInSeconds,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string response = MMSCURL::httpPutFormData(
		logger,
		ingestionJobKey,
		url,
		formData,
		timeoutInSeconds,
		maxRetryNumber,
		secondsToWaitBeforeToRetry
	);

	Json::Value jsonRoot = JSONUtils::toJson(ingestionJobKey, -1, response);

	return jsonRoot;
}

string MMSCURL::httpPostFileByFormData(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	long timeoutInSeconds,
	string pathFileName,
	int64_t fileSizeInBytes,
	string mediaContentType,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string requestType = "POST";

	return MMSCURL::httpPostPutFileByFormData(
		logger,
		ingestionJobKey,
		url,
		formData,
		requestType,
		timeoutInSeconds,
		pathFileName,
		fileSizeInBytes,
		mediaContentType,
		maxRetryNumber,
		secondsToWaitBeforeToRetry,
		contentRangeStart,
		contentRangeEnd_Excluded
	);
}

string MMSCURL::httpPutFileByFormData(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	long timeoutInSeconds,
	string pathFileName,
	int64_t fileSizeInBytes,
	string mediaContentType,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string requestType = "PUT";

	return MMSCURL::httpPostPutFileByFormData(
		logger,
		ingestionJobKey,
		url,
		formData,
		requestType,
		timeoutInSeconds,
		pathFileName,
		fileSizeInBytes,
		mediaContentType,
		maxRetryNumber,
		secondsToWaitBeforeToRetry,
		contentRangeStart,
		contentRangeEnd_Excluded
	);
}

Json::Value MMSCURL::httpPostFileByFormDataAndGetJson(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	long timeoutInSeconds,
	string pathFileName,
	int64_t fileSizeInBytes,
	string mediaContentType,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string response = MMSCURL::httpPostFileByFormData(
		logger,
		ingestionJobKey,
		url,
		formData,
		timeoutInSeconds,
		pathFileName,
		fileSizeInBytes,
		mediaContentType,
		maxRetryNumber,
		secondsToWaitBeforeToRetry,
		contentRangeStart,
		contentRangeEnd_Excluded
	);

	Json::Value jsonRoot = JSONUtils::toJson(ingestionJobKey, -1, response);

	return jsonRoot;
}

Json::Value MMSCURL::httpPutFileByFormDataAndGetJson(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	long timeoutInSeconds,
	string pathFileName,
	int64_t fileSizeInBytes,
	string mediaContentType,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string response = MMSCURL::httpPutFileByFormData(
		logger,
		ingestionJobKey,
		url,
		formData,
		timeoutInSeconds,
		pathFileName,
		fileSizeInBytes,
		mediaContentType,
		maxRetryNumber,
		secondsToWaitBeforeToRetry,
		contentRangeStart,
		contentRangeEnd_Excluded
	);

	Json::Value jsonRoot = JSONUtils::toJson(ingestionJobKey, -1, response);

	return jsonRoot;
}

size_t curlDownloadCallback(char* ptr, size_t size, size_t nmemb, void *f)
{
	MMSCURL::CurlDownloadData* curlDownloadData = (MMSCURL::CurlDownloadData*) f;
    
	auto logger = spdlog::get(curlDownloadData->loggerName);

	if (curlDownloadData->currentChunkNumber == 0)
	{
		(curlDownloadData->mediaSourceFileStream).open(
			curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::trunc);
		if (!curlDownloadData->mediaSourceFileStream)
		{
			string message = __FILEREF__ + "open file failed"
				+ ", ingestionJobKey: " + to_string(curlDownloadData->ingestionJobKey) 
				+ ", destBinaryPathName: " + curlDownloadData->destBinaryPathName
			;
			logger->error(message);

			// throw runtime_error(message);
		}
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
		if (curlDownloadData->mediaSourceFileStream)
			(curlDownloadData->mediaSourceFileStream).close();

		// (curlDownloadData->mediaSourceFileStream).open(localPathFileName, ios::binary | ios::out | ios::trunc);
		(curlDownloadData->mediaSourceFileStream).open(curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::app);
		if (!curlDownloadData->mediaSourceFileStream)
		{
			string message = __FILEREF__ + "open file failed"
				+ ", ingestionJobKey: " + to_string(curlDownloadData->ingestionJobKey) 
				+ ", destBinaryPathName: " + curlDownloadData->destBinaryPathName
			;
			logger->error(message);

			// throw runtime_error(message);
		}
		curlDownloadData->currentChunkNumber += 1;

		logger->info(__FILEREF__ + "Opening binary file"
			+ ", curlDownloadData->destBinaryPathName: " + curlDownloadData->destBinaryPathName
			+ ", curlDownloadData->currentChunkNumber: " + to_string(curlDownloadData->currentChunkNumber)
			+ ", curlDownloadData->currentTotalSize: " + to_string(curlDownloadData->currentTotalSize)
			+ ", curlDownloadData->maxChunkFileSize: " + to_string(curlDownloadData->maxChunkFileSize)
		);
	}

	if (curlDownloadData->mediaSourceFileStream)
		curlDownloadData->mediaSourceFileStream.write(ptr, size * nmemb);
	curlDownloadData->currentTotalSize += (size * nmemb);


	return size * nmemb;        
};

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

    if(currentFilePosition + (size * nmemb) <= curlUploadData->upToByte_Excluded)
        curlUploadData->mediaSourceFileStream.read(ptr, size * nmemb);
    else
        curlUploadData->mediaSourceFileStream.read(ptr,
			curlUploadData->upToByte_Excluded - currentFilePosition);

    int64_t charsRead = curlUploadData->mediaSourceFileStream.gcount();
    
	curlUploadData->payloadBytesSent += charsRead;

	// Docs: Returning 0 will signal end-of-file to the library and cause it to stop the current transfer
    return charsRead;        
};

size_t curlUploadFormDataCallback(char* ptr, size_t size, size_t nmemb, void *f)
{
	MMSCURL::CurlUploadFormData* curlUploadFormData = (MMSCURL::CurlUploadFormData*) f;

    auto logger = spdlog::get(curlUploadFormData->loggerName);

    int64_t currentFilePosition = curlUploadFormData->mediaSourceFileStream.tellg();

	// Docs: Returning 0 will signal end-of-file to the library and cause it to stop the current transfer
	if (curlUploadFormData->endOfFormDataSent && currentFilePosition == curlUploadFormData->upToByte_Excluded)
		return 0;

    if (!curlUploadFormData->formDataSent)
    {
        if (curlUploadFormData->formData.size() > size * nmemb)
        {
            logger->error(__FILEREF__ + "Not enougth memory!!!"
				+ ", curlUploadFormData->formDataSent: " + to_string(curlUploadFormData->formDataSent)
				+ ", curlUploadFormData->formData: " + curlUploadFormData->formData
				+ ", curlUploadFormData->endOfFormDataSent: " + to_string(curlUploadFormData->endOfFormDataSent)
				+ ", curlUploadFormData->endOfFormData: " + curlUploadFormData->endOfFormData
				+ ", curlUploadFormData->upToByte_Excluded: " + to_string(curlUploadFormData->upToByte_Excluded)
				+ ", curlUploadFormData->formData.size(): " + to_string(curlUploadFormData->formData.size())
				+ ", size * nmemb: " + to_string(size * nmemb)
            );

            return CURL_READFUNC_ABORT;
        }
        
        strcpy(ptr, curlUploadFormData->formData.c_str());
        
        curlUploadFormData->formDataSent = true;

		// this is not payload
		// curlUploadFormData->payloadBytesSent += curlUploadFormData->formData.size();

        // logger->info(__FILEREF__ + "First read"
		// 	+ ", curlUploadFormData->formData.size(): " + to_string(curlUploadFormData->formData.size())
		// 	+ ", curlUploadFormData->payloadBytesSent: " + to_string(curlUploadFormData->payloadBytesSent)
        // );
        
        return curlUploadFormData->formData.size();
    }
    else if (currentFilePosition == curlUploadFormData->upToByte_Excluded)
    {
        if (!curlUploadFormData->endOfFormDataSent)
        {
            if (curlUploadFormData->endOfFormData.size() > size * nmemb)
            {
                logger->error(__FILEREF__ + "Not enougth memory!!!"
					+ ", curlUploadFormData->formDataSent: " + to_string(curlUploadFormData->formDataSent)
					+ ", curlUploadFormData->formData: " + curlUploadFormData->formData
					+ ", curlUploadFormData->endOfFormDataSent: " + to_string(curlUploadFormData->endOfFormDataSent)
					+ ", curlUploadFormData->endOfFormData: " + curlUploadFormData->endOfFormData
					+ ", curlUploadFormData->upToByte_Excluded: " + to_string(curlUploadFormData->upToByte_Excluded)
					+ ", curlUploadFormData->endOfFormData.size(): " + to_string(curlUploadFormData->endOfFormData.size())
					+ ", size * nmemb: " + to_string(size * nmemb)
                );

                return CURL_READFUNC_ABORT;
            }

            strcpy(ptr, curlUploadFormData->endOfFormData.c_str());

            curlUploadFormData->endOfFormDataSent = true;

			// this is not payload
			// curlUploadFormData->payloadBytesSent += curlUploadFormData->endOfFormData.size();

            // logger->info(__FILEREF__ + "Last read"
			// 	+ ", curlUploadFormData->endOfFormData.size(): " + to_string(curlUploadFormData->endOfFormData.size())
			// 	+ ", curlUploadFormData->payloadBytesSent: " + to_string(curlUploadFormData->payloadBytesSent)
            // );

            return curlUploadFormData->endOfFormData.size();
        }
        else
        {
            logger->error(__FILEREF__ + "This scenario should never happen"
				+ ", curlUploadFormData->formDataSent: " + to_string(curlUploadFormData->formDataSent)
				+ ", curlUploadFormData->formData: " + curlUploadFormData->formData
				+ ", curlUploadFormData->endOfFormDataSent: " + to_string(curlUploadFormData->endOfFormDataSent)
				+ ", curlUploadFormData->endOfFormData: " + curlUploadFormData->endOfFormData
				+ ", curlUploadFormData->upToByte_Excluded: " + to_string(curlUploadFormData->upToByte_Excluded)
				+ ", curlUploadFormData->endOfFormData.size(): " + to_string(curlUploadFormData->endOfFormData.size())
            );

            return CURL_READFUNC_ABORT;
        }
    }

    if(currentFilePosition + (size * nmemb) <= curlUploadFormData->upToByte_Excluded)
        curlUploadFormData->mediaSourceFileStream.read(ptr, size * nmemb);
    else
        curlUploadFormData->mediaSourceFileStream.read(ptr,
			curlUploadFormData->upToByte_Excluded - currentFilePosition);

    int64_t charsRead = curlUploadFormData->mediaSourceFileStream.gcount();
    
	curlUploadFormData->payloadBytesSent += charsRead;

    // logger->info(__FILEREF__ + "curlUploadFormDataCallback"
    //     + ", currentFilePosition: " + to_string(currentFilePosition)
    //     + ", charsRead: " + to_string(charsRead)
	// 	+ ", curlUploadFormData->payloadBytesSent: " + to_string(curlUploadFormData->payloadBytesSent)
    // );

	// Docs: Returning 0 will signal end-of-file to the library and cause it to stop the current transfer
    return charsRead;        
};

string MMSCURL::httpGet(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string sResponse;
	int retryNumber = 0;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

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

			sResponse = response.str();
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

			// return sResponse;
			break;
		}
		catch (curlpp::LogicError & e) 
		{
			logger->error(__FILEREF__ + "httpGet failed (LogicError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url 
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (curlpp::RuntimeError & e) 
		{ 
			logger->error(__FILEREF__ + "httpGet failed (RuntimeError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url 
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
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

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
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

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
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

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return sResponse;
}

string MMSCURL::httpPostPutString(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	string requestType,	// POST or PUT
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string body,
	string contentType,	// i.e.: application/json
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string sResponse;
	int retryNumber = 0;

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

			logger->info(__FILEREF__ + "httpPostPutString (" + requestType + ")"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url
				+ ", body: " + body
			);
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

			// return sResponse;
			break;
		}
		catch (curlpp::LogicError & e) 
		{
			logger->error(__FILEREF__ + "httpPostPutString failed (LogicError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);
            
			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
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

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
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

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
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

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
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

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return sResponse;
}

string MMSCURL::httpPostPutFile(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	string requestType,	// POST or PUT
	long timeoutInSeconds,
	string basicAuthenticationUser,
	string basicAuthenticationPassword,
	string pathFileName,
	int64_t fileSizeInBytes,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{
	string sResponse;
	int retryNumber = 0;

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
				string message = __FILEREF__ + "open file failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
					+ ", pathFileName: " + pathFileName
				;
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
				// Content-Range: bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize

				contentLengthOrRangeHeader = string("Content-Range: bytes ")
					+ to_string(contentRangeStart) + "-" + to_string(contentRangeEnd_Excluded - 1)
					+ "/" + to_string(fileSizeInBytes)
				;
			}
			else
			{
				contentLengthOrRangeHeader = string("Content-Length: ") + to_string(fileSizeInBytes);
			}
			header.push_back(contentLengthOrRangeHeader);

			{
				// string userPasswordEncoded = Convert::base64_encode(_mmsAPIUser + ":" + _mmsAPIPassword);
				string userPasswordEncoded = Convert::base64_encode(basicAuthenticationUser
					+ ":" + basicAuthenticationPassword);
				string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

				header.push_back(basicAuthorization);
			}

			request.setOpt(new curlpp::options::CustomRequest(requestType));
			if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
				request.setOpt(new curlpp::options::PostFieldSizeLarge(
					contentRangeEnd_Excluded - contentRangeStart));
			else
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

			logger->info(__FILEREF__ + "httpPostPutFile (" + requestType + ")"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url
				+ ", contentLengthOrRangeHeader: " + contentLengthOrRangeHeader
				+ ", pathFileName: " + pathFileName
			);
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

			// return sResponse;
			break;
		}
		catch (curlpp::LogicError & e) 
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed (LogicError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", requestType: " + requestType
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);
            
			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (curlpp::RuntimeError & e) 
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed (RuntimeError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", requestType: " + requestType
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (runtime_error e)
		{
			if (responseInitialized
				&& response.str().find("502 Bad Gateway") != string::npos)
			{
				logger->error(__FILEREF__ + "Server is not reachable, is it down?"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", requestType: " + requestType
					+ ", url: " + url 
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				logger->error(__FILEREF__ + "httpPostPutFile failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
					+ ", requestType: " + requestType
					+ ", url: " + url
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", requestType: " + requestType
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return sResponse;
}

string MMSCURL::httpPostPutFormData(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	string requestType,	// POST or PUT
	long timeoutInSeconds,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	string sResponse;
	int retryNumber = 0;

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
			for(pair<string, string> data: formData)
			{
				sFormData += ("--" + boundary + endOfLine);
				sFormData += ("Content-Disposition: form-data; name=\""
					+ data.first + "\"" + endOfLine + endOfLine + data.second + endOfLine);
			}
			sFormData += ("--" + boundary + "--" + endOfLine + endOfLine);

			curlpp::Cleanup cleaner;
			curlpp::Easy request;

			list<string> headers;
			headers.push_back("Content-Type: multipart/form-data; boundary=\"" + boundary + "\"");

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
			request.setOpt(new curlpp::options::PostFields(sFormData));
			request.setOpt(new curlpp::options::PostFieldSize(sFormData.length()));

			request.setOpt(new curlpp::options::WriteStream(&response));

			logger->info(__FILEREF__ + "httpPostPutFile (" + requestType + ")"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url
				+ ", sFormData: " + sFormData
			);
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
				string message = __FILEREF__ + "httpPostPutFile success"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
					+ ", responseCode: " + to_string(responseCode) 
					+ ", @MMS statistics@ - elapsed (secs): @" + to_string(
						chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
					+ ", sFormData: " + sFormData
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

			// return sResponse;
			break;
		}
		catch (curlpp::LogicError & e) 
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed (LogicError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", requestType: " + requestType
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);
            
			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (curlpp::RuntimeError & e) 
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed (RuntimeError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", requestType: " + requestType
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (runtime_error e)
		{
			if (responseInitialized
				&& response.str().find("502 Bad Gateway") != string::npos)
			{
				logger->error(__FILEREF__ + "Server is not reachable, is it down?"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", requestType: " + requestType
					+ ", url: " + url 
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				logger->error(__FILEREF__ + "httpPostPutFile failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
					+ ", requestType: " + requestType
					+ ", url: " + url
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", requestType: " + requestType
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return sResponse;
}

string MMSCURL::httpPostPutFileByFormData(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	vector<pair<string, string>> formData,
	string requestType,	// POST or PUT
	long timeoutInSeconds,
	string pathFileName,
	int64_t fileSizeInBytes,
	string mediaContentType,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry,
	int64_t contentRangeStart,
	int64_t contentRangeEnd_Excluded
)
{

	string sResponse;
	int retryNumber = 0;

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
				string message = __FILEREF__ + "open file failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
					+ ", pathFileName: " + pathFileName
				;
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
				for(pair<string, string> data: formData)
				{
					curlUploadFormData.formData += ("--" + boundary + endOfLine);
					curlUploadFormData.formData += ("Content-Disposition: form-data; name=\""
						+ data.first + "\"" + endOfLine + endOfLine + data.second + endOfLine);
				}

				if (contentRangeStart >= 0 && contentRangeEnd_Excluded > 0)
				{
					curlUploadFormData.formData += ("--" + boundary + endOfLine);
					// 2023-01-06: il caricamento del video su facebook fallisce senza il campo filename
					curlUploadFormData.formData +=
						("Content-Disposition: form-data; name=\"video_file_chunk\"; filename=\""
							+ to_string(contentRangeStart) + "\""
						+ endOfLine + "Content-Type: " + mediaContentType
						+ endOfLine + "Content-Length: " + (to_string(contentRangeEnd_Excluded - contentRangeStart))
						+ endOfLine + endOfLine);
				}
				else
				{
					curlUploadFormData.formData += ("--" + boundary + endOfLine);
					curlUploadFormData.formData += ("Content-Disposition: form-data; name=\"source\""
						+ endOfLine + "Content-Type: " + mediaContentType
						+ endOfLine + "Content-Length: " + (to_string(fileSizeInBytes))
						+ endOfLine + endOfLine);
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
				postSize = (contentRangeEnd_Excluded - contentRangeStart)
					+ curlUploadFormData.formData.size()
					+ curlUploadFormData.endOfFormData.size()
				;
			else
				postSize = fileSizeInBytes
					+ curlUploadFormData.formData.size()
					+ curlUploadFormData.endOfFormData.size()
				;
			request.setOpt(new curlpp::options::PostFieldSizeLarge(postSize));

			list<string> header;

			// string acceptHeader = "Accept: */*";
			// header.push_back(acceptHeader);

			// string contentLengthHeader = "Content-Length: " + to_string(postSize);
			// header.push_back(contentLengthHeader);

			string contentTypeHeader = "Content-Type: multipart/form-data; boundary=\"" + boundary + "\"";
			header.push_back(contentTypeHeader);

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

			logger->info(__FILEREF__ + "httpPostPutFile (" + requestType + ")"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", url: " + url
				+ ", pathFileName: " + pathFileName
				+ ", postSize: " + to_string(postSize)
				+ ", curlUploadFormData.formData: " + curlUploadFormData.formData
				+ ", curlUploadFormData.endOfFormData: " + curlUploadFormData.endOfFormData
			);
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
				string message = __FILEREF__ + "httpPostPutFile success"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
					+ ", responseCode: " + to_string(responseCode) 
					+ ", @MMS statistics@ - elapsed (secs): @" + to_string(
						chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
					// + ", curlUploadFormData.formData: " + curlUploadFormData.formData
					// + ", curlUploadFormData.endOfFormData: " + curlUploadFormData.endOfFormData
					+ ", curlUploadFormData.payloadBytesSent: " + to_string(curlUploadFormData.payloadBytesSent)
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
					+ ", curlUploadFormData.formData: " + curlUploadFormData.formData
					+ ", curlUploadFormData.endOfFormData: " + curlUploadFormData.endOfFormData
					+ ", sResponse: " + sResponse
					+ ", responseCode: " + to_string(responseCode)
				;
				logger->error(message);

				throw runtime_error(message);
			}

			// return sResponse;
			break;
		}
		catch (curlpp::LogicError & e) 
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed (LogicError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", requestType: " + requestType
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);
            
			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (curlpp::RuntimeError & e) 
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed (RuntimeError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", requestType: " + requestType
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw runtime_error(e.what());
		}
		catch (runtime_error e)
		{
			if (responseInitialized
				&& response.str().find("502 Bad Gateway") != string::npos)
			{
				logger->error(__FILEREF__ + "Server is not reachable, is it down?"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", requestType: " + requestType
					+ ", url: " + url 
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw ServerNotReachable();
			}
			else
			{
				logger->error(__FILEREF__ + "httpPostPutFile failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
					+ ", requestType: " + requestType
					+ ", url: " + url
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				if (retryNumber < maxRetryNumber)
				{
					logger->info(__FILEREF__ + "sleep before trying again"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", retryNumber: " + to_string(retryNumber)
						+ ", maxRetryNumber: " + to_string(maxRetryNumber)
						+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
					);
					this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
				}
				else
					throw e;
			}
		}
		catch (exception e)
		{
			logger->error(__FILEREF__ + "httpPostPutFile failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", requestType: " + requestType
				+ ", url: " + url
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}

	return sResponse;
}

void MMSCURL::downloadFile(
	shared_ptr<spdlog::logger> logger,
	int64_t ingestionJobKey,
	string url,
	string destBinaryPathName,
	int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	int retryNumber = 0;

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

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
		catch (curlpp::RuntimeError & e) 
		{
			logger->error(__FILEREF__ + "Download failed (RuntimeError)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", url: " + url 
				+ ", exception: " + e.what()
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
		catch (runtime_error e)
		{
			logger->error(__FILEREF__ + "Download failed (runtime_error)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", url: " + url 
				+ ", exception: " + e.what()
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
		catch (exception e)
		{
			logger->error(__FILEREF__ + "Download failed (exception)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", url: " + url 
				+ ", exception: " + e.what()
			);

			if (retryNumber < maxRetryNumber)
			{
				logger->info(__FILEREF__ + "sleep before trying again"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", retryNumber: " + to_string(retryNumber)
					+ ", maxRetryNumber: " + to_string(maxRetryNumber)
					+ ", secondsToWaitBeforeToRetry: " + to_string(secondsToWaitBeforeToRetry)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry));
			}
			else
				throw e;
		}
	}
}

