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

#ifdef COMPRESSOR
#include "catralibraries/Compressor.h"
#endif
#include "JSONUtils.h"
// #include "StringUtils.h"
#include "catralibraries/Convert.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <curl/curl.h>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <regex>
#include <sstream>

#include <list>
#include <stdexcept>

namespace fs = std::filesystem;

// https://everything.curl.dev/libcurl/callbacks/read
// https://github.com/chrisvana/curlpp_copy/blob/master/include/curlpp/Options.hpp
// https://curl.se/libcurl/c/CURLOPT_POST.html

void CurlWrapper::globalInitialize() { curl_global_init(CURL_GLOBAL_DEFAULT); }

void CurlWrapper::globalTerminate() { curl_global_cleanup(); }

string CurlWrapper::basicAuthorization(const string &user, const string &password)
{
	if (user != "")
		return std::format("Basic {}", Convert::base64_encode(user + ":" + password));
	else
		return "";
}

string CurlWrapper::bearerAuthorization(const string &bearerToken)
{
	if (bearerToken != "")
		return std::format("Bearer {}", bearerToken);
	else
		return "";
}

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
	string url, long timeoutInSeconds, string authorization, vector<string> otherHeaders, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry, bool outputCompressed
)
{
#ifdef COMPRESSOR
	if (outputCompressed)
	{
		vector<uint8_t> binary;
		CurlWrapper::httpGetBinary(
			url, timeoutInSeconds, authorization, otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry, binary
		);
		return JSONUtils::toJson(Compressor::decompress_string(binary));
	}
	else
#endif
	{
		string response =
			CurlWrapper::httpGet(url, timeoutInSeconds, authorization, otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry);
		return JSONUtils::toJson(response);
	}
}

string CurlWrapper::httpPostString(
	string url, long timeoutInSeconds, string authorization, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, bool outputCompressed
)
{
	string requestType = "POST";

#ifdef COMPRESSOR
	if (outputCompressed)
	{
		vector<uint8_t> binary;
		CurlWrapper::httpPostPutBinary(
			url, requestType, timeoutInSeconds, authorization, body,
			contentType, // i.e.: application/json
			otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry, binary
		);
		return Compressor::decompress_string(binary);
	}
	else
#endif
	{
		return CurlWrapper::httpPostPutString(
				   url, requestType, timeoutInSeconds, authorization, body,
				   contentType, // i.e.: application/json
				   otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry
		)
			.second;
	}
}

string CurlWrapper::httpPutString(
	string url, long timeoutInSeconds, string authorization, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, bool outputCompressed
)
{
	string requestType = "PUT";

#ifdef COMPRESSOR
	if (outputCompressed)
	{
		vector<uint8_t> binary;
		CurlWrapper::httpPostPutBinary(
			url, requestType, timeoutInSeconds, authorization, body,
			contentType, // i.e.: application/json
			otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry, binary
		);
		return Compressor::decompress_string(binary);
	}
	else
#endif
	{
		return CurlWrapper::httpPostPutString(
				   url, requestType, timeoutInSeconds, authorization, body,
				   contentType, // i.e.: application/json
				   otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry
		)
			.second;
	}
}

pair<string, string> CurlWrapper::httpPostString(
	string url, long timeoutInSeconds, string authorization, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string requestType = "POST";

	return CurlWrapper::httpPostPutString(
		url, requestType, timeoutInSeconds, authorization, body,
		contentType, // i.e.: application/json
		otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry
	);
}

pair<string, string> CurlWrapper::httpPutString(
	string url, long timeoutInSeconds, string authorization, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	string requestType = "PUT";

	return CurlWrapper::httpPostPutString(
		url, requestType, timeoutInSeconds, authorization, body,
		contentType, // i.e.: application/json
		otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry
	);
}

