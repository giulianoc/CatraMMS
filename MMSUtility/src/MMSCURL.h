/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CURL.h
 * Author: giuliano
 *
 * Created on March 29, 2018, 6:27 AM
 */

#ifndef CURL_h
#define CURL_h

#include "spdlog/spdlog.h"
#include "json/json.h"
#include <fstream>

using namespace std;

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

struct ServerNotReachable: public exception {
	char const* what() const throw()
	{
		return "Server not reachable";
	};
};

class MMSCURL {
    
public:

    struct CurlDownloadData {
        int64_t		ingestionJobKey;
		string		loggerName;
        int         currentChunkNumber;
        string      destBinaryPathName;
        ofstream    mediaSourceFileStream;
        size_t      currentTotalSize;
        size_t      maxChunkFileSize;
    };

	struct CurlUploadData {
		string		loggerName;
		ifstream	mediaSourceFileStream;

		int64_t		lastByteSent;
		int64_t		upToByte_Excluded;
	};

	static string httpGet(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword
	);

	static Json::Value httpGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword
	);

	static string httpPostString(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType	// i.e.: application/json
	);

	static string httpPutString(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType	// i.e.: application/json
	);

	static Json::Value httpPostStringAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType	// i.e.: application/json
	);

	static Json::Value httpPutStringAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType	// i.e.: application/json
	);

	static string httpPostFile(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string pathFileName,
		int64_t fileSizeInBytes,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static string httpPutFile(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string pathFileName,
		int64_t fileSizeInBytes,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static Json::Value httpPostFileAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string pathFileName,
		int64_t fileSizeInBytes,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static Json::Value httpPutFileAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string pathFileName,
		int64_t fileSizeInBytes,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static string httpPostFileSplittingInChunks(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string pathFileName,
		int64_t fileSizeInBytes
	);

	static void downloadFile(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		string destBinaryPathName
	);

private:
	static string httpPostPutString(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		string requestType,	// POST or PUT
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType	// i.e.: application/json
	);

	static string httpPostPutFile(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		string requestType,	// POST or PUT
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string pathFileName,
		int64_t fileSizeInBytes,
		int64_t contentRangeStart,
		int64_t contentRangeEnd_Excluded
	);

};

#endif

