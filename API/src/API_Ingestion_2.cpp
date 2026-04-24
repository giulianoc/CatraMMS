/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   API.cpp
 *
 * Created on February 18, 2018, 1:27 AM
 */

#include "API.h"
#include "Convert.h"
#include "CurlWrapper.h"
#include "Datetime.h"
#include "Encrypt.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "ProcessUtility.h"
#include "SafeFileSystem.h"
#include "Validator.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"
#include <format>
#include <fstream>
#include <regex>
#include <spdlog/fmt/bundled/ranges.h>
#include <sstream>

using namespace std;
using json = nlohmann::json;

void API::uploadedBinary(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "uploadedBinary";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	try
	{
		if (_noFileSystemAccess)
		{
			string errorMessage = string("no rights to execute this method") + ", _noFileSystemAccess: " + to_string(_noFileSystemAccess);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		// sourceBinaryPathFile will be something like:
		// /var/mms/storage/nginxWorkingAreaRepository/0000001023
		string sourceBinaryPathFile = requestData.getHeaderParameter("x-file", string(""), true);

		// Content-Range: bytes 0-99999/100000
		bool contentRangePresent = false;
		uint64_t contentRangeStart = -1;
		uint64_t contentRangeEnd = -1;
		uint64_t contentRangeSize = -1;
		double uploadingProgress = 0.0;
		string contentRange = requestData.getHeaderParameter("content-range", string(""));
		if (!contentRange.empty())
		{
			try
			{
				FCGIRequestData::parseContentRange(contentRange, contentRangeStart, contentRangeEnd, contentRangeSize);

				// X : 100 = contentRangeEnd : contentRangeSize
				uploadingProgress = 100 * contentRangeEnd / contentRangeSize;

				contentRangePresent = true;
			}
			catch (exception &e)
			{
				string errorMessage = string("Content-Range is not well done. Expected format: "
											 "'Content-Range: bytes <start>-<end>/<size>'") +
									  ", contentRange: " + contentRange;
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(apiAuthorizationDetails->workspace);
		string destBinaryPathName = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
		bool segmentedContent = false;
		try
		{
			json parametersRoot = _mmsEngineDBFacade->ingestionJob_columnAsJson(
				apiAuthorizationDetails->workspace->_workspaceKey, "metaDataContent", ingestionJobKey,
				// 2022-12-18: l'ingestionJob potrebbe essere stato
				// appena aggiunto
				true
			);

			string field = "fileFormat";
			if (JSONUtils::isPresent(parametersRoot, field))
			{
				string fileFormat = JSONUtils::as<string>(parametersRoot, field, "");
				// 2022-08-11: I guess the correct fileFormat is m3u8-tar.gz and
				// not m3u8 if (fileFormat == "m3u8")
				if (fileFormat == "m3u8-tar.gz")
					segmentedContent = true;
			}
		}
		catch (DBRecordNotFound &e)
		{
			string errorMessage = string("ingestionJob_MetadataContent failed") +
								  ", workspace->_workspaceKey: " + to_string(apiAuthorizationDetails->workspace->_workspaceKey) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceBinaryPathFile: " + sourceBinaryPathFile +
								  ", destBinaryPathName: " + destBinaryPathName + ", e.what: " + e.what();
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("ingestionJob_MetadataContent failed") +
								  ", workspace->_workspaceKey: " + to_string(apiAuthorizationDetails->workspace->_workspaceKey) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceBinaryPathFile: " + sourceBinaryPathFile +
								  ", destBinaryPathName: " + destBinaryPathName;
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		if (segmentedContent)
			destBinaryPathName = destBinaryPathName + ".tar.gz";

		if (!contentRangePresent)
		{
			try
			{
				LOG_INFO(
					"Moving file from nginx area to ingestion user area"
					", ingestionJobKey: {}"
					", sourceBinaryPathFile: {}"
					", destBinaryPathName: {}",
					ingestionJobKey, sourceBinaryPathFile, destBinaryPathName
				);

				MMSStorage::move(ingestionJobKey, sourceBinaryPathFile, destBinaryPathName);
			}
			catch (exception &e)
			{
				string errorMessage = std::format(
					"Error to move file"
					", ingestionJobKey: {}"
					", sourceBinaryPathFile: {}"
					", destBinaryPathName: {}",
					ingestionJobKey, sourceBinaryPathFile, destBinaryPathName
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			/* 2023-03-19: manageTarFileInCaseOfIngestionOfSegments viene fatto
					da MMSEngineProcessor::handleLocalAssetIngestionEventThread
					per evitare che questa API impiega minuti a terminare
			if (segmentedContent)
			{
					try
					{
							// by a convention, the directory inside the tar
		file has to be named as 'content' string localSourceBinaryPathFile =
		"/content.tar.gz";

							_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(ingestionJobKey,
											destBinaryPathName,
		workspaceIngestionRepository, localSourceBinaryPathFile);
					}
					catch(runtime_error& e)
					{
							string errorMessage =
		string("manageTarFileInCaseOfIngestionOfSegments failed")
									+ ", ingestionJobKey: " +
		to_string(ingestionJobKey)
							;
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
					}
			}
			*/

			bool sourceBinaryTransferred = true;
			LOG_INFO(
				"Update IngestionJob"
				", ingestionJobKey: {}"
				", sourceBinaryTransferred: {}",
				ingestionJobKey, sourceBinaryTransferred
			);
			_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, sourceBinaryTransferred);
		}
		else
		{
			//  Content-Range is present

			if (fs::exists(destBinaryPathName))
			{
				if (contentRangeStart == 0)
				{
					// content is reset
					ofstream osDestStream(destBinaryPathName.c_str(), ofstream::binary | ofstream::trunc);

					osDestStream.close();
				}

#ifdef SAFEFILESYSTEMTHREAD
				unsigned long destBinaryPathNameSizeInBytes =
					SafeFileSystem::fileSizeThread(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
				unsigned long sourceBinaryPathFileSizeInBytes =
					SafeFileSystem::fileSizeThread(sourceBinaryPathFile, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
				unsigned long destBinaryPathNameSizeInBytes =
					SafeFileSystem::fileSizeProcess(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
				unsigned long sourceBinaryPathFileSizeInBytes =
					SafeFileSystem::fileSizeProcess(sourceBinaryPathFile, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#else
				unsigned long destBinaryPathNameSizeInBytes = fs::file_size(destBinaryPathName);
				unsigned long sourceBinaryPathFileSizeInBytes = fs::file_size(sourceBinaryPathFile);
#endif

				LOG_INFO(
					"Content-Range before concat"
					", ingestionJobKey: {}"
					", contentRangeStart: {}"
					", contentRangeEnd: {}"
					", contentRangeSize: {}"
					", segmentedContent: {}"
					", destBinaryPathName: {}"
					", destBinaryPathNameSizeInBytes: {}"
					", sourceBinaryPathFile: {}"
					", sourceBinaryPathFileSizeInBytes: {}",
					ingestionJobKey, contentRangeStart, contentRangeEnd, contentRangeSize, segmentedContent, destBinaryPathName,
					destBinaryPathNameSizeInBytes, sourceBinaryPathFile, sourceBinaryPathFileSizeInBytes
				);

				// waiting in case of nfs delay
				chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
				while (contentRangeStart != destBinaryPathNameSizeInBytes && chrono::system_clock::now() < end)
				{
					this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));

#ifdef SAFEFILESYSTEMTHREAD
					destBinaryPathNameSizeInBytes =
						SafeFileSystem::fileSizeThread(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
					destBinaryPathNameSizeInBytes =
						SafeFileSystem::fileSizeProcess(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#else
					destBinaryPathNameSizeInBytes = fs::file_size(destBinaryPathName);
#endif
				}

				if (contentRangeStart != destBinaryPathNameSizeInBytes)
				{
					string errorMessage = std::format(
						"Content-Range. This is NOT the next expected "
						"chunk because Content-Range start is different "
						"from fileSizeInBytes"
						", ingestionJobKey: {}"
						", contentRangeStart: {}"
						", contentRangeEnd: {}"
						", contentRangeSize: {}"
						", segmentedContent: {}"
						", destBinaryPathName: {}"
						", sourceBinaryPathFile: {}"
						", sourceBinaryPathFileSizeInBytes: {}"
						", destBinaryPathNameSizeInBytes (expected): {}",
						ingestionJobKey, contentRangeStart, contentRangeEnd, contentRangeSize, segmentedContent, destBinaryPathName,
						sourceBinaryPathFile, sourceBinaryPathFileSizeInBytes, destBinaryPathNameSizeInBytes
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				try
				{
					// bool removeSrcFileAfterConcat = true;

					// FileIO::concatFile(destBinaryPathName,
					// sourceBinaryPathFile, removeSrcFileAfterConcat);

					// concat files, append a file to another
					chrono::system_clock::time_point start = chrono::system_clock::now();
					{
						ofstream ofDestination(destBinaryPathName, std::ios_base::binary | std::ios_base::app);
						ifstream ifSource(sourceBinaryPathFile, std::ios_base::binary);

						ofDestination << ifSource.rdbuf();

						ofDestination.close();
						ifSource.close();
					}

#ifdef SAFEFILESYSTEMTHREAD
					uintmax_t destBinaryPathNameSize =
						SafeFileSystem::fileSizeThread(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
					uintmax_t destBinaryPathNameSize =
						SafeFileSystem::fileSizeProcess(destBinaryPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#else
					uintmax_t destBinaryPathNameSize = fs::file_size(destBinaryPathName);
#endif
					LOG_INFO(
						"Content-Range after concat"
						", ingestionJobKey: {}"
						", contentRangeStart: {}"
						", contentRangeEnd: {}"
						", contentRangeSize: {}"
						", segmentedContent: {}"
						", destBinaryPathName: {}"
						", destBinaryPathNameSizeInBytes: {}"
						", sourceBinaryPathFile: {}"
						", sourceBinaryPathFileSizeInBytes: {}"
						", concat elapsed (secs): {}",
						ingestionJobKey, contentRangeStart, contentRangeEnd, contentRangeSize, segmentedContent, destBinaryPathName,
						destBinaryPathNameSize, sourceBinaryPathFile, sourceBinaryPathFileSizeInBytes,
						chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - start).count()
					);

					fs::remove_all(sourceBinaryPathFile);
				}
				catch (exception &e)
				{
					string errorMessage = std::format(
						"Content-Range. Error to concat file"
						", ingestionJobKey: {}"
						", destBinaryPathName: {}"
						", sourceBinaryPathFile: {}",
						ingestionJobKey, destBinaryPathName, sourceBinaryPathFile
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				// binary file does not exist, so this is the first chunk

				if (contentRangeStart != 0)
				{
					string errorMessage = std::format(
						"Content-Range. This is the first chunk of the "
						"file and Content-Range start has to be 0"
						", ingestionJobKey: {}",
						", contentRangeStart: {}", ingestionJobKey, contentRangeStart
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				try
				{
					LOG_INFO(
						"Content-Range. Moving file from nginx area to "
						"ingestion user area"
						", ingestionJobKey: {}"
						", sourceBinaryPathFile: {}"
						", destBinaryPathName: {}",
						ingestionJobKey, sourceBinaryPathFile, destBinaryPathName
					);

					MMSStorage::move(ingestionJobKey, sourceBinaryPathFile, destBinaryPathName);
				}
				catch (exception &e)
				{
					string errorMessage = std::format(
						"Content-Range. Error to move file"
						", ingestionJobKey: {}"
						", sourceBinaryPathFile: {}"
						", destBinaryPathName: {}",
						ingestionJobKey, sourceBinaryPathFile, destBinaryPathName
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			if (contentRangeEnd + 1 == contentRangeSize)
			{
				/* 2023-03-19: manageTarFileInCaseOfIngestionOfSegments viene
				fatto da
				MMSEngineProcessor::handleLocalAssetIngestionEventThread per
				evitare che questa API impiega minuti a terminare if
				(segmentedContent)
				{
						try
						{
								// by a convention, the directory inside the tar
				file has to be named as 'content' string
				localSourceBinaryPathFile = "/content.tar.gz";

								_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(ingestionJobKey,
										destBinaryPathName,
				workspaceIngestionRepository, localSourceBinaryPathFile);
						}
						catch(runtime_error& e)
						{
								string errorMessage =
				string("manageTarFileInCaseOfIngestionOfSegments failed")
										+ ", ingestionJobKey: " +
				to_string(ingestionJobKey)
								;
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
						}
				}
				*/

				bool sourceBinaryTransferred = true;
				LOG_INFO(
					"Content-Range. Update IngestionJob"
					", ingestionJobKey: {}"
					", sourceBinaryTransferred: {}",
					ingestionJobKey, sourceBinaryTransferred
				);
				_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, sourceBinaryTransferred);
			}
			else
			{
				LOG_INFO(
					"Content-Range. Update IngestionJob (uploading progress)"
					", ingestionJobKey: {}"
					", uploadingProgress: {}",
					ingestionJobKey, uploadingProgress
				);
				_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(ingestionJobKey, uploadingProgress);
			}
		}

		string responseBody;
		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);
	}
	catch (exception e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::stopUploadFileProgressThread()
{
	_fileUploadProgressThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));
}

void API::fileUploadProgressCheckThread()
{
	while (!_fileUploadProgressThreadShutdown)
	{
		this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));

		lock_guard<mutex> locker(_fileUploadProgressData->_mutex);

		for (auto itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.begin();
			 itr != _fileUploadProgressData->_filesUploadProgressToBeMonitored.end();)
		{
			bool iteratorAlreadyUpdated = false;

			if (itr->_callFailures >= _maxProgressCallFailures)
			{
				LOG_ERROR(
					"fileUploadProgressCheckThread: remove entry because of too many call failures"
					", ingestionJobKey: {}"
					", progressId: {}"
					", binaryVirtualHostName: {}"
					", binaryListenHost: {}"
					", callFailures: {}"
					", _maxProgressCallFailures: {}",
					itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures,
					_maxProgressCallFailures
				);
				itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr); // returns iterator to the next element

				continue;
			}

			try
			{
				string progressURL = std::format("http://:{}:{}{}", itr->_binaryListenHost, _webServerPort, _progressURI);
				string progressIdHeader = std::format("X-Progress-ID: {}", itr->_progressId);
				string hostHeader = std::format("Host: {}", itr->_binaryVirtualHostName);

				LOG_INFO(
					"Call for upload progress"
					", ingestionJobKey: {}"
					", progressId: {}"
					", binaryVirtualHostName: {}"
					", binaryListenHost: {}"
					", callFailures: {}"
					", progressURL: {}"
					", progressIdHeader: {}"
					", hostHeader: {}",
					itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures, progressURL,
					progressIdHeader, hostHeader
				);

				vector<string> otherHeaders;
				otherHeaders.push_back(progressIdHeader);
				otherHeaders.push_back(hostHeader); // important for the nginx virtual host
				int curlTimeoutInSeconds = 120;
				json uploadProgressResponse = CurlWrapper::httpGetJson(
					progressURL, curlTimeoutInSeconds, "", otherHeaders, std::format(", ingestionJobKey: {}", itr->_ingestionJobKey)
				);

				try
				{
					// json uploadProgressResponse =
					// JSONUtils::toJson<json>(-1, -1, sResponse);

					// { "state" : "uploading", "received" : 731195032, "size" :
					// 745871360 } At the end: { "state" : "done" } In case of
					// error: { "state" : "error", "status" : 500 }
					string state = JSONUtils::as<string>(uploadProgressResponse, "state", "");
					if (state == "done")
					{
						double relativeProgress = 100.0;
						double relativeUploadingPercentage = 100.0;

						int64_t absoluteReceived = -1;
						if (itr->_contentRangePresent)
							absoluteReceived = itr->_contentRangeEnd;
						int64_t absoluteSize = -1;
						if (itr->_contentRangePresent)
							absoluteSize = itr->_contentRangeSize;

						double absoluteProgress;
						if (itr->_contentRangePresent)
							absoluteProgress = (static_cast<double>(absoluteReceived) / static_cast<double>(absoluteSize)) * 100;

						// this is to have one decimal in the percentage
						double absoluteUploadingPercentage;
						if (itr->_contentRangePresent)
							absoluteUploadingPercentage = static_cast<double>(static_cast<int>(absoluteProgress * 10)) / 10;

						if (itr->_contentRangePresent)
						{
							LOG_INFO(
								"Upload just finished"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", relativeProgress: {}"
								", relativeUploadingPercentage: {}"
								", absoluteProgress: {}"
								", absoluteUploadingPercentage: {}"
								", lastPercentageUpdated: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, relativeProgress,
								relativeUploadingPercentage, absoluteProgress, absoluteUploadingPercentage, itr->_lastPercentageUpdated
							);
						}
						else
						{
							LOG_INFO(
								"Upload just finished"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", relativeProgress: {}"
								", relativeUploadingPercentage: {}"
								", lastPercentageUpdated: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, relativeProgress,
								relativeUploadingPercentage, itr->_lastPercentageUpdated
							);
						}

						if (itr->_contentRangePresent)
						{
							LOG_INFO(
								"Update IngestionJob"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", absoluteUploadingPercentage: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost,
								absoluteUploadingPercentage
							);
							_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, absoluteUploadingPercentage);
						}
						else
						{
							LOG_INFO(
								"Update IngestionJob"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", relativeUploadingPercentage: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost,
								relativeUploadingPercentage
							);
							_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, relativeUploadingPercentage);
						}

						itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr); // returns iterator to the next element

						iteratorAlreadyUpdated = true;
					}
					else if (state == "error")
					{
						LOG_ERROR(
							"fileUploadProgressCheckThread: remove entry because state is 'error'"
							", ingestionJobKey: {}"
							", progressId: {}"
							", binaryVirtualHostName: {}"
							", binaryListenHost: {}"
							", callFailures: {}"
							", _maxProgressCallFailures: {}",
							itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures,
							_maxProgressCallFailures
						);
						itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr); // returns iterator to the next element

						iteratorAlreadyUpdated = true;
					}
					else if (state == "uploading")
					{
						int64_t relativeReceived = JSONUtils::as<int64_t>(uploadProgressResponse, "received", 0);
						int64_t absoluteReceived = -1;
						if (itr->_contentRangePresent)
							absoluteReceived = relativeReceived + itr->_contentRangeStart;
						int64_t relativeSize = JSONUtils::as<int64_t>(uploadProgressResponse, "size", 0);
						int64_t absoluteSize = -1;
						if (itr->_contentRangePresent)
							absoluteSize = itr->_contentRangeSize;

						double relativeProgress = ((double)relativeReceived / (double)relativeSize) * 100;
						double absoluteProgress;
						if (itr->_contentRangePresent)
							absoluteProgress = ((double)absoluteReceived / (double)absoluteSize) * 100;

						// this is to have one decimal in the percentage
						double relativeUploadingPercentage = ((double)((int)(relativeProgress * 10))) / 10;
						double absoluteUploadingPercentage;
						if (itr->_contentRangePresent)
							absoluteUploadingPercentage = ((double)((int)(absoluteProgress * 10))) / 10;

						if (itr->_contentRangePresent)
						{
							LOG_INFO(
								"Upload still running"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", relativeProgress: {}"
								", absoluteProgress: {}"
								", lastPercentageUpdated: {}"
								", relativeReceived: {}"
								", absoluteReceived: {}"
								", relativeSize: {}"
								", absoluteSize: {}"
								", relativeUploadingPercentage: {}"
								", absoluteUploadingPercentage: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, relativeProgress,
								absoluteProgress, itr->_lastPercentageUpdated, relativeReceived, absoluteReceived, relativeSize, absoluteSize,
								relativeUploadingPercentage, absoluteUploadingPercentage
							);
						}
						else
						{
							LOG_INFO(
								"Upload still running"
								", ingestionJobKey: {}"
								", progressId: {}"
								", binaryVirtualHostName: {}"
								", binaryListenHost: {}"
								", progress: {}"
								", lastPercentageUpdated: {}"
								", received: {}"
								", size: {}"
								", uploadingPercentage: {}",
								itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, relativeProgress,
								itr->_lastPercentageUpdated, relativeReceived, relativeSize, relativeUploadingPercentage
							);
						}

						if (itr->_contentRangePresent)
						{
							if (itr->_lastPercentageUpdated != absoluteUploadingPercentage)
							{
								LOG_INFO(
									"Update IngestionJob"
									", ingestionJobKey: {}"
									", progressId: {}"
									", binaryVirtualHostName: {}"
									", binaryListenHost: {}"
									", absoluteUploadingPercentage: {}",
									itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost,
									absoluteUploadingPercentage
								);
								_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, absoluteUploadingPercentage);

								itr->_lastPercentageUpdated = absoluteUploadingPercentage;
							}
						}
						else
						{
							if (itr->_lastPercentageUpdated != relativeUploadingPercentage)
							{
								LOG_INFO(
									"Update IngestionJob"
									", ingestionJobKey: {}"
									", progressId: {}"
									", binaryVirtualHostName: {}"
									", binaryListenHost: {}"
									", uploadingPercentage: {}",
									itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost,
									relativeUploadingPercentage
								);
								_mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(itr->_ingestionJobKey, relativeUploadingPercentage);

								itr->_lastPercentageUpdated = relativeUploadingPercentage;
							}
						}
					}
					else
					{
						string errorMessage = std::format(
							"file upload progress. State is wrong"
							", state: {}"
							", ingestionJobKey: {}"
							", progressId: {}"
							", binaryVirtualHostName: {}"
							", binaryListenHost: {}"
							", callFailures: {}"
							", progressURL: {}"
							", progressIdHeader: {}",
							state, itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures,
							progressURL, progressIdHeader
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				catch (...)
				{
					string errorMessage = "response Body json is not well format"
						// + ", sResponse: " + sResponse
						;
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch (exception& e)
			{
				LOG_ERROR(
					"Call for upload progress failed"
					", ingestionJobKey: {}"
					", progressId: {}"
					", binaryVirtualHostName: {}"
					", binaryListenHost: {}"
					", callFailures: {}"
					", exception: {}",
					itr->_ingestionJobKey, itr->_progressId, itr->_binaryVirtualHostName, itr->_binaryListenHost, itr->_callFailures, e.what()
				);

				itr->_callFailures = itr->_callFailures + 1;
			}

			if (!iteratorAlreadyUpdated)
				++itr;
		}
	}
}

void API::ingestionRootsStatus(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "ingestionRootsStatus";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t ingestionRootKey = requestData.getQueryParameter("ingestionRootKey", static_cast<int64_t>(-1), false);

		int64_t mediaItemKey = requestData.getQueryParameter("mediaItemKey", static_cast<int64_t>(-1), false);

		int32_t start = requestData.getQueryParameter("start", static_cast<int64_t>(0), false);

		int32_t rows = requestData.getQueryParameter("rows", static_cast<int64_t>(10), false);
		if (rows > _maxPageSize)
		{
			// 2022-02-13: changed to return an error otherwise the user
			//	think to ask for a huge number of items while the return
			// is much less

			// rows = _maxPageSize;

			string errorMessage = std::format(
				"rows parameter too big"
				", rows: {}"
				", _maxPageSize: {}",
				rows, _maxPageSize
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string startIngestionDate = requestData.getQueryParameter("startIngestionDate", "", false);

		string endIngestionDate = requestData.getQueryParameter("endIngestionDate", "", false);

		string label = requestData.getQueryParameter("label", "", false);

		string status = requestData.getQueryParameter("status", "all", false);

		bool hiddenToo = requestData.getQueryParameter("hiddenToo", true, false);

		bool asc = requestData.getQueryParameter("asc", true, false);

		bool ingestionJobOutputs = requestData.getQueryParameter("ingestionJobOutputs", true, false);

		bool dependencyInfo = requestData.getQueryParameter("dependencyInfo", true, false);

		{
			json ingestionStatusRoot = _mmsEngineDBFacade->getIngestionRootsStatus(
				apiAuthorizationDetails->workspace, ingestionRootKey, mediaItemKey, start, rows,
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate, label, status, asc, dependencyInfo, ingestionJobOutputs, hiddenToo,
				// 2022-12-18: IngestionRoot dovrebbe essere stato aggiunto
				// da tempo
				false
			);

			string responseBody = JSONUtils::toString(ingestionStatusRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::ingestionRootMetaDataContent(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "ingestionRootMetaDataContent";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t ingestionRootKey = requestData.getQueryParameter("ingestionRootKey", static_cast<int64_t>(-1), true);

		bool processedMetadata = requestData.getQueryParameter("processedMetadata", false);

		{
			string ingestionRootMetaDataContent;
			if (processedMetadata)
				ingestionRootMetaDataContent = _mmsEngineDBFacade->ingestionRoot_columnAsString(
					apiAuthorizationDetails->workspace->_workspaceKey, "processedMetaDataContent", ingestionRootKey,
					// 2022-12-18: IngestionJobKey dovrebbe essere stato
					// aggiunto da tempo
					false
				);
			else
				ingestionRootMetaDataContent = _mmsEngineDBFacade->ingestionRoot_columnAsString(
					apiAuthorizationDetails->workspace->_workspaceKey, "metaDataContent", ingestionRootKey,
					// 2022-12-18: IngestionJobKey dovrebbe essere stato
					// aggiunto da tempo
					false
				);

			/*
			string ingestionRootMetaDataContent = _mmsEngineDBFacade->getIngestionRootMetaDataContent(
				workspace, ingestionRootKey, processedMetadata,
				// 2022-12-18: IngestionJobKey dovrebbe essere stato
				// aggiunto da tempo
				false
			);
			*/

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, ingestionRootMetaDataContent);
		}
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::ingestionJobsStatus(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "ingestionJobsStatus";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1));

		int32_t start = requestData.getQueryParameter("start", static_cast<int32_t>(0));

		int32_t rows = requestData.getQueryParameter("rows", static_cast<int32_t>(10));
		if (rows > _maxPageSize)
		{
			// 2022-02-13: changed to return an error otherwise the user
			//	think to ask for a huge number of items while the return
			// is much less

			// rows = _maxPageSize;

			string errorMessage = std::format(
				"rows parameter too big"
				", rows: {}"
				", _maxPageSize: {}",
				rows, _maxPageSize
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string label = requestData.getQueryParameter("label", "");

		bool labelLike = requestData.getQueryParameter("labelLike", true);

		string startIngestionDate = requestData.getQueryParameter("startIngestionDate", "");
		string endIngestionDate = requestData.getQueryParameter("endIngestionDate", "");

		string startScheduleDate = requestData.getQueryParameter("startScheduleDate", "");

		string ingestionType = requestData.getQueryParameter("ingestionType", "");

		bool asc = requestData.getQueryParameter("asc", true);

		bool ingestionJobOutputs = requestData.getQueryParameter("ingestionJobOutputs", true);

		bool dependencyInfo = requestData.getQueryParameter("dependencyInfo", true);

		// used in case of live-proxy
		string configurationLabel = requestData.getQueryParameter("configurationLabel", "");

		// used in case of live-grid
		string outputChannelLabel = requestData.getQueryParameter("outputChannelLabel", "");

		// used in case of live-recorder
		int64_t recordingCode = requestData.getQueryParameter("recordingCode", static_cast<int64_t>(-1));

		// used in case of broadcaster
		bool broadcastIngestionJobKeyNotNull = requestData.getQueryParameter("broadcastIngestionJobKeyNotNull", false);

		string jsonParametersCondition = requestData.getQueryParameter("jsonParametersCondition", "");

		string status = requestData.getQueryParameter("status", "all");

		bool fromMaster = requestData.getQueryParameter("fromMaster", false);

		{
			json ingestionStatusRoot = _mmsEngineDBFacade->getIngestionJobsStatus(
				apiAuthorizationDetails->workspace, ingestionJobKey, start, rows, label, labelLike,
				/* startAndEndIngestionDatePresent, */ startIngestionDate, endIngestionDate, startScheduleDate, ingestionType, configurationLabel,
				outputChannelLabel, recordingCode, broadcastIngestionJobKeyNotNull, jsonParametersCondition, asc, status, dependencyInfo,
				ingestionJobOutputs, fromMaster
			);

			string responseBody = JSONUtils::toString(ingestionStatusRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::cancelIngestionJob(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "API::cancelIngestionJob";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canCancelIngestionJob)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", cancelIngestionJob: {}",
			apiAuthorizationDetails->canCancelIngestionJob
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		/*
		 * This forceCancel parameter was added because of this Scenario:
		 *	1. Live proxy ingestion job.
		 *	2. the ffmpeg command will never start, may be because of a
		 *wrong url In this scenario there is no way to cancel the job because:
		 *		1. The EncoderVideoAudioProxy thread will never exit
		 *from his loop because it is a Live Proxy. The only way to exit is
		 *through the kill of the encoding job (kill of the ffmpeg command)
		 *		2. The encoding job cannot be killed because the ffmpeg
		 *process will never start
		 *
		 *  Really I discovered later that the above scenario was already
		 *managed by the kill encoding job method. In fact, this method set the
		 *encodingStatusFailures into DB to -100. The EncoderVideoAudioProxy
		 *thread checks this number and, if it is negative, exit for his
		 *  internal look
		 *
		 *  For this reason, it would be better to avoid to use the forceCancel
		 *parameter because it is set the ingestionJob status to
		 *End_CanceledByUser but it could leave the EncoderVideoAudioProxy
		 *thread allocated and/or the ffmpeg process running.
		 *
		 * This forceCancel parameter is useful in scenarios where we have to
		 *force the status of the IngestionJob to End_CanceledByUser status. In
		 *this case it is important to check if there are active associated
		 *EncodingJob (i.e. ToBeProcessed or Processing) and set them to
		 *End_CanceledByUser.
		 *
		 * Otherwise the EncodingJob, orphan of the IngestionJob, will remain
		 *definitevely in this 'active' state creating problems to the Engine.
		 * Also, these EncodingJobs may have also the processor field set to
		 *NULL (specially in case of ToBeProcessed) and therefore they will not
		 *managed by the reset procedure called when the Engine start.
		 *
		 *
		 */
		bool forceCancel = requestData.getQueryParameter("forceCancel", false);

		MMSEngineDBFacade::IngestionStatus ingestionStatus = _mmsEngineDBFacade->ingestionJob_Status(
			apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey,
			// 2022-12-18: meglio avere una info sicura
			true
		);

		if (!forceCancel && ingestionStatus != MMSEngineDBFacade::IngestionStatus::Start_TaskQueued)
		{
			string errorMessage = std::format(
				"The IngestionJob cannot be removed because of his Status"
				", ingestionJobKey: {}"
				", ingestionStatus: {}",
				ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus)
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		LOG_INFO(
			"Update IngestionJob"
			", ingestionJobKey: {}"
			", IngestionStatus: End_CanceledByUser"
			", errorMessage: ",
			ingestionJobKey
		);
		_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_CanceledByUser, "");

		if (forceCancel)
			_mmsEngineDBFacade->forceCancelEncodingJob(ingestionJobKey);

		string responseBody;
		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::updateIngestionJob(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "updateIngestionJob";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditMedia)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditMedia: {}",
			apiAuthorizationDetails->canEditMedia
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		try
		{
			LOG_INFO(
				"ingestionJob_IngestionTypeStatus"
				", workspace->_workspaceKey: {}"
				", ingestionJobKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey
			);

			auto [ingestionType, ingestionStatus] = _mmsEngineDBFacade->ingestionJob_IngestionTypeStatus(
				apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey,
				// 2022-12-18: meglio avere una informazione sicura
				true
			);

			if (ingestionStatus != MMSEngineDBFacade::IngestionStatus::Start_TaskQueued)
			{
				string errorMessage = std::format(
					"It is not possible to update an IngestionJob that "
					"it is not in Start_TaskQueued status"
					", ingestionJobKey: {}"
					", ingestionStatus: {}",
					ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus)
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			json metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);

			string field = "IngestionType";
			if (!JSONUtils::isPresent(metadataRoot, field))
			{
				string errorMessage = std::format(
					"IngestionType field is missing"
					", ingestionJobKey: {}",
					ingestionJobKey
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sIngestionType = JSONUtils::as<string>(metadataRoot, "IngestionType", "");

			if (sIngestionType == MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionType::LiveRecorder))
			{
				if (ingestionType != MMSEngineDBFacade::IngestionType::LiveRecorder)
				{
					string errorMessage = std::format(
						"It was requested an Update of Live-Recorder "
						"but IngestionType is not a LiveRecorder"
						", ingestionJobKey: {}"
						", ingestionType: {}",
						ingestionJobKey, MMSEngineDBFacade::toString(ingestionType)
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				{
					bool ingestionJobLabelModified = false;
					string newIngestionJobLabel;
					bool channelLabelModified = false;
					string newChannelLabel;
					bool recordingPeriodStartModified = false;
					string newRecordingPeriodStart;
					bool recordingPeriodEndModified = false;
					string newRecordingPeriodEnd;
					bool recordingVirtualVODModified = false;
					bool newRecordingVirtualVOD;

					{
						field = "IngestionJobLabel";
						if (JSONUtils::isPresent(metadataRoot, field))
						{
							ingestionJobLabelModified = true;
							newIngestionJobLabel = JSONUtils::as<string>(metadataRoot, "IngestionJobLabel", "");
						}

						field = "ChannelLabel";
						if (JSONUtils::isPresent(metadataRoot, field))
						{
							channelLabelModified = true;
							newChannelLabel = JSONUtils::as<string>(metadataRoot, "ChannelLabel", "");
						}

						field = "scheduleStart";
						if (JSONUtils::isPresent(metadataRoot, field))
						{
							recordingPeriodStartModified = true;
							newRecordingPeriodStart = JSONUtils::as<string>(metadataRoot, "scheduleStart", "");
						}

						field = "scheduleEnd";
						if (JSONUtils::isPresent(metadataRoot, field))
						{
							recordingPeriodEndModified = true;
							newRecordingPeriodEnd = JSONUtils::as<string>(metadataRoot, "scheduleEnd", "");
						}

						field = "RecordingVirtualVOD";
						if (JSONUtils::isPresent(metadataRoot, field))
						{
							recordingVirtualVODModified = true;
							newRecordingVirtualVOD = JSONUtils::as<bool>(metadataRoot, "RecordingVirtualVOD", false);
						}
					}

					if (recordingPeriodStartModified)
					{
						// Validator validator(_logger, _mmsEngineDBFacade,
						// _configuration);
						Datetime::parseStringToUtcInSecs(newRecordingPeriodStart);
					}

					if (recordingPeriodEndModified)
					{
						// Validator validator(_logger, _mmsEngineDBFacade,
						// _configuration);
						Datetime::parseStringToUtcInSecs(newRecordingPeriodEnd);
					}

					LOG_INFO(
						"Update IngestionJob"
						", workspaceKey: {}"
						", ingestionJobKey: {}",
						apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey
					);

					_mmsEngineDBFacade->updateIngestionJob_LiveRecorder(
						apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey, ingestionJobLabelModified, newIngestionJobLabel, channelLabelModified,
						newChannelLabel, recordingPeriodStartModified, newRecordingPeriodStart, recordingPeriodEndModified, newRecordingPeriodEnd,
						recordingVirtualVODModified, newRecordingVirtualVOD, apiAuthorizationDetails->admin
					);

					LOG_INFO(
						"IngestionJob updated"
						", workspaceKey: {}"
						", ingestionJobKey: {}",
						apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey
					);
				}
			}

			json responseRoot;
			responseRoot["status"] = string("success");

			string responseBody = JSONUtils::toString(responseRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"{} failed"
				", e.what(): {}",
				api, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::ingestionJobSwitchToEncoder(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "ingestionJobSwitchToEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditMedia)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditMedia: {}",
			apiAuthorizationDetails->canEditMedia
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		int64_t ingestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		// mandatory nel caso di broadcaster, servono:
		// 	- newPushEncoderKey e newPushPublicEncoderName per lo switch del broadcaster
		// 	- newEncodersPoolLabel per lo switch del broadcast
		// mandatory nel caso di Live-Proxy or VOD-Proxy or CountdownProxy, serve:
		// 	- newEncodersPoolLabel
		int64_t newPushEncoderKey = requestData.getQueryParameter("newPushEncoderKey", static_cast<int64_t>(-1), false);
		// newPushPublicEncoderName: indica se bisogna usare l'IP pubblico o quello interno/privato
		bool newPushPublicEncoderName = requestData.getQueryParameter("newPushPublicEncoderName", false, false);
		string newEncodersPoolLabel = requestData.getQueryParameter("newEncodersPoolLabel", string(""), false);

		LOG_INFO(
			"ingestionJobSwitchToEncoder"
			", workspace->_workspaceKey: {}"
			", ingestionJobKey: {}"
			", newPushEncoderKey: {}"
			", newPushPublicEncoderName: {}"
			", newEncodersPoolLabel: {}",
			apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey, newPushEncoderKey, newPushPublicEncoderName, newEncodersPoolLabel
		);

		auto [ingestionType, ingestionStatus, metadataContentRoot] = _mmsEngineDBFacade->ingestionJob_IngestionTypeStatusMetadataContent(
			apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey,
			// 2022-12-18: meglio avere una informazione sicura
			true
		);

		if (ingestionStatus != MMSEngineDBFacade::IngestionStatus::EncodingQueued)
		{
			string errorMessage = std::format(
				"It is not possible to switch to a new encoder when "
				"ingestionJob is not in EncodingQueued status"
				", ingestionJobKey: {}"
				", ingestionStatus: {}",
				ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus)
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// NOTA BENE: QUESTO METODO POTREBBE NON ESSERE COMPLETO

		if (ingestionType == MMSEngineDBFacade::IngestionType::LiveProxy)
		{
			json broadcasterRoot = JSONUtils::as<json>(metadataContentRoot["internalMMS"], "broadcaster");
			if (broadcasterRoot != nullptr)
			{
				// ingestionJobKey is referring a Broadcaster / Live Channel

				// verifica se newPushEncoderKey sia uno degli encoder gestiti dal workspace
				if (!_mmsEngineDBFacade->encoderWorkspaceMapping_isPresent(apiAuthorizationDetails->workspace->_workspaceKey, newPushEncoderKey))
				{
					string errorMessage = std::format(
						"EncoderKey is not managed by the workspaceKey"
						", ingestionJobKey: {}"
						", workspaceKey: {}"
						", newPushEncoderKey: {}",
						ingestionJobKey, apiAuthorizationDetails->workspace->_workspaceKey, newPushEncoderKey
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				// modifica broadcasterIngestionJob->metadataContentRoot in modo che l'engine faccia partire l'encodingJob su newPushEncoderKey
				{
					json internalMMSRoot = JSONUtils::as<json>(metadataContentRoot, "internalMMS");
					json encodersDetailsRoot = JSONUtils::as<json>(internalMMSRoot, "encodersDetails");
					if (encodersDetailsRoot == nullptr)
					{
						string errorMessage = std::format(
							"No encodersDetails json found"
							", ingestionJobKey: {}",
							ingestionJobKey
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					encodersDetailsRoot["pushEncoderKey"] = newPushEncoderKey;
					encodersDetailsRoot["pushPublicEncoderName"] = newPushPublicEncoderName;
					internalMMSRoot["encodersDetails"] = encodersDetailsRoot;
					metadataContentRoot["internalMMS"] = internalMMSRoot;

					_mmsEngineDBFacade->updateIngestionJobMetadataContent(ingestionJobKey, JSONUtils::toString(metadataContentRoot));

					auto [broadcasterEncodingJobKey, broadcasterEncoderKey] =
						_mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKey(ingestionJobKey, true);
					// dopo aver modificato il pushEncoderKey, killo l'encodingjob del broadcaster
					// solo per farlo ripartire in modo da usare il nuovo encoder
					try
					{
						killEncodingJob(broadcasterEncoderKey, ingestionJobKey, broadcasterEncodingJobKey, "killToRestartByEngine");
					}
					catch (...)
					{
						LOG_ERROR(
							"killEncodingJob (killToRestartByEngine) failed"
							", broadcasterEncoderKey: {}"
							", ingestionJobKey: {}"
							", broadcasterEncodingJobKey: {}",
							broadcasterEncoderKey, ingestionJobKey, broadcasterEncodingJobKey
						);
					}
				}

				int64_t broadcastIngestionJobKey = JSONUtils::as<int64_t>(broadcasterRoot, "broadcastIngestionJobKey", -1);

				// 1. modifica broadcastIngestionJob->metadataContentRoot in modo che l'engine faccia partire l'encodingJob su newEncodersPoolLabel
				// 2. modifica broadcastEncodingJob->outputsRoot[0]->udpUrl per farlo puntare al nuovo encoder/server su cui ascolta il broadcaster
				{
					auto [broadcastIngestionType, broadcastIngestionStatus, broadcastMetadataContentRoot] =
						_mmsEngineDBFacade->ingestionJob_IngestionTypeStatusMetadataContent(
							apiAuthorizationDetails->workspace->_workspaceKey, broadcastIngestionJobKey,
							// 2022-12-18: meglio avere una informazione sicura
							true
						);

					if (broadcastIngestionStatus != MMSEngineDBFacade::IngestionStatus::EncodingQueued)
					{
						string errorMessage = std::format(
							"It is not possible to switch to a new encoder when "
							"ingestionJob is not in EncodingQueued status"
							", broadcastIngestionJobKey: {}"
							", broadcastIngestionStatus: {}",
							broadcastIngestionJobKey, MMSEngineDBFacade::toString(broadcastIngestionStatus)
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					json internalMMSRoot = JSONUtils::as<json>(broadcastMetadataContentRoot, "internalMMS");
					json encodersDetailsRoot = JSONUtils::as<json>(internalMMSRoot, "encodersDetails");
					if (encodersDetailsRoot == nullptr)
					{
						string errorMessage = std::format(
							"No encodersDetails json found"
							", ingestionJobKey: {}",
							ingestionJobKey
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					encodersDetailsRoot["encodersPoolLabel"] = newEncodersPoolLabel;
					internalMMSRoot["encodersDetails"] = encodersDetailsRoot;
					broadcastMetadataContentRoot["internalMMS"] = internalMMSRoot;

					_mmsEngineDBFacade->updateIngestionJobMetadataContent(
						broadcastIngestionJobKey, JSONUtils::toString(broadcastMetadataContentRoot)
					);

					auto [broadcastEncodingJobKey, broadcastEncoderKey, broadcastEncodingJobParametersRoot] =
						_mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKeyParameters(broadcastIngestionJobKey, true);

					// nel caso del broadcast, è necessario anche aggiornare outputsRoot[0]->udpUrl
					// per farlo puntare al nuovo server su cui ascolta il broadcaster
					{
						string broadcasterStreamConfigurationLabel = JSONUtils::as<string>(metadataContentRoot, "configurationLabel");

						string newOutputUdpUrl = _mmsEngineDBFacade->getStreamPushServerUrl(
							apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey, broadcasterStreamConfigurationLabel, newPushEncoderKey,
							newPushPublicEncoderName, false
						);

						json outputsRoot = broadcastEncodingJobParametersRoot["outputsRoot"];
						json outputRoot = outputsRoot[0];
						outputRoot["udpUrl"] = newOutputUdpUrl;
						outputsRoot[0] = outputRoot;
						broadcastEncodingJobParametersRoot["outputsRoot"] = outputsRoot;
						_mmsEngineDBFacade->updateEncodingJobParameters(
							broadcastEncodingJobKey, JSONUtils::toString(broadcastEncodingJobParametersRoot)
						);
					}

					// dopo aver modificato il pushEncoderKey, killo l'encodingjob del broadcaster
					// solo per farlo ripartire in modo da usare il nuovo encoder
					try
					{
						killEncodingJob(broadcastEncoderKey, broadcastIngestionJobKey, broadcastEncodingJobKey, "killToRestartByEngine");
					}
					catch (...)
					{
						LOG_ERROR(
							"killEncodingJob (killToRestartByEngine) failed"
							", broadcastEncoderKey: {}"
							", broadcastIngestionJobKey: {}"
							", broadcastEncodingJobKey: {}",
							broadcastEncoderKey, broadcastIngestionJobKey, broadcastEncodingJobKey
						);
					}
				}
			}
			else
			{
				// modifica ingestionJob->metadataContentRoot in modo che l'engine faccia partire l'encodingJob su newEncodersPoolLabel

				auto [ingestionType, ingestionStatus, metadataContentRoot] = _mmsEngineDBFacade->ingestionJob_IngestionTypeStatusMetadataContent(
					apiAuthorizationDetails->workspace->_workspaceKey, ingestionJobKey,
					// 2022-12-18: meglio avere una informazione sicura
					true
				);

				if (ingestionStatus != MMSEngineDBFacade::IngestionStatus::EncodingQueued)
				{
					string errorMessage = std::format(
						"It is not possible to switch to a new encoder when "
						"ingestionJob is not in EncodingQueued status"
						", ingestionJobKey: {}"
						", ingestionStatus: {}",
						ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus)
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				json internalMMSRoot = JSONUtils::as<json>(metadataContentRoot, "internalMMS");
				json encodersDetailsRoot = JSONUtils::as<json>(internalMMSRoot, "encodersDetails");
				if (encodersDetailsRoot == nullptr)
				{
					string errorMessage = std::format(
						"No encodersDetails json found"
						", ingestionJobKey: {}",
						ingestionJobKey
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				encodersDetailsRoot["encodersPoolLabel"] = newEncodersPoolLabel;
				internalMMSRoot["encodersDetails"] = encodersDetailsRoot;
				metadataContentRoot["internalMMS"] = internalMMSRoot;

				_mmsEngineDBFacade->updateIngestionJobMetadataContent(ingestionJobKey, JSONUtils::toString(metadataContentRoot));

				auto [encodingJobKey, encoderKey] = _mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKey(ingestionJobKey, true);
				// dopo aver modificato il pushEncoderKey, killo l'encodingjob del broadcaster
				// solo per farlo ripartire in modo da usare il nuovo encoder
				try
				{
					killEncodingJob(encoderKey, ingestionJobKey, encodingJobKey, "killToRestartByEngine");
				}
				catch (...)
				{
					LOG_ERROR(
						"killEncodingJob (killToRestartByEngine) failed"
						", encoderKey: {}"
						", ingestionJobKey: {}"
						", encodingJobKey: {}",
						encoderKey, ingestionJobKey, encodingJobKey
					);
				}
			}
		}
		else
		{
			string errorMessage = std::format(
				"ingestionJobSwitchToEncoder. switch cannot be managed"
				", ingestionJobKey: {}"
				", ingestionType: {}",
				ingestionJobKey, MMSEngineDBFacade::toString(ingestionType)
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string responseBody;
		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::changeLiveProxyPlaylist(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "changeLiveProxyPlaylist";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	/* log già eseguito nel try con i parametri
	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);
	*/

	try
	{
		int64_t broadcasterIngestionJobKey = requestData.getQueryParameter("ingestionJobKey", -1, true);
		bool interruptPlaylist = requestData.getQueryParameter("interruptPlaylist", false);

		LOG_INFO(
			"Received {}"
			", broadcasterIngestionJobKey: {}"
			", interruptPlaylist: {}"
			", requestData.requestBody: {}",
			api, broadcasterIngestionJobKey, interruptPlaylist, requestData.requestBody
		);

		// next try/catch initialize the belows parameters using the broadcaster info

		// check of ingestion job and retrieve some fields
		int64_t utcBroadcasterStart;
		int64_t utcBroadcasterEnd;

		int64_t broadcastIngestionJobKey;
		string broadcastDefaultMediaType; // options: Stream, Media, Countdown,
										  // Direct URL
		// used in case mediaType is Stream
		json broadcastDefaultStreamInputRoot = nullptr;
		// used in case mediaType is Media
		json broadcastDefaultVodInputRoot = nullptr;
		// used in case mediaType is Countdown
		json broadcastDefaultCountdownInputRoot = nullptr;
		// used in case mediaType is Direct URL
		json broadcastDefaultDirectURLInputRoot = nullptr;
		try
		{
			LOG_INFO(
				"ingestionJob_IngestionTypeStatusMetadataContent"
				", workspace->_workspaceKey: {}"
				", broadcasterIngestionJobKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey
			);

			auto [ingestionType, ingestionStatus, metadataContentRoot] = _mmsEngineDBFacade->ingestionJob_IngestionTypeStatusMetadataContent(
				apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey,
				// 2022-12-18: meglio avere una informazione sicura
				true
			);

			if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy)
			{
				string errorMessage = std::format(
					"Ingestion type is not a Live/VODProxy"
					", broadcasterIngestionJobKey: {}"
					", ingestionType: {}",
					broadcasterIngestionJobKey, MMSEngineDBFacade::toString(ingestionType)
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);
			string prefixIngestionStatus = "End_";
			if (sIngestionStatus.starts_with(prefixIngestionStatus))
			{
				string errorMessage = std::format(
					"Ingestion job is already finished"
					", broadcasterIngestionJobKey: {}"
					", sIngestionStatus: {}"
					", ingestionType: {}",
					broadcasterIngestionJobKey, sIngestionStatus, MMSEngineDBFacade::toString(ingestionType)
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string field = "internalMMS";
			json internalMMSRoot = JSONUtils::as<json>(metadataContentRoot, field, json(), {}, true);

			field = "broadcaster";
			json broadcasterRoot = JSONUtils::as<json>(internalMMSRoot, field, json(), {}, true);

			field = "broadcastIngestionJobKey";
			broadcastIngestionJobKey = JSONUtils::as<int64_t>(broadcasterRoot, field, 0, {}, true);

			field = "schedule";
			json proxyPeriodRoot = JSONUtils::as<json>(metadataContentRoot, field, json(), {}, true);

			field = "timePeriod";
			bool timePeriod = JSONUtils::as<bool>(metadataContentRoot, field, false, {}, true);

			field = "start";
			string proxyPeriodStart = JSONUtils::as<string>(proxyPeriodRoot, field, "");
			utcBroadcasterStart = Datetime::parseStringToUtcInSecs(proxyPeriodStart);

			field = "end";
			string proxyPeriodEnd = JSONUtils::as<string>(proxyPeriodRoot, field, "");
			utcBroadcasterEnd = Datetime::parseStringToUtcInSecs(proxyPeriodEnd);

			field = "broadcastDefaultPlaylistItem";
			if (JSONUtils::isPresent(broadcasterRoot, field))
			{
				json broadcastDefaultPlaylistItemRoot = broadcasterRoot[field];

				field = "mediaType";
				if (JSONUtils::isPresent(broadcastDefaultPlaylistItemRoot, field))
				{
					broadcastDefaultMediaType = JSONUtils::as<string>(broadcastDefaultPlaylistItemRoot, field, "");

					if (broadcastDefaultMediaType == "Stream")
					{
						field = "streamConfigurationLabel";
						string broadcastDefaultConfigurationLabel = JSONUtils::as<string>(broadcastDefaultPlaylistItemRoot, field, "");
						int maxWidth = -1;
						string userAgent;
						string otherInputOptions;

						field = "filters";
						json filtersRoot;
						if (JSONUtils::isPresent(broadcastDefaultPlaylistItemRoot, field))
							filtersRoot = broadcastDefaultPlaylistItemRoot[field];

						broadcastDefaultStreamInputRoot = _mmsEngineDBFacade->getStreamInputRoot(
							apiAuthorizationDetails->workspace, broadcasterIngestionJobKey, broadcastDefaultConfigurationLabel, "",
							"", // useVideoTrackFromPhysicalPathName,
								// useVideoTrackFromPhysicalDeliveryURL
							maxWidth, userAgent, otherInputOptions, "", filtersRoot
						);
					}
					else if (broadcastDefaultMediaType == "Media")
					{
						vector<tuple<int64_t, string, string, string>> sources;

						MMSEngineDBFacade::ContentType vodContentType;

						field = "referencePhysicalPathKeys";
						if (JSONUtils::isPresent(broadcastDefaultPlaylistItemRoot, field))
						{
							json referencePhysicalPathKeysRoot = broadcastDefaultPlaylistItemRoot[field];

							int64_t firstMediaEncodingProfileKey = -2;
							for (int referencePhysicalPathKeyIndex = 0; referencePhysicalPathKeyIndex < referencePhysicalPathKeysRoot.size();
								 referencePhysicalPathKeyIndex++)
							{
								json referencePhysicalPathKeyRoot = referencePhysicalPathKeysRoot[referencePhysicalPathKeyIndex];

								int64_t broadcastDefaultPhysicalPathKey = JSONUtils::as<int64_t>(referencePhysicalPathKeyRoot, "physicalPathKey", -1);
								string broadcastDefaultTitle = JSONUtils::as<string>(referencePhysicalPathKeyRoot, "mediaItemTitle", "");

								// controllo che tutti i media usano lo stesso encoding profile
								{
									int64_t currentEncodingProfileKey = _mmsEngineDBFacade->physicalPath_columnAsInt64(
										"encodingprofilekey", broadcastDefaultPhysicalPathKey, nullptr, false
									);
									if (firstMediaEncodingProfileKey == -2) // primo media
										firstMediaEncodingProfileKey = currentEncodingProfileKey;
									else if (firstMediaEncodingProfileKey != currentEncodingProfileKey)
									{
										string errorMessage = std::format(
											"Media are not using the same encoding profile"
											", broadcasterIngestionJobKey: {}"
											", firstMediaEncodingProfileKey: {}"
											", currentEncodingProfileKey: {}",
											broadcasterIngestionJobKey, firstMediaEncodingProfileKey, currentEncodingProfileKey
										);
										LOG_ERROR(errorMessage);

										throw runtime_error(errorMessage);
									}
								}

								string sourcePhysicalPathName;
								{
									tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = _mmsStorage->getPhysicalPathDetails(
										broadcastDefaultPhysicalPathKey,
										// 2022-12-18: MIK dovrebbe
										// essere stato aggiunto da
										// tempo
										false
									);

									bool warningIfMissing = false;
									tie(ignore, vodContentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
										_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
											apiAuthorizationDetails->workspace->_workspaceKey, broadcastDefaultPhysicalPathKey, warningIfMissing,
											// 2022-12-18: MIK dovrebbe
											// essere stato aggiunto da
											// tempo
											false
										);
								}

								// int64_t durationInMilliSeconds =
								// 	_mmsEngineDBFacade->getMediaDurationInMilliseconds(
								// 	-1, broadcastDefaultPhysicalPathKey);

								// calculate delivery URL in case of an external
								// encoder
								string sourcePhysicalDeliveryURL;
								{
									int64_t utcNow;
									{
										chrono::system_clock::time_point now = chrono::system_clock::now();
										utcNow = chrono::system_clock::to_time_t(now);
									}

									tie(sourcePhysicalDeliveryURL, ignore) = _mmsDeliveryAuthorization->createDeliveryAuthorization(
										-1, // userKey,
										apiAuthorizationDetails->workspace,
										"", // clientIPAddress,

										-1, // mediaItemKey,
										"", // uniqueName,
										-1, // encodingProfileKey,
										"", // encodingProfileLabel,

										broadcastDefaultPhysicalPathKey,

										-1, // ingestionJobKey,	(in case of live)
										-1, // deliveryCode,
										nullopt,

										abs(utcNow - utcBroadcasterEnd), // ttlInSeconds,
										999999,							 // maxRetries,
										false,							 // reuseAuthIfPresent
										false,							 // playerIPToBeAuthorized
										"", "", nullopt, nullopt, nullopt,
										false,							 // save,
										"MMS_SignedURL",				 // deliveryType,

										false, // warningIfMissingMediaItemKey,
										true,  // filteredByStatistic
										""	   // userId (it is not needed
											   // it filteredByStatistic is
											   // true
									);
								}

								sources.emplace_back(
									broadcastDefaultPhysicalPathKey, broadcastDefaultTitle, sourcePhysicalPathName, sourcePhysicalDeliveryURL
								);
							}
						}

						json filtersRoot = JSONUtils::as<json>(broadcastDefaultPlaylistItemRoot, "filters", json());

						string otherInputOptions = JSONUtils::as<string>(broadcastDefaultPlaylistItemRoot, "otherInputOptions");

						/*
						if (JSONUtils::isPresent(broadcastDefaultPlaylistItemRoot, field))
							filtersRoot = broadcastDefaultPlaylistItemRoot[field];
						*/

						// the same json structure is used in
						// MMSEngineProcessor::manageVODProxy
						broadcastDefaultVodInputRoot = _mmsEngineDBFacade->getVodInputRoot(vodContentType, sources, filtersRoot, otherInputOptions);
					}
					else if (broadcastDefaultMediaType == "Countdown")
					{
						field = "physicalPathKey";
						int64_t broadcastDefaultPhysicalPathKey = JSONUtils::as<int64_t>(broadcastDefaultPlaylistItemRoot, field, -1);
						field = "text";
						string broadcastDefaultText = JSONUtils::as<string>(broadcastDefaultPlaylistItemRoot, field, "");
						field = "textPosition_X_InPixel";
						string broadcastDefaultTextPosition_X_InPixel = JSONUtils::as<string>(broadcastDefaultPlaylistItemRoot, field, "");
						field = "textPosition_Y_InPixel";
						string broadcastDefaultTextPosition_Y_InPixel = JSONUtils::as<string>(broadcastDefaultPlaylistItemRoot, field, "");

						MMSEngineDBFacade::ContentType vodContentType;
						string sourcePhysicalPathName;
						string sourcePhysicalDeliveryURL;
						int64_t videoDurationInMilliSeconds;
						{
							tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
								broadcastDefaultPhysicalPathKey,
								// 2022-12-18: MIK dovrebbe essere stato
								// aggiunto da tempo
								false
							);
							tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

							int64_t sourceMediaItemKey = -1;
							videoDurationInMilliSeconds = _mmsEngineDBFacade->getMediaDurationInMilliseconds(
								sourceMediaItemKey, broadcastDefaultPhysicalPathKey,
								// 2022-12-18: MIK dovrebbe essere stato
								// aggiunto da tempo
								false
							);

							// calculate delivery URL in case of an external
							// encoder
							{
								int64_t utcNow;
								{
									chrono::system_clock::time_point now = chrono::system_clock::now();
									utcNow = chrono::system_clock::to_time_t(now);
								}

								pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
									-1, // userKey,
									apiAuthorizationDetails->workspace,
									"", // clientIPAddress,

									-1, // mediaItemKey,
									"", // uniqueName,
									-1, // encodingProfileKey,
									"", // encodingProfileLabel,

									broadcastDefaultPhysicalPathKey,

									-1, // ingestionJobKey,	(in case of live)
									-1, // deliveryCode,
									nullopt,

									abs(utcNow - utcBroadcasterEnd), // ttlInSeconds,
									999999,							 // maxRetries,
									false,							 // reuseAuthIfPresent
									false,							 // playerIPToBeAuthorized
									"", "", nullopt, nullopt, nullopt,
									false,							 // save,
									"MMS_SignedURL",				 // deliveryType,

									false, // warningIfMissingMediaItemKey,
									true,  // filteredByStatistic
									""	   // userId (it is not needed it
										   // filteredByStatistic is true
								);

								tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
							}
						}

						// inizializza filtersRoot e verifica se drawtext is present
						bool isDrawTextFilterPresent = false;
						field = "filters";
						json filtersRoot;
						if (JSONUtils::isPresent(broadcastDefaultPlaylistItemRoot, field))
						{
							filtersRoot = broadcastDefaultPlaylistItemRoot["filters"];
							field = "video";
							if (JSONUtils::isPresent(filtersRoot, field))
							{
								json videoFiltersRoot = broadcastDefaultPlaylistItemRoot["video"];
								for (int videoFilterIndex = 0; videoFilterIndex < videoFiltersRoot.size(); videoFilterIndex++)
								{
									json videoFilterRoot = videoFiltersRoot[videoFilterIndex];
									field = "type";
									if (JSONUtils::isPresent(videoFilterRoot, field) && videoFilterRoot[field] == "drawtext")
										isDrawTextFilterPresent = true;
								}
							}
						}
						if (!isDrawTextFilterPresent)
						{
							string errorMessage = std::format(
								"Countdown has to have the drawText filter"
								", broadcasterIngestionJobKey: {}"
								", broadcastDefaultPlaylistItemRoot: {}",
								broadcasterIngestionJobKey, JSONUtils::toString(broadcastDefaultPlaylistItemRoot)
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						// the same json structure is used in
						// MMSEngineProcessor::manageVODProxy
						broadcastDefaultCountdownInputRoot = _mmsEngineDBFacade->getCountdownInputRoot(
							sourcePhysicalPathName, sourcePhysicalDeliveryURL, broadcastDefaultPhysicalPathKey, videoDurationInMilliSeconds,
							filtersRoot
						);
					}
					else if (broadcastDefaultMediaType == "Direct URL")
					{
						field = "url";
						string broadcastDefaultURL = JSONUtils::as<string>(broadcastDefaultPlaylistItemRoot, field, "");

						field = "filters";
						json filtersRoot = JSONUtils::as<json>(broadcastDefaultPlaylistItemRoot, field, json());

						broadcastDefaultDirectURLInputRoot = _mmsEngineDBFacade->getDirectURLInputRoot(broadcastDefaultURL, filtersRoot);
					}
					else
					{
						string errorMessage = std::format(
							"Broadcaster data: unknown MediaType"
							", broadcasterIngestionJobKey: {}"
							", broadcastDefaultMediaType: {}",
							broadcasterIngestionJobKey, broadcastDefaultMediaType
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				else
				{
					string errorMessage = std::format(
						"Broadcaster data: no mediaType is present"
						", broadcasterIngestionJobKey: {}",
						broadcasterIngestionJobKey
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string errorMessage = std::format(
					"Broadcaster data: no broadcastDefaultPlaylistItem is present"
					", broadcasterIngestionJobKey: {}",
					broadcasterIngestionJobKey
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"{} failed"
				", broadcasterIngestionJobKey: {}"
				", e.what(): {}",
				api, broadcasterIngestionJobKey, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// check/build the new playlist
		json newPlaylistRoot = json::array();
		try
		{
			json newReceivedPlaylistRoot = JSONUtils::toJson<json>(requestData.requestBody);

			// check the received playlist
			// in case of vodInput/countdownInput, the physicalPathKey is
			// received but we need to set vodContentType and
			// sourcePhysicalPathName
			{
				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < newReceivedPlaylistRoot.size(); newReceivedPlaylistIndex++)
				{
					json newReceivedPlaylistItemRoot = newReceivedPlaylistRoot[newReceivedPlaylistIndex];

					// aggiungo sUtcScheduleStart/End in modo da capire le date per chi vede la playlist (info di debug)
					{
						string sUtcScheduleStart;
						string sUtcScheduleEnd;
						if (JSONUtils::isPresent(newReceivedPlaylistItemRoot, "timePeriod") && newReceivedPlaylistItemRoot["timePeriod"])
						{
							if (JSONUtils::isPresent(newReceivedPlaylistItemRoot, "utcScheduleStart"))
							{
								sUtcScheduleStart = Datetime::utcToUtcString(newReceivedPlaylistItemRoot["utcScheduleStart"]);
								newReceivedPlaylistItemRoot["sUtcScheduleStart"] = sUtcScheduleStart;
							}
							if (JSONUtils::isPresent(newReceivedPlaylistItemRoot, "utcScheduleEnd"))
							{
								sUtcScheduleEnd = Datetime::utcToUtcString(newReceivedPlaylistItemRoot["utcScheduleEnd"]);
								newReceivedPlaylistItemRoot["sUtcScheduleEnd"] = sUtcScheduleEnd;
							}
						}
						LOG_INFO(
							"Processing newReceivedPlaylistRoot (the received one)"
							", broadcasterIngestionJobKey: {}"
							", newReceivedPlaylistRoot: {}/{}"
							", sUtcScheduleStart: {}"
							", sUtcScheduleEnd: {}",
							broadcasterIngestionJobKey, newReceivedPlaylistIndex, newReceivedPlaylistRoot.size(), sUtcScheduleStart, sUtcScheduleEnd
						);
					}
					{
						if (JSONUtils::isPresent(newReceivedPlaylistItemRoot, "streamInput"))
						{
							json streamInputRoot = newReceivedPlaylistItemRoot["streamInput"];

							streamInputRoot["filters"] = getReviewedFiltersRoot(streamInputRoot["filters"], apiAuthorizationDetails->workspace, -1);

							newReceivedPlaylistItemRoot["streamInput"] = streamInputRoot;
						}
						else if (JSONUtils::isPresent(newReceivedPlaylistItemRoot, "vodInput"))
						{
							json vodInputRoot = newReceivedPlaylistItemRoot["vodInput"];

							vodInputRoot["filters"] = getReviewedFiltersRoot(vodInputRoot["filters"], apiAuthorizationDetails->workspace, -1);

							// field = "sources";
							json sourcesRoot = JSONUtils::as<json>(vodInputRoot, "sources", json(), {}, true);

							if (sourcesRoot.size() == 0)
							{
								string errorMessage = std::format(
									"No source is present"
									", broadcasterIngestionJobKey: {}"
									", json data: {}",
									broadcasterIngestionJobKey, requestData.requestBody
								);
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}

							MMSEngineDBFacade::ContentType vodContentType;

							// viene creato uno nuovo perchè in caso di errori (es physicalPath che non esiste) qualche item di sourcesRoot potrebbe
							// essere eliminato
							json newSourcesRoot = json::array();

							int64_t firstMediaEncodingProfileKey = -2;
							for (int sourceIndex = 0; sourceIndex < sourcesRoot.size(); sourceIndex++)
							{
								json sourceRoot = sourcesRoot[sourceIndex];

								// field = "physicalPathKey";
								if (!JSONUtils::isPresent(sourceRoot, "physicalPathKey"))
								{
									string errorMessage = std::format("physicalPathKey is missing, json data: {}", requestData.requestBody);
									LOG_ERROR(errorMessage);

									throw runtime_error(errorMessage);
								}
								int64_t physicalPathKey = JSONUtils::as<int64_t>(sourceRoot, "physicalPathKey", -1);

								// controllo che tutti i media usano lo stesso encoding profile
								try
								{
									int64_t currentEncodingProfileKey =
										_mmsEngineDBFacade->physicalPath_columnAsInt64("encodingprofilekey", physicalPathKey, nullptr, false);
									if (firstMediaEncodingProfileKey == -2) // primo media
										firstMediaEncodingProfileKey = currentEncodingProfileKey;
									else if (firstMediaEncodingProfileKey != currentEncodingProfileKey)
									{
										LOG_ERROR(
											"PhysicalPath not using the same encoding profile, just skip it"
											", broadcasterIngestionJobKey: {}"
											", physicalPathKey: {}"
											", firstMediaEncodingProfileKey: {}"
											", currentEncodingProfileKey: {}",
											broadcasterIngestionJobKey, physicalPathKey, firstMediaEncodingProfileKey, currentEncodingProfileKey
										);
										continue;
									}
								}
								catch (DBRecordNotFound &e)
								{
									LOG_ERROR(
										"PhysicalPath not found, just skip it"
										", broadcasterIngestionJobKey: {}"
										", physicalPathKey: {}"
										", e.what(): {}",
										api, broadcasterIngestionJobKey, physicalPathKey, e.what()
									);
									continue;
								}

								string sourcePhysicalPathName;
								{
									tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
										physicalPathKey,
										// 2022-12-18: MIK dovrebbe
										// essere stato aggiunto da
										// tempo
										false
									);
									tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

									bool warningIfMissing = false;
									tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
										mediaItemKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
											apiAuthorizationDetails->workspace->_workspaceKey, physicalPathKey, warningIfMissing,
											// 2022-12-18: MIK dovrebbe
											// essere stato aggiunto da
											// tempo
											false
										);

									tie(ignore, vodContentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = mediaItemKeyDetails;
								}
								// field = "sourcePhysicalPathName";
								sourceRoot["sourcePhysicalPathName"] = sourcePhysicalPathName;

								// calculate delivery URL in case of an external
								// encoder
								string sourcePhysicalDeliveryURL;
								{
									int64_t utcNow;
									{
										chrono::system_clock::time_point now = chrono::system_clock::now();
										utcNow = chrono::system_clock::to_time_t(now);
									}

									pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
										-1, // userKey,
										apiAuthorizationDetails->workspace,
										"", // clientIPAddress,

										-1, // mediaItemKey,
										"", // uniqueName,
										-1, // encodingProfileKey,
										"", // encodingProfileLabel,

										physicalPathKey,

										-1, // ingestionJobKey,
											// (in case of live)
										-1, // deliveryCode,
										nullopt,

										abs(utcNow - utcBroadcasterEnd), // ttlInSeconds,
										999999,							 // maxRetries,
										false,							 // reuseAuthIfPresent
										false,							 // playerIPToBeAuthorized
										"", "", nullopt, nullopt,  nullopt,
										false,							 // save,
										"MMS_SignedURL",				 // deliveryType,

										false, // warningIfMissingMediaItemKey,
										true,  // filteredByStatistic
										""	   // userId (it is not
											   // needed it
											   // filteredByStatistic is
											   // true
									);

									tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
								}
								// field = "sourcePhysicalDeliveryURL";
								sourceRoot["sourcePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;

								// sourcesRoot[sourceIndex] = sourceRoot;
								newSourcesRoot.push_back(sourceRoot);
							}

							// vodInputRoot["sources"] = sourcesRoot;
							vodInputRoot["sources"] = newSourcesRoot;

							// field = "vodContentType";
							vodInputRoot["vodContentType"] = MMSEngineDBFacade::toString(vodContentType);

							// field = "vodInput";
							newReceivedPlaylistItemRoot["vodInput"] = vodInputRoot;
						}
						else if (JSONUtils::isPresent(newReceivedPlaylistItemRoot, "countdownInput"))
						{
							json countdownInputRoot = newReceivedPlaylistItemRoot["countdownInput"];

							countdownInputRoot["filters"] = getReviewedFiltersRoot(countdownInputRoot["filters"], apiAuthorizationDetails->workspace, -1);

							// field = "physicalPathKey";
							if (!JSONUtils::isPresent(countdownInputRoot, "physicalPathKey"))
							{
								string errorMessage = std::format("physicalPathKey is missing, json data: {}", requestData.requestBody);
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
							int64_t physicalPathKey = JSONUtils::as<int64_t>(countdownInputRoot, "physicalPathKey", -1);

							MMSEngineDBFacade::ContentType vodContentType;
							string sourcePhysicalPathName;
							int64_t videoDurationInMilliSeconds;
							{
								tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
									physicalPathKey,
									// 2022-12-18: MIK dovrebbe essere
									// stato aggiunto da tempo
									false
								);
								tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

								bool warningIfMissing = false;
								tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
									mediaItemKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
										apiAuthorizationDetails->workspace->_workspaceKey, physicalPathKey, warningIfMissing,
										// 2022-12-18: MIK dovrebbe
										// essere stato aggiunto da
										// tempo
										false
									);
								tie(ignore, vodContentType, ignore, ignore, ignore, ignore, ignore, ignore, videoDurationInMilliSeconds) =
									mediaItemKeyDetails;

								// videoDurationInMilliSeconds =
								// _mmsEngineDBFacade->getMediaDurationInMilliseconds(
								// 	-1, physicalPathKey);
							}

							// field = "mmsSourceVideoAssetPathName";
							countdownInputRoot["mmsSourceVideoAssetPathName"] = sourcePhysicalPathName;

							// field = "videoDurationInMilliSeconds";
							countdownInputRoot["videoDurationInMilliSeconds"] = videoDurationInMilliSeconds;

							// field = "vodContentType";
							countdownInputRoot["vodContentType"] = MMSEngineDBFacade::toString(vodContentType);

							// field = "countdownInput";
							newReceivedPlaylistItemRoot["countdownInput"] = countdownInputRoot;
						}
						else if (JSONUtils::isPresent(newReceivedPlaylistItemRoot, "directURLInput"))
						{
							json directURLInputRoot = newReceivedPlaylistItemRoot["directURLInput"];

							directURLInputRoot["filters"] = getReviewedFiltersRoot(directURLInputRoot["filters"], apiAuthorizationDetails->workspace, -1);

							newReceivedPlaylistItemRoot["directURLInput"] = directURLInputRoot;
						}

						newReceivedPlaylistRoot[newReceivedPlaylistIndex] = newReceivedPlaylistItemRoot;
					}
				}
			}

			// 2023-02-26: Probabilmente l'array di Json ricevuto è già
			// ordinato,
			//		per sicurezza ordiniamo in base al campo start
			//		Per utilizzare 'sort' inizializziamo un vector
			vector<json> vNewReceivedPlaylist;
			{
				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < newReceivedPlaylistRoot.size(); newReceivedPlaylistIndex++)
				{
					json newReceivedPlaylistItemRoot = newReceivedPlaylistRoot[newReceivedPlaylistIndex];
					vNewReceivedPlaylist.push_back(newReceivedPlaylistItemRoot);
				}

				sort(
					vNewReceivedPlaylist.begin(), vNewReceivedPlaylist.end(),
					[](json aRoot, json bRoot)
					{
						int64_t aUtcProxyPeriodStart = JSONUtils::as<int64_t>(aRoot, "utcScheduleStart", -1);
						int64_t bUtcProxyPeriodStart = JSONUtils::as<int64_t>(bRoot, "utcScheduleStart", -1);

						return aUtcProxyPeriodStart < bUtcProxyPeriodStart;
					}
				);

				LOG_INFO(
					"Sort playlist items"
					", broadcasterIngestionJobKey: {}"
					", vNewReceivedPlaylist.size: {}",
					broadcasterIngestionJobKey, vNewReceivedPlaylist.size()
				);
			}
			// 2023-02-26: ora che il vettore è ordinato, elimino gli elementi
			// precedenti a 'now - X days' (just a retention)
			{
				int32_t playlistItemsRetentionInHours = 3 * 24;
				if (apiAuthorizationDetails->workspace->_preferences != nullptr
					&& apiAuthorizationDetails->workspace->_preferences.contains("api") && apiAuthorizationDetails->workspace->_preferences["api"].is_object())
				{
					const json &apiRoot = apiAuthorizationDetails->workspace->_preferences["api"];
					if (apiRoot.contains("liveProxy") && apiRoot["liveProxy"].is_object())
					{
						const json &liveProxyRoot = apiRoot["liveProxy"];
						if (liveProxyRoot.contains("playlist") && liveProxyRoot["playlist"].is_object())
						{
							const json &playlistRoot = liveProxyRoot["playlist"];
							playlistItemsRetentionInHours = JSONUtils::as<int32_t>(playlistRoot, "retentionInHours", playlistItemsRetentionInHours);
						}
					}
				}
				chrono::system_clock::time_point now = chrono::system_clock::now();
				chrono::system_clock::time_point retention = now - chrono::hours(playlistItemsRetentionInHours);

				time_t utcRetention = chrono::system_clock::to_time_t(retention);

				int currentPlaylistIndex = -1;
				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < vNewReceivedPlaylist.size(); newReceivedPlaylistIndex++)
				{
					const json& newReceivedPlaylistItemRoot = vNewReceivedPlaylist[newReceivedPlaylistIndex];

					int64_t utcProxyPeriodStart = JSONUtils::as<int64_t>(newReceivedPlaylistItemRoot, "utcScheduleStart", -1);
					// int64_t utcProxyPeriodEnd = JSONUtils::as<int64_t>(newReceivedPlaylistItemRoot, "utcScheduleEnd", -1);

					if (newReceivedPlaylistIndex != 0 && utcProxyPeriodStart >= utcRetention)
					{
						currentPlaylistIndex = newReceivedPlaylistIndex - 1;

						break;
					}
					/*
					if (utcProxyPeriodStart <= utcRetention && utcRetention < utcProxyPeriodEnd)
					{
						currentPlaylistIndex = newReceivedPlaylistIndex;

						break;
					}
					*/
				}
				int leavePastEntriesNumber = 1;
				if (currentPlaylistIndex - leavePastEntriesNumber > 0)
				{
					LOG_INFO(
						"Erase playlist items in the past: {} items"
						", broadcasterIngestionJobKey: {}"
						", playlistItemsRetentionInHours: {}"
						", currentPlaylistIndex: {}"
						", leavePastEntriesNumber: {}"
						", vNewReceivedPlaylist.size: {}",
						currentPlaylistIndex - leavePastEntriesNumber, broadcasterIngestionJobKey, playlistItemsRetentionInHours,
						currentPlaylistIndex, leavePastEntriesNumber, vNewReceivedPlaylist.size()
					);

					vNewReceivedPlaylist.erase(
						vNewReceivedPlaylist.begin(), vNewReceivedPlaylist.begin() + (currentPlaylistIndex - leavePastEntriesNumber)
					);
				}
				else
				{
					LOG_INFO(
						"Erase playlist items in the past: nothing"
						", broadcasterIngestionJobKey: {}"
						", playlistItemsRetentionInHours: {}"
						", currentPlaylistIndex: {}"
						", leavePastEntriesNumber: {}"
						", vNewReceivedPlaylist.size: {}",
						broadcasterIngestionJobKey, playlistItemsRetentionInHours, currentPlaylistIndex, leavePastEntriesNumber,
						vNewReceivedPlaylist.size()
					);
				}
			}

			// build the new playlist
			// add the default media in case of hole filling newPlaylistRoot
			// genero un errore in caso di sovrapposizioni tra gli items della playlist
			{
				int64_t utcCurrentBroadcasterStart = utcBroadcasterStart;

				for (int newReceivedPlaylistIndex = 0; newReceivedPlaylistIndex < vNewReceivedPlaylist.size(); newReceivedPlaylistIndex++)
				{
					json newReceivedPlaylistItemRoot = vNewReceivedPlaylist[newReceivedPlaylistIndex];

					// correct values have to be:
					//	utcCurrentBroadcasterStart <= utcProxyPeriodStart < utcProxyPeriodEnd
					// the last utcProxyPeriodEnd has to be equal to utcBroadcasterEnd
					string field = "utcScheduleStart";
					int64_t utcProxyPeriodStart = JSONUtils::as<int64_t>(newReceivedPlaylistItemRoot, field, -1);
					field = "utcScheduleEnd";
					int64_t utcProxyPeriodEnd = JSONUtils::as<int64_t>(newReceivedPlaylistItemRoot, field, -1);

					LOG_INFO(
						"Processing newReceivedPlaylistRoot"
						", broadcasterIngestionJobKey: {}"
						", newReceivedPlaylistRoot: {}/{}"
						", utcCurrentBroadcasterStart: {} ({})"
						", utcProxyPeriodStart: {} ({})"
						", utcProxyPeriodEnd: {} ({})",
						broadcasterIngestionJobKey, newReceivedPlaylistIndex, newReceivedPlaylistRoot.size(), utcCurrentBroadcasterStart,
						Datetime::utcToUtcString(utcCurrentBroadcasterStart), utcProxyPeriodStart,
						Datetime::utcToUtcString(utcProxyPeriodStart),
						utcProxyPeriodEnd, Datetime::utcToUtcString(utcProxyPeriodEnd)
					);

					if (utcCurrentBroadcasterStart > utcProxyPeriodStart || utcProxyPeriodStart >= utcProxyPeriodEnd ||
						utcProxyPeriodEnd > utcBroadcasterEnd)
					{
						string partialMessage;

						if (utcCurrentBroadcasterStart > utcProxyPeriodStart)
							partialMessage = std::format(
								"utcCurrentBroadcasterStart {} ({}) > utcProxyPeriodStart {} ({})", utcCurrentBroadcasterStart,
								Datetime::utcToUtcString(utcCurrentBroadcasterStart), utcProxyPeriodStart,
								Datetime::utcToUtcString(utcProxyPeriodStart)
							);
						else if (utcProxyPeriodStart >= utcProxyPeriodEnd)
							partialMessage = std::format(
								"utcProxyPeriodStart {} ({}) >= utcProxyPeriodEnd {} ({})", utcProxyPeriodStart,
								Datetime::utcToUtcString(utcProxyPeriodStart), utcProxyPeriodEnd,
								Datetime::utcToUtcString(utcProxyPeriodEnd)
							);
						else if (utcProxyPeriodEnd > utcBroadcasterEnd)
							partialMessage = std::format(
								"utcProxyPeriodEnd {} ({}) > utcBroadcasterEnd {} ({})", utcProxyPeriodEnd,
								Datetime::utcToUtcString(utcProxyPeriodEnd), utcBroadcasterEnd,
								Datetime::utcToUtcString(utcBroadcasterEnd)
							);

						string errorMessage = std::format(
							"Wrong dates ({})"
							", newReceivedPlaylistIndex: {}",
							partialMessage, newReceivedPlaylistIndex
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					if (utcProxyPeriodStart == utcCurrentBroadcasterStart)
						newPlaylistRoot.push_back(newReceivedPlaylistItemRoot);
					else // if (utcCurrentBroadcasterStart <
						 // utcProxyPeriodStart)
					{
						json newdPlaylistItemToBeAddedRoot;

						field = "defaultBroadcast";
						newdPlaylistItemToBeAddedRoot[field] = true;

						field = "timePeriod";
						newdPlaylistItemToBeAddedRoot[field] = true;

						field = "utcScheduleStart";
						newdPlaylistItemToBeAddedRoot[field] = utcCurrentBroadcasterStart;

						field = "utcScheduleEnd";
						newdPlaylistItemToBeAddedRoot[field] = utcProxyPeriodStart;

						if (broadcastDefaultMediaType == "Stream")
						{
							if (broadcastDefaultStreamInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["streamInput"] = broadcastDefaultStreamInputRoot;
							else
							{
								string errorMessage = std::format(
									"Broadcaster data: no default Stream present"
									", broadcasterIngestionJobKey: {}",
									broadcasterIngestionJobKey
								);
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (broadcastDefaultMediaType == "Media")
						{
							if (broadcastDefaultVodInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["vodInput"] = broadcastDefaultVodInputRoot;
							else
							{
								string errorMessage = std::format(
									"Broadcaster data: no default Media present"
									", broadcasterIngestionJobKey: {}",
									broadcasterIngestionJobKey
								);
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (broadcastDefaultMediaType == "Countdown")
						{
							if (broadcastDefaultCountdownInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["countdownInput"] = broadcastDefaultCountdownInputRoot;
							else
							{
								string errorMessage = std::format(
									"Broadcaster data: no default Countdown present"
									", broadcasterIngestionJobKey: {}",
									broadcasterIngestionJobKey
								);
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (broadcastDefaultMediaType == "Direct URL")
						{
							if (broadcastDefaultDirectURLInputRoot != nullptr)
								newdPlaylistItemToBeAddedRoot["directURLInput"] = broadcastDefaultDirectURLInputRoot;
							else
							{
								string errorMessage = std::format(
									"Broadcaster data: no default DirectURL present"
									", broadcasterIngestionJobKey: {}",
									broadcasterIngestionJobKey
								);
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: unknown MediaType"
								", broadcasterIngestionJobKey: {}"
								", broadcastDefaultMediaType: {}",
								broadcasterIngestionJobKey, broadcastDefaultMediaType
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						newPlaylistRoot.push_back(newdPlaylistItemToBeAddedRoot);

						newPlaylistRoot.push_back(newReceivedPlaylistItemRoot);
					}
					utcCurrentBroadcasterStart = utcProxyPeriodEnd;
				}

				if (vNewReceivedPlaylist.size() == 0)
				{
					// no items inside the playlist

					json newdPlaylistItemToBeAddedRoot;

					string field = "defaultBroadcast";
					newdPlaylistItemToBeAddedRoot[field] = true;

					field = "timePeriod";
					newdPlaylistItemToBeAddedRoot[field] = true;

					field = "utcScheduleStart";
					newdPlaylistItemToBeAddedRoot[field] = utcBroadcasterStart;

					field = "utcScheduleEnd";
					newdPlaylistItemToBeAddedRoot[field] = utcBroadcasterEnd;

					if (broadcastDefaultMediaType == "Stream")
					{
						if (broadcastDefaultStreamInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["streamInput"] = broadcastDefaultStreamInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Stream present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Media")
					{
						if (broadcastDefaultVodInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["vodInput"] = broadcastDefaultVodInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Media present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Countdown")
					{
						if (broadcastDefaultCountdownInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["countdownInput"] = broadcastDefaultCountdownInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Countdown present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Direct URL")
					{
						if (broadcastDefaultDirectURLInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["directURLInput"] = broadcastDefaultDirectURLInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Direct URL present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						string errorMessage = std::format(
							"Broadcaster data: unknown MediaType"
							", broadcasterIngestionJobKey: {}"
							", broadcastDefaultMediaType: {}",
							broadcasterIngestionJobKey, broadcastDefaultMediaType
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					newPlaylistRoot.push_back(newdPlaylistItemToBeAddedRoot);
				}
				else if (utcCurrentBroadcasterStart < utcBroadcasterEnd)
				{
					// last period has to be added

					json newdPlaylistItemToBeAddedRoot;

					string field = "defaultBroadcast";
					newdPlaylistItemToBeAddedRoot[field] = true;

					field = "timePeriod";
					newdPlaylistItemToBeAddedRoot[field] = true;

					field = "utcScheduleStart";
					newdPlaylistItemToBeAddedRoot[field] = utcCurrentBroadcasterStart;

					field = "utcScheduleEnd";
					newdPlaylistItemToBeAddedRoot[field] = utcBroadcasterEnd;

					if (broadcastDefaultMediaType == "Stream")
					{
						if (broadcastDefaultStreamInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["streamInput"] = broadcastDefaultStreamInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Stream present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Media")
					{
						if (broadcastDefaultVodInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["vodInput"] = broadcastDefaultVodInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Media present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Countdown")
					{
						if (broadcastDefaultCountdownInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["countdownInput"] = broadcastDefaultCountdownInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Countdown present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else if (broadcastDefaultMediaType == "Direct URL")
					{
						if (broadcastDefaultDirectURLInputRoot != nullptr)
							newdPlaylistItemToBeAddedRoot["directURLInput"] = broadcastDefaultDirectURLInputRoot;
						else
						{
							string errorMessage = std::format(
								"Broadcaster data: no default Direct URL present"
								", broadcasterIngestionJobKey: {}",
								broadcasterIngestionJobKey
							);
							LOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						string errorMessage = std::format(
							"Broadcaster data: unknown MediaType"
							", broadcasterIngestionJobKey: {}"
							", broadcastDefaultMediaType: {}",
							broadcasterIngestionJobKey, broadcastDefaultMediaType
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					newPlaylistRoot.push_back(newdPlaylistItemToBeAddedRoot);
				}
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"{} failed"
				", broadcasterIngestionJobKey: {}"
				", e.what(): {}",
				api, broadcasterIngestionJobKey, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// 2021-12-22: For sure we will have the BroadcastIngestionJob.
		//		We may not have the EncodingJob in case it has to be
		// executed in the future 		In this case we will save the playlist into the
		// broadcast ingestion job
		string ffmpegEncoderURL;
		ostringstream response;
		try
		{
			LOG_INFO(
				"ingestionJob_IngestionTypeMetadataContent"
				", workspace->_workspaceKey: {}"
				", broadcasterIngestionJobKey: {}"
				", broadcastIngestionJobKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey, broadcastIngestionJobKey
			);

			auto [ingestionType, metadataContentRoot] = _mmsEngineDBFacade->ingestionJob_IngestionTypeMetadataContent(
				apiAuthorizationDetails->workspace->_workspaceKey, broadcastIngestionJobKey,
				// 2022-12-18: meglio avere una informazione sicura
				true
			);

			if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy && ingestionType != MMSEngineDBFacade::IngestionType::VODProxy &&
				ingestionType != MMSEngineDBFacade::IngestionType::Countdown)
			{
				string errorMessage = std::format(
					"Ingestion type is not a LiveProxy-VODProxy-Countdown"
					", broadcasterIngestionJobKey: {}"
					", broadcastIngestionJobKey: {}"
					", ingestionType: {}",
					broadcasterIngestionJobKey, broadcastIngestionJobKey, MMSEngineDBFacade::toString(ingestionType)
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string newPlaylist = JSONUtils::toString(newPlaylistRoot);

			json broadcastParametersRoot;
			int64_t broadcastEncodingJobKey = -1;
			int64_t broadcastEncoderKey = -1;
			try
			{
				tie(broadcastEncodingJobKey, broadcastEncoderKey, broadcastParametersRoot) =
					_mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKeyParameters(
						broadcastIngestionJobKey,
						// 2022-12-18: l'IngestionJob potrebbe essere stato
						// appena aggiunto
						true
					);
			}
			catch (exception &e)
			{
				LOG_WARN(e.what());

				// throw;
			}

			// we may have the scenario where the encodingJob is not present
			// because will be executed in the future
			if (broadcastEncodingJobKey != -1)
			{
				// update of the parameters
				string field = "inputsRoot";
				broadcastParametersRoot[field] = newPlaylistRoot;

				string newBroadcastParameters = JSONUtils::toString(broadcastParametersRoot);

				_mmsEngineDBFacade->updateEncodingJobParameters(broadcastEncodingJobKey, newBroadcastParameters);

				// we may have the scenario where the encodingJob is present but
				// it is not running (timing to be run in the future). In this
				// case broadcastEncoderKey will be -1
				if (broadcastEncoderKey > 0)
				{
					string transcoderHost;
					tie(transcoderHost, ignore) = _mmsEngineDBFacade->getEncoderURL(broadcastEncoderKey);

					ffmpegEncoderURL = std::format("{}{}/{}?interruptPlaylist={}", transcoderHost,
						_ffmpegEncoderChangeLiveProxyPlaylistURI, broadcastEncodingJobKey, interruptPlaylist);

					LOG_INFO(
						"Calling the encoder changeLiveProxy"
						", broadcasterIngestionJobKey: {}"
						", broadcastIngestionJobKey: {}"
						", broadcastEncodingJobKey: {}"
						", ffmpegEncoderURL: {}",
						broadcasterIngestionJobKey, broadcastIngestionJobKey, broadcastEncodingJobKey, ffmpegEncoderURL
					);
					vector<string> otherHeaders;
					json encoderResponse = CurlWrapper::httpPutStringAndGetJson(
						ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, CurlWrapper::basicAuthorization(_ffmpegEncoderUser, _ffmpegEncoderPassword),
						newPlaylist,
						"application/json", // contentType
						otherHeaders, std::format(", ingestionJobKey: {}", broadcasterIngestionJobKey)
					);
				}
				else
					LOG_INFO(
						"broadcastEncoderKey was not found, the IngestionJob is updated"
						", broadcasterIngestionJobKey: {}"
						", broadcastIngestionJobKey: {}"
						", broadcastEncodingJobKey: {}"
						", broadcastEncoderKey: {}",
						broadcasterIngestionJobKey, broadcastIngestionJobKey, broadcastEncodingJobKey, broadcastEncoderKey
					);
				}
			else
			{
				LOG_INFO(
					"The Broadcast EncodingJob was not found, the IngestionJob is updated"
					", broadcasterIngestionJobKey: {}"
					", broadcastIngestionJobKey: {}"
					", broadcastEncodingJobKey: {}",
					broadcasterIngestionJobKey, broadcastIngestionJobKey, broadcastEncodingJobKey
				);

				// update of the parameters
				json mmsInternalRoot;
				json broadcasterRoot;

				string field = "internalMMS";
				if (JSONUtils::isPresent(metadataContentRoot, field))
					mmsInternalRoot = metadataContentRoot[field];

				field = "broadcaster";
				if (JSONUtils::isPresent(mmsInternalRoot, field))
					broadcasterRoot = mmsInternalRoot[field];

				field = "broadcasterInputsRoot";
				broadcasterRoot[field] = newPlaylistRoot;

				field = "broadcaster";
				mmsInternalRoot[field] = broadcasterRoot;

				field = "internalMMS";
				metadataContentRoot[field] = mmsInternalRoot;

				string newMetadataContentRoot = JSONUtils::toString(metadataContentRoot);

				_mmsEngineDBFacade->updateIngestionJobMetadataContent(broadcastIngestionJobKey, newMetadataContentRoot);
			}

			string responseBody;
			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"{} failed"
				", broadcasterIngestionJobKey: {}"
				", ffmpegEncoderURL: {}"
				", response.str: {}"
				", e.what(): {}",
				api, broadcasterIngestionJobKey, ffmpegEncoderURL, response.str(), e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"{} failed"
			", workspace->_workspaceKey: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::changeLiveProxyOverlayText(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "changeLiveProxyOverlayText";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t broadcasterIngestionJobKey = requestData.getQueryParameter("ingestionJobKey", static_cast<int64_t>(-1), true);

		LOG_INFO("{}, broadcasterIngestionJobKey: {}", api, broadcasterIngestionJobKey);

		try
		{
			{
				LOG_INFO(
					"ingestionJobQuery"
					", workspace->_workspaceKey: {}"
					", broadcasterIngestionJobKey: {}",
					apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey
				);

				auto [ingestionType, ingestionStatus] =
					_mmsEngineDBFacade->ingestionJob_IngestionTypeStatus(apiAuthorizationDetails->workspace->_workspaceKey, broadcasterIngestionJobKey, false);

				if (ingestionType != MMSEngineDBFacade::IngestionType::LiveProxy)
				{
					string errorMessage = std::format(
						"Ingestion type is not a Live/VODProxy"
						", broadcasterIngestionJobKey: {}"
						", ingestionType: {}",
						broadcasterIngestionJobKey, MMSEngineDBFacade::toString(ingestionType)
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);
				string prefixIngestionStatus = "End_";
				if (sIngestionStatus.starts_with("End_"))
				{
					string errorMessage = std::format(
						"Ingestion job is already finished"
						", broadcasterIngestionJobKey: {}"
						", sIngestionStatus: {}"
						", ingestionType: {}",
						broadcasterIngestionJobKey, sIngestionStatus, MMSEngineDBFacade::toString(ingestionType)
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			int64_t broadcasterEncodingJobKey;
			int64_t broadcasterEncoderKey;
			{
				LOG_INFO(
					"encodingJobQuery"
					", broadcasterIngestionJobKey: {}",
					broadcasterIngestionJobKey
				);

				tie(broadcasterEncodingJobKey, broadcasterEncoderKey) =
					_mmsEngineDBFacade->encodingJob_EncodingJobKeyEncoderKey(broadcasterIngestionJobKey, false);

				if (broadcasterEncodingJobKey == -1 || broadcasterEncoderKey == -1)
				{
					string errorMessage = std::format(
						"encodingJobKey and/or encoderKey not found"
						", broadcasterEncodingJobKey: {}",
						", broadcasterEncoderKey: {}", ", broadcasterIngestionJobKey: {}", broadcasterEncodingJobKey, broadcasterEncoderKey,
						broadcasterIngestionJobKey
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			{
				string encoderURL;
				tie(encoderURL, ignore) = _mmsEngineDBFacade->getEncoderURL(broadcasterEncoderKey);

				string ffmpegEncoderURL = std::format("{}{}/{}", encoderURL, _ffmpegEncoderChangeLiveProxyOverlayTextURI, broadcasterEncodingJobKey);

				vector<string> otherHeaders;
				CurlWrapper::httpPutStringAndGetJson(
					ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, CurlWrapper::basicAuthorization(_ffmpegEncoderUser, _ffmpegEncoderPassword),
					string(requestData.requestBody),
					"text/plain", // contentType
					otherHeaders, std::format(", ingestionJobKey: {}", broadcasterIngestionJobKey)
				);
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string responseBody;
		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

// LO STESSO METODO E' IN MMSEngineProcessor.cpp
json API::getReviewedFiltersRoot(json filtersRoot, const shared_ptr<Workspace>& workspace, int64_t ingestionJobKey)
{
	if (filtersRoot == nullptr)
		return filtersRoot;

	/*
	LOG_INFO(
		"getReviewedFiltersRoot in"
		", filters: {}",
		JSONUtils::toString(filtersRoot)
	);
	*/

	// se viene usato il filtro imageoverlay, è necessario recuperare sourcePhysicalPathName e sourcePhysicalDeliveryURL
	if (JSONUtils::isPresent(filtersRoot, "complex"))
	{
		json complexFiltersRoot = filtersRoot["complex"];
		for (int complexFilterIndex = 0; complexFilterIndex < complexFiltersRoot.size(); complexFilterIndex++)
		{
			json complexFilterRoot = complexFiltersRoot[complexFilterIndex];
			if (JSONUtils::isPresent(complexFilterRoot, "type") && complexFilterRoot["type"] == "imageoverlay")
			{
				if (!JSONUtils::isPresent(complexFilterRoot, "imagePhysicalPathKey"))
				{
					string errorMessage = std::format(
						"imageoverlay filter without imagePhysicalPathKey"
						", ingestionJobKey: {}"
						", imageoverlay filter: {}",
						ingestionJobKey, JSONUtils::toString(complexFilterRoot)
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				string sourcePhysicalPathName;
				{
					tuple<string, int, string, string, int64_t, string> physicalPathDetails =
						_mmsStorage->getPhysicalPathDetails(complexFilterRoot["imagePhysicalPathKey"], false);
					tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
				}

				// calculate delivery URL in case of an external encoder
				string sourcePhysicalDeliveryURL;
				{
					int64_t utcNow;
					{
						chrono::system_clock::time_point now = chrono::system_clock::now();
						utcNow = chrono::system_clock::to_time_t(now);
					}

					pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
						-1, // userKey,
						workspace,
						"", // clientIPAddress,

						-1, // mediaItemKey,
						"", // uniqueName,
						-1, // encodingProfileKey,
						"", // encodingProfileLabel,

						complexFilterRoot["imagePhysicalPathKey"],

						-1, // ingestionJobKey,	(in case of live)
						-1, // deliveryCode,
						nullopt,

						365 * 24 * 60 * 60, // ttlInSeconds, 365 days!!!
						999999,				// maxRetries,
						false,				// reuseAuthIfPresent
						false,				// playerIPToBeAuthorized
						"", "", nullopt, nullopt,  nullopt,
						false,				// save,
						"MMS_SignedURL",	// deliveryType,

						false, // warningIfMissingMediaItemKey,
						true,  // filteredByStatistic
						""	   // userId (it is not needed it
							   // filteredByStatistic is true
					);

					tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
				}

				complexFilterRoot["imagePhysicalPathName"] = sourcePhysicalPathName;
				complexFilterRoot["imagePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;
				complexFiltersRoot[complexFilterIndex] = complexFilterRoot;
			}
		}
		filtersRoot["complex"] = complexFiltersRoot;
	}

	/*
	LOG_INFO(
		"getReviewedFiltersRoot out"
		", filters: {}",
		JSONUtils::toString(filtersRoot)
	);
	*/

	return filtersRoot;
}