json CurlWrapper::httpPostStringAndGetJson(
	string url, long timeoutInSeconds, string authorization, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, bool outputCompressed
)
{
	string response = CurlWrapper::httpPostString(
		url, timeoutInSeconds, authorization, body, contentType, otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry,
		outputCompressed
	);
	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

json CurlWrapper::httpPutStringAndGetJson(
	string url, long timeoutInSeconds, string authorization, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, bool outputCompressed
)
{
	string response = CurlWrapper::httpPutString(
		url, timeoutInSeconds, authorization, body, contentType, otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry,
		outputCompressed
	);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

string CurlWrapper::httpPostFile(
	string url, long timeoutInSeconds, string authorization, string pathFileName, int64_t fileSizeInBytes, string contentType, string referenceToLog,
	int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string requestType = "POST";

	return CurlWrapper::httpPostPutFile(
		url, requestType, timeoutInSeconds, authorization, pathFileName, fileSizeInBytes, contentType, referenceToLog, maxRetryNumber,
		secondsToWaitBeforeToRetry, contentRangeStart, contentRangeEnd_Excluded
	);
}

string CurlWrapper::httpPutFile(
	string url, long timeoutInSeconds, string authorization, string pathFileName, int64_t fileSizeInBytes, string contentType, string referenceToLog,
	int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string requestType = "PUT";

	return CurlWrapper::httpPostPutFile(
		url, requestType, timeoutInSeconds, authorization, pathFileName, fileSizeInBytes, contentType, referenceToLog, maxRetryNumber,
		secondsToWaitBeforeToRetry, contentRangeStart, contentRangeEnd_Excluded
	);
}

json CurlWrapper::httpPostFileAndGetJson(
	string url, long timeoutInSeconds, string authorization, string pathFileName, int64_t fileSizeInBytes, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string response = CurlWrapper::httpPostFile(
		url, timeoutInSeconds, authorization, pathFileName, fileSizeInBytes, "", referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry,
		contentRangeStart, contentRangeEnd_Excluded
	);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

json CurlWrapper::httpPutFileAndGetJson(
	string url, long timeoutInSeconds, string authorization, string pathFileName, int64_t fileSizeInBytes, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
)
{
	string response = CurlWrapper::httpPutFile(
		url, timeoutInSeconds, authorization, pathFileName, fileSizeInBytes, "", referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry,
		contentRangeStart, contentRangeEnd_Excluded
	);

	json jsonRoot = JSONUtils::toJson(response);

	return jsonRoot;
}

string CurlWrapper::httpPostFileSplittingInChunks(
	string url, long timeoutInSeconds, string authorization, string pathFileName, function<bool(int, int)> chunkCompleted, string referenceToLog,
	int maxRetryNumber, int secondsToWaitBeforeToRetry
)
{
	int64_t fileSizeInBytes = fs::file_size(pathFileName);

	int64_t chunkSize = 100 * 1000 * 1000;

	if (fileSizeInBytes <= chunkSize)
		return httpPostFile(
			url, timeoutInSeconds, authorization, pathFileName, fileSizeInBytes, "", referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry
		);

	int chunksNumber = fileSizeInBytes / chunkSize;
	if (fileSizeInBytes % chunkSize != 0)
		chunksNumber++;

	string lastHttpReturn;
	// stopped: il client deve settarla a true se vuole interrompere l'attività
	for (int chunkIndex = 0; chunkIndex < chunksNumber; chunkIndex++)
	{
		int64_t contentRangeStart = chunkIndex * chunkSize;
		int64_t contentRangeEnd_Excluded = chunkIndex + 1 < chunksNumber ? (chunkIndex + 1) * chunkSize : fileSizeInBytes;

		lastHttpReturn = httpPostFile(
			url, timeoutInSeconds, authorization, pathFileName, fileSizeInBytes, "", referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry,
			contentRangeStart, contentRangeEnd_Excluded
		);

		// chunkCompleted:
		// riceve l'îndice del chunk completato ed il numero totali di chunks
		// ritorna true se l'upload deve continuare, false se l'upload dete essere interrotto
		if (!chunkCompleted(chunkIndex, chunksNumber))
			break;
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

		SPDLOG_DEBUG(
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

		SPDLOG_DEBUG(
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

#ifdef _WIN32
		strcpy_s(ptr, size * nmemb, curlUploadFormData->formData.c_str());
#else
		strcpy(ptr, curlUploadFormData->formData.c_str());
#endif

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

#ifdef _WIN32
			strcpy_s(ptr, size * nmemb, curlUploadFormData->endOfFormData.c_str());
#else
			strcpy(ptr, curlUploadFormData->endOfFormData.c_str());
#endif

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

size_t curlWriteStringCallback(char *ptr, size_t size, size_t nmemb, void *f)
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
			"curlWriteStringCallback failed"
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

size_t curlWriteBytesCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
	try
	{
		vector<uint8_t> *buffer = (vector<uint8_t> *)f;

		buffer->insert(buffer->end(), ptr, ptr + (size * nmemb));

		return size * nmemb;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"curlWriteBytesCallback failed"
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
	string url, long timeoutInSeconds, string authorization, vector<string> otherHeaders, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry
)
{
	vector<uint8_t> binary;
	CurlWrapper::httpGetBinary(
		url, timeoutInSeconds, authorization, otherHeaders, referenceToLog, maxRetryNumber, secondsToWaitBeforeToRetry, binary
	);

	string response = string(binary.begin(), binary.end());

	while (response.size() > 0 && (response.back() == 10 || response.back() == 13))
		response.pop_back();

	return response;
}

void CurlWrapper::httpGetBinary(
	string url, long timeoutInSeconds, string authorization, vector<string> otherHeaders, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry, vector<uint8_t> &binary
)
{
	string api = "httpGet";

	for (int retryNumber = 0; retryNumber <= maxRetryNumber; retryNumber++)
	{
		CURL *curl = nullptr;
		struct curl_slist *headersList = nullptr;

		try
		{
			try
			{
				// curlpp::Cleanup cleaner;
				// curlpp::Easy request;

				curl = curl_easy_init();
				if (!curl)
				{
					string errorMessage = std::format("{}. curl_easy_init failed", api);
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
					if (authorization != "")
						headersList = curl_slist_append(headersList, std::format("Authorization: {}", authorization).c_str());

					for (string header : otherHeaders)
						headersList = curl_slist_append(headersList, header.c_str());

					curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
				}

				// request.setOpt(new curlpp::options::WriteStream(&response));
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&binary);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteBytesCallback);

				SPDLOG_DEBUG(
					"{} details"
					"{}"
					", url: {}"
					", authorization: {}"
					", otherHeaders.size: {}",
					api, referenceToLog, url, authorization, otherHeaders.size()
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
					string errorMessage = std::format(
						"{}. curl_easy_perform failed"
						", curlCode: {}"
						", curlCode message: {}"
						", url: {}",
						api, static_cast<int>(curlCode), curl_easy_strerror(curlCode), url
					);
					// SPDLOG_ERROR(errorMessage);

					throw ServerNotReachable(errorMessage);
				}

				// sResponse = response.str();
				// LF and CR create problems to the json parser...
				// while (response.size() > 0 && (response.back() == 10 || response.back() == 13))
				// 	response.pop_back();

				// long responseCode = curlpp::infos::ResponseCode::get(request);
				long responseCode;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
				if (responseCode == 200)
				{
					SPDLOG_DEBUG(
						"{} success"
						"{}"
						", @statistics@ - elapsed (secs): @{}@",
						// ", response: {}",
						api, referenceToLog,
						chrono::duration_cast<chrono::seconds>(end - start).count() // , regex_replace(response, regex("\n"), " ")
					);
				}
				else
				{
					string errorMessage = std::format(
						"{} failed, wrong return status"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						// ", response: {}"
						", responseCode: {}",
						api, referenceToLog,
						chrono::duration_cast<chrono::seconds>(end - start).count(), // regex_replace(response, regex("\n"), " "),
						responseCode
					);

					throw HTTPError(responseCode, errorMessage);
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

				break;
			}
			catch (CurlException &e)
			{
				throw;
			}
			catch (exception &e)
			{
				throw CurlException(e.what());
			}
		}
		catch (CurlException &e)
		{
			SPDLOG_ERROR(e.what());

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

			if (e.type() == "HTTPError")
			{
				HTTPError *exception = dynamic_cast<HTTPError *>(&e);
				if (exception->httpErrorCode == 404)
					throw;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_WARN(
					"{} retry"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry * (retryNumber + 1)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry * (retryNumber + 1)));
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", e.type: {}"
					", exception: {}",
					// ", response.str(): {}",
					api, referenceToLog, url, timeoutInSeconds, e.type(), e.what() // , response
				);

				if (e.type() == "ServerNotReachable")
					throw;
				else if (e.type() == "HTTPError")
				{
					HTTPError *exception = dynamic_cast<HTTPError *>(&e);
					if (exception->httpErrorCode == 502)
						throw ServerNotReachable(e.what());
					else
						throw;
				}
				else
					throw;
			}
		}
	}
}

string CurlWrapper::httpDelete(
	string url, long timeoutInSeconds, string authorization, vector<string> otherHeaders, string referenceToLog, int maxRetryNumber,
	int secondsToWaitBeforeToRetry
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
			try
			{
				// curlpp::Cleanup cleaner;
				// curlpp::Easy request;

				curl = curl_easy_init();
				if (!curl)
				{
					string errorMessage = std::format("{}. curl_easy_init failed", api);
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
					if (authorization != "")
						headersList = curl_slist_append(headersList, std::format("Authorization: {}", authorization).c_str());

					for (string header : otherHeaders)
						headersList = curl_slist_append(headersList, header.c_str());

					curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
				}

				// request.setOpt(new curlpp::options::WriteStream(&response));
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteStringCallback);

				SPDLOG_DEBUG(
					"{} details"
					"{}"
					", url: {}"
					", authorization: {}"
					", otherHeaders.size: {}",
					api, referenceToLog, url, authorization, otherHeaders.size()
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
					string errorMessage = std::format(
						"{}. curl_easy_perform failed"
						", curlCode message: {}"
						", url: {}",
						api, curl_easy_strerror(curlCode), url
					);
					// SPDLOG_ERROR(errorMessage);

					throw ServerNotReachable(errorMessage);
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
					SPDLOG_DEBUG(
						"{} success"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", response: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " ")
					);
				}
				else
				{
					string errorMessage = std::format(
						"{} failed, wrong return status"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", response: {}"
						", responseCode: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " "),
						responseCode
					);

					throw HTTPError(responseCode, errorMessage);
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
			catch (CurlException &e)
			{
				throw;
			}
			catch (exception &e)
			{
				throw CurlException(e.what());
			}
		}
		catch (CurlException &e)
		{
			SPDLOG_ERROR(e.what());

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

			if (e.type() == "HTTPError")
			{
				HTTPError *exception = dynamic_cast<HTTPError *>(&e);
				if (exception->httpErrorCode == 404)
					throw;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_WARN(
					"{} retry"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry * (retryNumber + 1)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry * (retryNumber + 1)));
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), response
				);

				if (e.type() == "ServerNotReachable")
					throw;
				else if (e.type() == "HTTPError")
				{
					HTTPError *exception = dynamic_cast<HTTPError *>(&e);
					if (exception->httpErrorCode == 502)
						throw ServerNotReachable(e.what());
					else
						throw;
				}
				else
					throw;
			}
		}
	}

	return response;
}

