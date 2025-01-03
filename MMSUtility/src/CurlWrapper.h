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
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"
#include <curl/curl.h>
#include <deque>
#include <fstream>
#include <vector>

using namespace std;

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename((char *)__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

struct ServerNotReachable : public exception
{
	char const *what() const throw() { return "Server not reachable"; };
};

class CurlWrapper
{

  public:
	struct CurlDownloadData
	{
		string referenceToLog;
		int currentChunkNumber;
		string destBinaryPathName;
		ofstream mediaSourceFileStream;
		size_t currentTotalSize;
		size_t maxChunkFileSize;
	};

	struct CurlUploadData
	{
		ifstream mediaSourceFileStream;

		int64_t payloadBytesSent;
		int64_t upToByte_Excluded;
	};

	struct CurlUploadFormData
	{
		ifstream mediaSourceFileStream;

		int64_t payloadBytesSent;
		int64_t upToByte_Excluded;

		bool formDataSent;
		string formData;

		bool endOfFormDataSent;
		string endOfFormData;
	};

	struct CurlUploadEmailData
	{
		deque<string> emailLines;
	};

	static void globalInitialize();

	static void globalTerminate();

	static string escape(const string &url);
	static string unescape(const string &url);

	static string httpGet(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, vector<string> otherHeaders,
		string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15
	);

	static json httpGetJson(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, vector<string> otherHeaders,
		string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15
	);

	static string httpDelete(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, vector<string> otherHeaders,
		string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15
	);

	static pair<string, string> httpPostString(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
		string contentType, // i.e.: application/json
		vector<string> otherHeaders, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15
	);

	static string httpPutString(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
		string contentType, // i.e.: application/json
		vector<string> otherHeaders, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15
	);

	static json httpPostStringAndGetJson(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
		string contentType, // i.e.: application/json
		vector<string> otherHeaders, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15
	);

	static json httpPutStringAndGetJson(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
		string contentType, // i.e.: application/json
		vector<string> otherHeaders, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15
	);

	static string httpPostFile(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
		int64_t fileSizeInBytes, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1, int64_t contentRangeEnd_Excluded = -1
	);

	static string httpPutFile(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
		int64_t fileSizeInBytes, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1, int64_t contentRangeEnd_Excluded = -1
	);

	static json httpPostFileAndGetJson(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
		int64_t fileSizeInBytes, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1, int64_t contentRangeEnd_Excluded = -1
	);

	static json httpPutFileAndGetJson(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
		int64_t fileSizeInBytes, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1, int64_t contentRangeEnd_Excluded = -1
	);

	static string httpPostFileSplittingInChunks(
		string url, long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName,
		int64_t fileSizeInBytes, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15
	);

	static string httpPostFormData(
		string url, vector<pair<string, string>> formData, long timeoutInSeconds, string referenceToLog = "", int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static string httpPutFormData(
		string url, vector<pair<string, string>> formData, long timeoutInSeconds, string referenceToLog = "", int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static json httpPostFormDataAndGetJson(
		string url, vector<pair<string, string>> formData, long timeoutInSeconds, string referenceToLog = "", int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static json httpPutFormDataAndGetJson(
		string url, vector<pair<string, string>> formData, long timeoutInSeconds, string referenceToLog = "", int maxRetryNumber = 0,
		int secondsToWaitBeforeToRetry = 15
	);

	static string httpPostFileByFormData(
		string url, vector<pair<string, string>> formData, long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes,
		string mediaContentType, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1, int64_t contentRangeEnd_Excluded = -1
	);

	static string httpPutFileByFormData(
		string url, vector<pair<string, string>> formData, long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes,
		string mediaContentType, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1, int64_t contentRangeEnd_Excluded = -1
	);

	static json httpPostFileByFormDataAndGetJson(
		string url, vector<pair<string, string>> formData, long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes,
		string mediaContentType, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1, int64_t contentRangeEnd_Excluded = -1
	);

	static json httpPutFileByFormDataAndGetJson(
		string url, vector<pair<string, string>> formData, long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes,
		string mediaContentType, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15,
		int64_t contentRangeStart = -1, int64_t contentRangeEnd_Excluded = -1
	);

	static void downloadFile(
		string url, string destBinaryPathName, function<int(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)> progressCallback,
		void *progressData, long downloadChunkSizeInMegaBytes = 500, string referenceToLog = "", int maxRetryNumber = 0, bool resumeActive = false,
		int secondsToWaitBeforeToRetry = 15
	);

	static void ftpFile(
		string filePathName, string fileName, int64_t sizeInBytes, string ftpServer, int ftpPort, string ftpUserName, string ftpPassword,
		string ftpRemoteDirectory, string ftpRemoteFileName, function<int(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)> progressCallback,
		void *progressData, string referenceToLog = "", int maxRetryNumber = 0, int secondsToWaitBeforeToRetry = 15
	);

	static void sendEmail(
		string emailServerURL, // i.e.: smtps://smtppro.zoho.eu:465
		string from,		   // i.e.: info@catramms-cloud.com
		string tosCommaSeparated, string ccsCommaSeparated, string subject, vector<string> &emailBody,
		// 2023-02-18: usiamo 'from' come username perchè, mi è sembrato che ZOHO blocca l'email
		//	se username e from sono diversi
		// string userName,	// i.e.: info@catramms-cloud.com
		string password
	);

  private:
	static pair<string, string> httpPostPutString(
		string url,
		string requestType, // POST or PUT
		long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string body,
		string contentType, // i.e.: application/json
		vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
	);

	static string httpPostPutFile(
		string url,
		string requestType, // POST or PUT
		long timeoutInSeconds, string basicAuthenticationUser, string basicAuthenticationPassword, string pathFileName, int64_t fileSizeInBytes,
		string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
	);

	static string httpPostPutFormData(
		string url, vector<pair<string, string>> formData,
		string requestType, // POST or PUT
		long timeoutInSeconds, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
	);

	static string httpPostPutFileByFormData(
		string url, vector<pair<string, string>> formData,
		string requestType, // POST or PUT
		long timeoutInSeconds, string pathFileName, int64_t fileSizeInBytes, string mediaContentType, string referenceToLog, int maxRetryNumber,
		int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
	);
};

#endif
