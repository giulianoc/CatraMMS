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

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <fstream>
#include <deque>
#include <vector>

using namespace std;

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;


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

		int64_t		payloadBytesSent;
		int64_t		upToByte_Excluded;
	};

	struct CurlUploadFormData {
		string		loggerName;
		ifstream	mediaSourceFileStream;

		int64_t		payloadBytesSent;
		int64_t		upToByte_Excluded;

		bool		formDataSent;
		string		formData;

		bool		endOfFormDataSent;
		string		endOfFormData;
	};

	struct CurlUploadEmailData {
		deque<string>	emailLines;
	};

	static string httpGet(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		vector<string> otherHeaders,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static json httpGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		vector<string> otherHeaders,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static string httpDelete(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		vector<string> otherHeaders,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static pair<string, string> httpPostString(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType,	// i.e.: application/json
		vector<string> otherHeaders,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static string httpPutString(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType,	// i.e.: application/json
		vector<string> otherHeaders,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static json httpPostStringAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType,	// i.e.: application/json
		vector<string> otherHeaders,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static json httpPutStringAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType,	// i.e.: application/json
		vector<string> otherHeaders,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
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
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15,
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
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static json httpPostFileAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string pathFileName,
		int64_t fileSizeInBytes,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static json httpPutFileAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string pathFileName,
		int64_t fileSizeInBytes,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15,
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
		int64_t fileSizeInBytes,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static string httpPostFormData(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		vector<pair<string, string>> formData,
		long timeoutInSeconds,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static string httpPutFormData(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		vector<pair<string, string>> formData,
		long timeoutInSeconds,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static json httpPostFormDataAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		vector<pair<string, string>> formData,
		long timeoutInSeconds,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static json httpPutFormDataAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		vector<pair<string, string>> formData,
		long timeoutInSeconds,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static string httpPostFileByFormData(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		vector<pair<string, string>> formData,
		long timeoutInSeconds,
		string pathFileName,
		int64_t fileSizeInBytes,
		string mediaContentType,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static string httpPutFileByFormData(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		vector<pair<string, string>> formData,
		long timeoutInSeconds,
		string pathFileName,
		int64_t fileSizeInBytes,
		string mediaContentType,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static json httpPostFileByFormDataAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		vector<pair<string, string>> formData,
		long timeoutInSeconds,
		string pathFileName,
		int64_t fileSizeInBytes,
		string mediaContentType,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static json httpPutFileByFormDataAndGetJson(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		vector<pair<string, string>> formData,
		long timeoutInSeconds,
		string pathFileName,
		int64_t fileSizeInBytes,
		string mediaContentType,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1,
		int64_t contentRangeEnd_Excluded = -1
	);

	static void downloadFile(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		string destBinaryPathName,
		curlpp::types::ProgressFunctionFunctor functor,
		int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static void ftpFile(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string filePathName,
		string fileName,
		int64_t sizeInBytes,
		string ftpServer,
		int ftpPort,
		string ftpUserName,
		string ftpPassword,
		string ftpRemoteDirectory,
		string ftpRemoteFileName,
		curlpp::types::ProgressFunctionFunctor functor
	);

	static void sendEmail(
		string emailServerURL,	// i.e.: smtps://smtppro.zoho.eu:465
		string from,	// i.e.: info@catramms-cloud.com
		string tosCommaSeparated,
		string ccsCommaSeparated,
		string subject,
		vector<string>& emailBody,
		// 2023-02-18: usiamo 'from' come username perchè, mi è sembrato che ZOHO blocca l'email
		//	se username e from sono diversi
		// string userName,	// i.e.: info@catramms-cloud.com
		string password
	);

private:
	static pair<string, string> httpPostPutString(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		string requestType,	// POST or PUT
		long timeoutInSeconds,
		string basicAuthenticationUser,
		string basicAuthenticationPassword,
		string body,
		string contentType,	// i.e.: application/json
		vector<string> otherHeaders,
		int maxRetryNumber,
		int secondsToWaitBeforeToRetry
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
		int maxRetryNumber,
		int secondsToWaitBeforeToRetry,
		int64_t contentRangeStart,
		int64_t contentRangeEnd_Excluded
	);

	static string httpPostPutFormData(
		shared_ptr<spdlog::logger> logger,
		int64_t ingestionJobKey,
		string url,
		vector<pair<string, string>> formData,
		string requestType,	// POST or PUT
		long timeoutInSeconds,
		int maxRetryNumber,
		int secondsToWaitBeforeToRetry
	);

	static string httpPostPutFileByFormData(
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
	);
};

#endif