pair<string, string> CurlWrapper::httpPostPutString(
	string url,
	string requestType, // POST or PUT
	long timeoutInSeconds, string authorization, string body,
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
			try
			{
				curl = curl_easy_init();
				if (!curl)
				{
					string errorMessage = std::format("{}. curl_easy_init failed", api);
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
						headersList = curl_slist_append(headersList, std::format("Content-Type: {}", contentType).c_str());
					if (authorization != "")
						headersList = curl_slist_append(headersList, std::format("Authorization: {}", authorization).c_str());

					for (string header : otherHeaders)
						headersList = curl_slist_append(headersList, header.c_str());

					curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
				}

				// request.setOpt(new curlpp::options::WriteStream(&response));
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&responseHeaderAndBody);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteStringCallback);

				// store response headers in the response, You simply have to set next option to prefix the header to the
				// normal body output.
				// request.setOpt(new curlpp::options::Header(true));
				curl_easy_setopt(curl, CURLOPT_HEADER, 1L);

				SPDLOG_DEBUG(
					"{} details"
					"{}"
					", url: {}"
					", contentType: {}"
					", authorization: {}"
					", otherHeaders.size: {}"
					", body: {}",
					api, referenceToLog, url, contentType, authorization, otherHeaders.size(), regex_replace(body, regex("\n"), " ")
				);

				chrono::system_clock::time_point start = chrono::system_clock::now();
				// request.perform();
				CURLcode curlCode = curl_easy_perform(curl);
				chrono::system_clock::time_point end = chrono::system_clock::now();
				if (curlCode != CURLE_OK)
				{
					string errorMessage = std::format(
						"{}. curl_easy_perform failed"
						", curlCode message: {}"
						", url: {}",
						api, curl_easy_strerror(curlCode), url
					);
					// SPDLOG_ERROR(errorMessage);

					throw ServerNotReachable(errorMessage);
				}

				// long responseCode = curlpp::infos::ResponseCode::get(request);
				long responseCode;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
				if (responseCode == 200 || responseCode == 201 || responseCode == 308 // permanently removed/redirect
				)
				{
					SPDLOG_DEBUG(
						"{} success"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", responseHeaderAndBody: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), responseHeaderAndBody
					);
				}
				else
				{
					string errorMessage = std::format(
						"{} failed, wrong return status"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", responseHeaderAndBody: {}"
						", responseCode: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), responseHeaderAndBody, responseCode
					);

					throw HTTPError(responseCode, errorMessage);
				}

				// 2023-01-09: eventuali HTTP/1.1 100 Continue\r\n\r\n vengono scartate
				string prefix("HTTP/1.1 100 Continue\r\n\r\n");
				while (responseHeaderAndBody.starts_with(prefix))
					responseHeaderAndBody = responseHeaderAndBody.substr(prefix.size());

				size_t beginOfHeaderBodySeparatorIndex;
				if ((beginOfHeaderBodySeparatorIndex = responseHeaderAndBody.find("\r\n\r\n")) == string::npos)
				{
					string errorMessage = std::format(
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
			catch (CurlException &e)
			{
				throw;
			}
			catch (exception &e)
			{
				throw CurlException(e.what());
			}
		}
		catch (CurlException &e)
		{
			SPDLOG_ERROR(e.what());

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

			if (e.type() == "HTTPError")
			{
				HTTPError *exception = dynamic_cast<HTTPError *>(&e);
				if (exception->httpErrorCode == 404)
					throw;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_WARN(
					"{} retry"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry * (retryNumber + 1)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry * (retryNumber + 1)));
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), responseHeaderAndBody
				);

				if (e.type() == "ServerNotReachable")
					throw;
				else if (e.type() == "HTTPError")
				{
					HTTPError *exception = dynamic_cast<HTTPError *>(&e);
					if (exception->httpErrorCode == 502)
						throw ServerNotReachable(e.what());
					else
						throw;
				}
				else
					throw;
			}
		}
	}

	return make_pair(responseHeader, responseBody);
}

