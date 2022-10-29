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
		int64_t		fileSizeInBytes;        
	};

	static string httpGet(
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		shared_ptr<spdlog::logger> logger
	);

	static string httpPostPutFile(
		int64_t ingestionJobKey,
		string url,
		string requestType,	// POST or PUT
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string pathFileName,
		int64_t fileSizeInBytes,
		shared_ptr<spdlog::logger> logger
	);

	static string httpPostPutString(
		int64_t ingestionJobKey,
		string url,
		string requestType,	// POST or PUT
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType,	// i.e.: application/json
		shared_ptr<spdlog::logger> logger
	);

	static void downloadFile(
		int64_t ingestionJobKey,
		string url,
		string destBinaryPathName,
		shared_ptr<spdlog::logger> logger
	);
};

#endif