void CurlWrapper::httpPostPutBinary(
	string url,
	string requestType, // POST or PUT
	long timeoutInSeconds, string authorization, string body,
	string contentType, // i.e.: application/json
	vector<string> otherHeaders, string referenceToLog, int maxRetryNumber, int secondsToWaitBeforeToRetry, vector<uint8_t> &binary
)
{
	string api = "httpPostPutBinary";

	int retryNumber = -1;

	while (retryNumber < maxRetryNumber)
	{
		retryNumber++;

		CURL *curl = nullptr;
		struct curl_slist *headersList = nullptr;

		try
		{
			try
			{
				curl = curl_easy_init();
				if (!curl)
				{
					string errorMessage = std::format("{}. curl_easy_init failed", api);
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
						headersList = curl_slist_append(headersList, std::format("Content-Type: {}", contentType).c_str());
					if (authorization != "")
						headersList = curl_slist_append(headersList, std::format("Authorization: {}", authorization).c_str());

					for (string header : otherHeaders)
						headersList = curl_slist_append(headersList, header.c_str());

					curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
				}

				// request.setOpt(new curlpp::options::WriteStream(&response));
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&binary);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteBytesCallback);

				// store response headers in the response, You simply have to set next option to prefix the header to the
				// normal body output.
				// request.setOpt(new curlpp::options::Header(true));
				// curl_easy_setopt(curl, CURLOPT_HEADER, 1L);

				SPDLOG_DEBUG(
					"{} details"
					"{}"
					", url: {}"
					", contentType: {}"
					", authorization: {}"
					", otherHeaders.size: {}"
					", body: {}",
					api, referenceToLog, url, contentType, authorization, otherHeaders.size(), regex_replace(body, regex("\n"), " ")
				);

				chrono::system_clock::time_point start = chrono::system_clock::now();
				// request.perform();
				CURLcode curlCode = curl_easy_perform(curl);
				chrono::system_clock::time_point end = chrono::system_clock::now();
				if (curlCode != CURLE_OK)
				{
					string errorMessage = std::format(
						"{}. curl_easy_perform failed"
						", curlCode message: {}"
						", url: {}",
						api, curl_easy_strerror(curlCode), url
					);
					// SPDLOG_ERROR(errorMessage);

					throw ServerNotReachable(errorMessage);
				}

				// long responseCode = curlpp::infos::ResponseCode::get(request);
				long responseCode;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
				if (responseCode == 200 || responseCode == 201 || responseCode == 308 // permanently removed/redirect
				)
				{
					SPDLOG_DEBUG(
						"{} success"
						"{}"
						", @statistics@ - elapsed (secs): @{}@",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count()
					);
				}
				else
				{
					string errorMessage = std::format(
						"{} failed, wrong return status"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", responseCode: {}"
						", responseMessage: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), responseCode,
						string(binary.begin(), binary.end())
					);

					throw HTTPError(responseCode, errorMessage);
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
			catch (CurlException &e)
			{
				throw;
			}
			catch (exception &e)
			{
				throw CurlException(e.what());
			}
		}
		catch (CurlException &e)
		{
			SPDLOG_ERROR(e.what());

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

			if (e.type() == "HTTPError")
			{
				HTTPError *exception = dynamic_cast<HTTPError *>(&e);
				if (exception->httpErrorCode == 404)
					throw;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_WARN(
					"{} retry"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry * (retryNumber + 1)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry * (retryNumber + 1)));
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}",
					api, referenceToLog, url, timeoutInSeconds, e.what()
				);

				if (e.type() == "ServerNotReachable")
					throw;
				else if (e.type() == "HTTPError")
				{
					HTTPError *exception = dynamic_cast<HTTPError *>(&e);
					if (exception->httpErrorCode == 502)
						throw ServerNotReachable(e.what());
					else
						throw;
				}
				else
					throw;
			}
		}
	}
}

string CurlWrapper::httpPostPutFile(
	string url,
	string requestType, // POST or PUT
	long timeoutInSeconds, string authorization, string pathFileName, int64_t fileSizeInBytes, string contentType, string referenceToLog,
	int maxRetryNumber, int secondsToWaitBeforeToRetry, int64_t contentRangeStart, int64_t contentRangeEnd_Excluded
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
			try
			{
				CurlUploadData curlUploadData;
				curlUploadData.mediaSourceFileStream.open(pathFileName, ios::binary);
				if (!curlUploadData.mediaSourceFileStream)
				{
					string message = std::format(
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
					string errorMessage = std::format("{}. curl_easy_init failed", api);
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
							std::format("Content-Range: bytes {}-{}/{}", contentRangeStart, contentRangeEnd_Excluded - 1, fileSizeInBytes);
					}
					else
						contentLengthOrRangeHeader = std::format("Content-Length: {}", fileSizeInBytes);
					headersList = curl_slist_append(headersList, contentLengthOrRangeHeader.c_str());
					if (contentType != "")
						headersList = curl_slist_append(headersList, std::format("Content-Type: {}", contentType).c_str());
					if (authorization != "")
						headersList = curl_slist_append(headersList, std::format("Authorization: {}", authorization).c_str());

					curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);
				}

				// request.setOpt(new curlpp::options::WriteStream(&response));
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteStringCallback);

				SPDLOG_DEBUG(
					"{} details"
					"{}"
					", url: {}"
					", authorization: {}"
					", contentLengthOrRangeHeader: {}"
					", pathFileName: {}",
					api, referenceToLog, url, authorization, contentLengthOrRangeHeader, pathFileName
				);

				chrono::system_clock::time_point start = chrono::system_clock::now();
				// request.perform();
				CURLcode curlCode = curl_easy_perform(curl);
				chrono::system_clock::time_point end = chrono::system_clock::now();
				if (curlCode != CURLE_OK)
				{
					string errorMessage = std::format(
						"{}. curl_easy_perform failed"
						", curlCode message: {}"
						", url: {}",
						api, curl_easy_strerror(curlCode), url
					);
					// SPDLOG_ERROR(errorMessage);

					throw ServerNotReachable(errorMessage);
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
					SPDLOG_DEBUG(
						"{} success"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", response: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " ")
					);
				}
				else
				{
					string errorMessage = std::format(
						"{} failed, wrong return status"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", response: {}"
						", responseCode: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " "),
						responseCode
					);

					throw HTTPError(responseCode, errorMessage);
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
			catch (CurlException &e)
			{
				throw;
			}
			catch (exception &e)
			{
				throw CurlException(e.what());
			}
		}
		catch (CurlException &e)
		{
			SPDLOG_ERROR(e.what());

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

			if (e.type() == "HTTPError")
			{
				HTTPError *exception = dynamic_cast<HTTPError *>(&e);
				if (exception->httpErrorCode == 404)
					throw;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_WARN(
					"{} retry"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry * (retryNumber + 1)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry * (retryNumber + 1)));
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), response
				);

				if (e.type() == "ServerNotReachable")
					throw;
				else if (e.type() == "HTTPError")
				{
					HTTPError *exception = dynamic_cast<HTTPError *>(&e);
					if (exception->httpErrorCode == 502)
						throw ServerNotReachable(e.what());
					else
						throw;
				}
				else
					throw;
			}
		}
	}

	return response;
}

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
			try
			{
				// we could apply md5 to utc time
				string sFormData;
				string boundary;
				{
					boundary = to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()));

					string endOfLine = "\r\n";

					// fill in formData
					for (pair<string, string> data : formData)
					{
						sFormData += ("--" + boundary + endOfLine);
						sFormData +=
							("Content-Disposition: form-data; name=\"" + data.first + "\"" + endOfLine + endOfLine + data.second + endOfLine);
					}
					sFormData += ("--" + boundary + "--" + endOfLine + endOfLine);
				}

				// curlpp::Cleanup cleaner;
				// curlpp::Easy request;

				curl = curl_easy_init();
				if (!curl)
				{
					string errorMessage = std::format("{}. curl_easy_init failed", api);
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
				headersList = curl_slist_append(headersList, std::format("Content-Type: multipart/form-data; boundary=\"{}\"", boundary).c_str());
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);

				// request.setOpt(new curlpp::options::PostFields(sFormData));
				// request.setOpt(new curlpp::options::PostFieldSize(sFormData.length()));
				curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, sFormData.length());
				curl_easy_setopt(curl, CURLOPT_POSTFIELDS, sFormData.c_str());

				// request.setOpt(new curlpp::options::WriteStream(&response));
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteStringCallback);

				SPDLOG_DEBUG(
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
					string errorMessage = std::format(
						"{}. curl_easy_perform failed"
						", curlCode message: {}"
						", url: {}",
						api, curl_easy_strerror(curlCode), url
					);
					// SPDLOG_ERROR(errorMessage);

					throw ServerNotReachable(errorMessage);
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
					SPDLOG_DEBUG(
						"{} success"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", response: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " ")
					);
				}
				else
				{
					string errorMessage = std::format(
						"{} failed, wrong return status"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", response: {}"
						", responseCode: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), regex_replace(response, regex("\n"), " "),
						responseCode
					);

					throw HTTPError(responseCode, errorMessage);
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
			catch (CurlException &e)
			{
				throw;
			}
			catch (exception &e)
			{
				throw CurlException(e.what());
			}
		}
		catch (CurlException &e)
		{
			SPDLOG_ERROR(e.what());

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

			if (e.type() == "HTTPError")
			{
				HTTPError *exception = dynamic_cast<HTTPError *>(&e);
				if (exception->httpErrorCode == 404)
					throw;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_WARN(
					"{} retry"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry * (retryNumber + 1)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry * (retryNumber + 1)));
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), response
				);

				if (e.type() == "ServerNotReachable")
					throw;
				else if (e.type() == "HTTPError")
				{
					HTTPError *exception = dynamic_cast<HTTPError *>(&e);
					if (exception->httpErrorCode == 502)
						throw ServerNotReachable(e.what());
					else
						throw;
				}
				else
					throw;
			}
		}
	}

	return response;
}

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
			try
			{
				CurlUploadFormData curlUploadFormData;
				curlUploadFormData.mediaSourceFileStream.open(pathFileName, ios::binary);
				if (!curlUploadFormData.mediaSourceFileStream)
				{
					string message = std::format(
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
					string errorMessage = std::format("{}. curl_easy_init failed", api);
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
						postSize = (contentRangeEnd_Excluded - contentRangeStart) + curlUploadFormData.formData.size() +
								   curlUploadFormData.endOfFormData.size();
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
				headersList = curl_slist_append(headersList, std::format("Content-Type: multipart/form-data; boundary=\"{}\"", boundary).c_str());
				curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);

				// request.setOpt(new curlpp::options::WriteStream(&response));
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteStringCallback);

				SPDLOG_DEBUG(
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
					string errorMessage = std::format(
						"{}. curl_easy_perform failed"
						", curlCode message: {}"
						", url: {}",
						api, curl_easy_strerror(curlCode), url
					);
					// SPDLOG_ERROR(errorMessage);

					throw ServerNotReachable(errorMessage);
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
					SPDLOG_DEBUG(
						"{} success"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", curlUploadFormData.payloadBytesSent: {}"
						", response: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), curlUploadFormData.payloadBytesSent,
						regex_replace(response, regex("\n"), " ")
					);
				}
				else
				{
					string errorMessage = std::format(
						"{} failed, wrong return status"
						"{}"
						", @statistics@ - elapsed (secs): @{}@"
						", curlUploadFormData.formData: {}"
						", curlUploadFormData.endOfFormData: {}"
						", response: {}"
						", responseCode: {}",
						api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count(), curlUploadFormData.formData,
						curlUploadFormData.endOfFormData, regex_replace(response, regex("\n"), " "), responseCode
					);

					throw HTTPError(responseCode, errorMessage);
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
			catch (CurlException &e)
			{
				throw;
			}
			catch (exception &e)
			{
				throw CurlException(e.what());
			}
		}
		catch (CurlException &e)
		{
			SPDLOG_ERROR(e.what());

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

			if (e.type() == "HTTPError")
			{
				HTTPError *exception = dynamic_cast<HTTPError *>(&e);
				if (exception->httpErrorCode == 404)
					throw;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_WARN(
					"{} retry"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry * (retryNumber + 1)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry * (retryNumber + 1)));
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed"
					"{}"
					", url: {}"
					", timeoutInSeconds: {}"
					", exception: {}"
					", response.str(): {}",
					api, referenceToLog, url, timeoutInSeconds, e.what(), response
				);

				if (e.type() == "ServerNotReachable")
					throw;
				else if (e.type() == "HTTPError")
				{
					HTTPError *exception = dynamic_cast<HTTPError *>(&e);
					if (exception->httpErrorCode == 502)
						throw ServerNotReachable(e.what());
					else
						throw;
				}
				else
					throw;
			}
		}
	}

	return response;
}

void CurlWrapper::downloadFile(
	string url, string destBinaryPathName, int (*progressCallback)(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t), void *progressData,
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

					SPDLOG_WARN(
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
					string errorMessage = std::format("{}. curl_easy_init failed", api);
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
				curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
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

				SPDLOG_DEBUG(
					"{} details"
					"{}"
					", url: {}"
					", resumeScenario: {}",
					api, referenceToLog, url, resumeScenario
				);

				chrono::system_clock::time_point start = chrono::system_clock::now();
				// request.perform();
				CURLcode curlCode = curl_easy_perform(curl);
				chrono::system_clock::time_point end = chrono::system_clock::now();
				if (curlCode != CURLE_OK)
				{
					string errorMessage = std::format(
						"{}. curl_easy_perform failed"
						", curlCode message: {}"
						", url: {}",
						api, curl_easy_strerror(curlCode), url
					);
					// SPDLOG_ERROR(errorMessage);

					throw ServerNotReachable(errorMessage);
				}

				(curlDownloadData.mediaSourceFileStream).close();

				SPDLOG_DEBUG(
					"{} success"
					"{}"
					", @statistics@ - elapsed (secs): @{}@",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count()
				);

				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				break;
			}
			catch (CurlException &e)
			{
				throw;
			}
			catch (exception &e)
			{
				throw CurlException(e.what());
			}
		}
		catch (CurlException &e)
		{
			SPDLOG_ERROR(e.what());

			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_WARN(
					"{} retry"
					"{}"
					", url: {}"
					", exception: {}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, url, e.what(), retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry * (retryNumber + 1)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry * (retryNumber + 1)));
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed"
					"{}"
					", url: {}"
					", exception: {}",
					api, referenceToLog, url, e.what()
				);

				throw;
			}
		}
	}
}

void CurlWrapper::ftpFile(
	string filePathName, string fileName, int64_t sizeInBytes, string ftpServer, int ftpPort, string ftpUserName, string ftpPassword,
	string ftpRemoteDirectory, string ftpRemoteFileName, int (*progressCallback)(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t),
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
			try
			{
				ftpUrl = std::format("ftp://{}:{}@{}:{}{}", ftpUserName, ftpPassword, ftpServer, ftpPort, ftpRemoteDirectory);

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
					string errorMessage = std::format("{}. curl_easy_init failed", api);
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
				curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
				curl_easy_setopt(curl, CURLOPT_XFERINFODATA, progressData);
				curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

				SPDLOG_DEBUG(
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
					string errorMessage = std::format(
						"{}. curl_easy_perform failed"
						", curlCode message: {}"
						", ftpUrl: {}",
						api, curl_easy_strerror(curlCode), ftpUrl
					);
					// SPDLOG_ERROR(errorMessage);

					throw ServerNotReachable(errorMessage);
				}

				SPDLOG_DEBUG(
					"{} success"
					"{}"
					", @statistics@ - elapsed (secs): @{}@",
					api, referenceToLog, chrono::duration_cast<chrono::seconds>(end - start).count()
				);

				if (curl)
				{
					curl_easy_cleanup(curl);
					curl = nullptr;
				}

				break;
			}
			catch (CurlException &e)
			{
				throw;
			}
			catch (exception &e)
			{
				throw CurlException(e.what());
			}
		}
		catch (CurlException &e)
		{
			SPDLOG_ERROR(e.what());

			if (curl)
			{
				curl_easy_cleanup(curl);
				curl = nullptr;
			}

			if (retryNumber < maxRetryNumber)
			{
				SPDLOG_WARN(
					"{} retry"
					"{}"
					", exception: {}"
					", retryNumber: {}"
					", maxRetryNumber: {}"
					", secondsToWaitBeforeToRetry: {}",
					api, referenceToLog, e.what(), retryNumber, maxRetryNumber, secondsToWaitBeforeToRetry * (retryNumber + 1)
				);
				this_thread::sleep_for(chrono::seconds(secondsToWaitBeforeToRetry * (retryNumber + 1)));
			}
			else
			{
				SPDLOG_ERROR(
					"{} failed"
					"{}"
					", exception: {}",
					api, referenceToLog, e.what()
				);

				throw;
			}
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
	string emailServerURL, // i.e.: smtps://xxx.xxx.xxx:465
	string userName,	   // i.e.: xxx@xxx.com
	// 2023-02-18: mi è sembrato che il provider blocca l'email se username e from sono diversi!!!
	string password, string from, string tosCommaSeparated, string ccsCommaSeparated, string subject, vector<string> &emailBody,
	string contentType // i.e.: text/html; charset=\"UTF-8\"
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
		curlUploadEmailData.emailLines.push_back(std::format("From: <{}>\r\n", from));

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
						addresses = std::format("<{}>", address);
					else
						addresses += std::format(", <{}>", address);
				}
			}

			curlUploadEmailData.emailLines.push_back(std::format("To: {}\r\n", addresses));
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
						addresses = std::format("<{}>", address);
					else
						addresses += std::format(", <{}>", address);
				}
			}

			curlUploadEmailData.emailLines.push_back(std::format("Cc: {}\r\n", addresses));
		}

		curlUploadEmailData.emailLines.push_back(std::format("Subject: {}\r\n", subject));
		curlUploadEmailData.emailLines.push_back(std::format("Content-Type: {}\r\n", contentType));
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

	SPDLOG_DEBUG("Email sent successful");

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
