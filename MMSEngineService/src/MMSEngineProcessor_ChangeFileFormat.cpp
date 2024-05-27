
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"
/*
#include <stdio.h>

#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "GEOInfoTimes.h"
#include "MMSCURL.h"
#include "PersistenceLock.h"
#include "ThreadsStatisticTimes.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/System.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
// #include "EMailSender.h"
#include "Magick++.h"
// #include <openssl/md5.h>
#include "spdlog/spdlog.h"
#include <openssl/evp.h>

#define MD5BUFFERSIZE 16384
*/

void MMSEngineProcessor::changeFileFormatThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "changeFileFormatThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "changeFileFormatThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured media to be used to changeFileFormat" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		string outputFileFormat;
		{
			string field = "outputFileFormat";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			outputFileFormat = JSONUtils::asString(parametersRoot, field, "");
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				int64_t mediaItemKey;
				int64_t physicalPathKey;
				string mmsSourceAssetPathName;
				string relativePath;

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					mediaItemKey = key;
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						mediaItemKey, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(physicalPathKey, mmsSourceAssetPathName, ignore, relativePath, ignore, ignore, ignore) = physicalPathDetails;
				}
				else
				{
					physicalPathKey = key;

					tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						physicalPathKey,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(mmsSourceAssetPathName, ignore, relativePath, ignore, ignore, ignore) = physicalPathDetails;

					bool warningIfMissing = false;
					tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemDetails =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, physicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato
							// appena aggiunto
							true
						);
					tie(mediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) = mediaItemDetails;
				}

				vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
				vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

				_mmsEngineDBFacade->getVideoDetails(
					mediaItemKey, physicalPathKey,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true, videoTracks, audioTracks
				);

				// add the new file as a new variant of the MIK
				{
					string changeFormatFileName = to_string(ingestionJobKey) + "_" + to_string(mediaItemKey) + "_changeFileFormat";
					if (outputFileFormat == "m3u8-tar.gz") // || outputFileFormat == "m3u8-streaming")
						changeFormatFileName += ".m3u8";
					else if (outputFileFormat == "streaming-to-mp4")
						changeFormatFileName += ".mp4";
					else
						changeFormatFileName += (string(".") + outputFileFormat);

					string stagingChangeFileFormatAssetPathName;
					{
						bool removeLinuxPathIfExist = true;
						bool neededForTranscoder = false;
						stagingChangeFileFormatAssetPathName = _mmsStorage->getStagingAssetPathName(
							neededForTranscoder, workspace->_directoryName, to_string(ingestionJobKey), "/", changeFormatFileName,
							-1, // _encodingItem->_mediaItemKey, not used
								// because encodedFileName is not ""
							-1, // _encodingItem->_physicalPathKey, not used
								// because encodedFileName is not ""
							removeLinuxPathIfExist
						);
					}

					try
					{
						SPDLOG_INFO(
							string() + "Calling ffmpeg.changeFileFormat" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName + ", changeFormatFileName: " + changeFormatFileName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName
						);

						FFMpeg ffmpeg(_configurationRoot, _logger);

						ffmpeg.changeFileFormat(
							ingestionJobKey, physicalPathKey, mmsSourceAssetPathName, videoTracks, audioTracks, stagingChangeFileFormatAssetPathName,
							outputFileFormat
						);

						SPDLOG_INFO(
							string() + "ffmpeg.changeFileFormat done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName + ", changeFormatFileName: " + changeFormatFileName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName
						);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "ffmpeg.changeFileFormat failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName + ", e.what(): " + e.what()
						);

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "ffmpeg.changeFileFormat failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName + ", e.what(): " + e.what()
						);

						throw e;
					}

					tuple<int64_t, long, json> mediaInfoDetails;
					vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
					vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;

					int imageWidth = -1;
					int imageHeight = -1;
					string imageFormat;
					int imageQuality = -1;
					try
					{
						SPDLOG_INFO(
							string() + "Calling ffmpeg.getMediaInfo" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", _ingestionJobKey: " + to_string(ingestionJobKey) +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName
						);
						int timeoutInSeconds = 20;
						bool isMMSAssetPathName = true;
						FFMpeg ffmpeg(_configurationRoot, _logger);
						mediaInfoDetails = ffmpeg.getMediaInfo(
							ingestionJobKey, isMMSAssetPathName, timeoutInSeconds, stagingChangeFileFormatAssetPathName, videoTracks, audioTracks
						);

						// tie(durationInMilliSeconds, bitRate,
						// 	videoCodecName, videoProfile, videoWidth,
						// videoHeight, videoAvgFrameRate, videoBitRate,
						// 	audioCodecName, audioSampleRate, audioChannels,
						// audioBitRate) = mediaInfo;
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "getMediaInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", _ingestionJobKey: " +
							to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName +
							", _workspace->_directoryName: " + workspace->_directoryName + ", e.what(): " + e.what()
						);

						{
							string directoryPathName;
							try
							{
								size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
								if (endOfDirectoryIndex != string::npos)
								{
									directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

									SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
									fs::remove_all(directoryPathName);
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeDirectory failed" + ", _ingestionJobKey: " + to_string(ingestionJobKey) +
									", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName +
									", directoryPathName: " + directoryPathName + ", exception: " + e.what()
								);
							}
						}

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "getMediaInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", _ingestionJobKey: " +
							to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName +
							", workspace->_directoryName: " + workspace->_directoryName
						);

						{
							string directoryPathName;
							try
							{
								size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
								if (endOfDirectoryIndex != string::npos)
								{
									directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

									SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
									fs::remove_all(directoryPathName);
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeDirectory failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", _ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " +
									stagingChangeFileFormatAssetPathName + ", directoryPathName: " + directoryPathName + ", exception: " + e.what()
								);
							}
						}

						throw e;
					}

					string mmsChangeFileFormatAssetPathName;
					unsigned long mmsPartitionIndexUsed;
					try
					{
						bool deliveryRepositoriesToo = true;

						mmsChangeFileFormatAssetPathName = _mmsStorage->moveAssetInMMSRepository(
							ingestionJobKey, stagingChangeFileFormatAssetPathName, workspace->_directoryName, changeFormatFileName, relativePath,

							&mmsPartitionIndexUsed, // OUT
							// &sourceFileType,

							deliveryRepositoriesToo, workspace->_territories
						);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "_mmsStorage->moveAssetInMMSRepository failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName + ", e.what(): " + e.what()
						);

						{
							string directoryPathName;
							try
							{
								size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
								if (endOfDirectoryIndex != string::npos)
								{
									directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

									SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
									fs::remove_all(directoryPathName);
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeDirectory failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", _ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " +
									stagingChangeFileFormatAssetPathName + ", directoryPathName: " + directoryPathName + ", exception: " + e.what()
								);
							}
						}

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "_mmsStorage->moveAssetInMMSRepository failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) +
							", mmsSourceAssetPathName: " + mmsSourceAssetPathName +
							", stagingChangeFileFormatAssetPathName: " + stagingChangeFileFormatAssetPathName + ", e.what(): " + e.what()
						);

						{
							string directoryPathName;
							try
							{
								size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
								if (endOfDirectoryIndex != string::npos)
								{
									directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

									SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
									fs::remove_all(directoryPathName);
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeDirectory failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", _ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " +
									stagingChangeFileFormatAssetPathName + ", directoryPathName: " + directoryPathName + ", exception: " + e.what()
								);
							}
						}

						throw e;
					}

					// remove staging directory
					{
						string directoryPathName;
						try
						{
							size_t endOfDirectoryIndex = stagingChangeFileFormatAssetPathName.find_last_of("/");
							if (endOfDirectoryIndex != string::npos)
							{
								directoryPathName = stagingChangeFileFormatAssetPathName.substr(0, endOfDirectoryIndex);

								SPDLOG_INFO(string() + "removeDirectory" + ", directoryPathName: " + directoryPathName);
								fs::remove_all(directoryPathName);
							}
						}
						catch (runtime_error &e)
						{
							SPDLOG_ERROR(
								string() + "removeDirectory failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", _ingestionJobKey: " + to_string(ingestionJobKey) + ", stagingChangeFileFormatAssetPathName: " +
								stagingChangeFileFormatAssetPathName + ", directoryPathName: " + directoryPathName + ", exception: " + e.what()
							);
						}
					}

					try
					{
						int64_t physicalItemRetentionInMinutes = -1;
						{
							string field = "physicalItemRetention";
							if (JSONUtils::isMetadataPresent(parametersRoot, field))
							{
								string retention = JSONUtils::asString(parametersRoot, field, "1d");
								physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
							}
						}

						unsigned long long mmsAssetSizeInBytes;
						{
							mmsAssetSizeInBytes = fs::file_size(mmsChangeFileFormatAssetPathName);
						}

						bool externalReadOnlyStorage = false;
						string externalDeliveryTechnology;
						string externalDeliveryURL;
						int64_t liveRecordingIngestionJobKey = -1;
						int64_t changeFormatPhysicalPathKey = _mmsEngineDBFacade->saveVariantContentMetadata(
							workspace->_workspaceKey, ingestionJobKey, liveRecordingIngestionJobKey, mediaItemKey, externalReadOnlyStorage,
							externalDeliveryTechnology, externalDeliveryURL, changeFormatFileName, relativePath, mmsPartitionIndexUsed,
							mmsAssetSizeInBytes,
							-1, // encodingProfileKey,
							physicalItemRetentionInMinutes,

							mediaInfoDetails, videoTracks, audioTracks,

							imageWidth, imageHeight, imageFormat, imageQuality
						);

						SPDLOG_INFO(
							string() + "Saved the Encoded content" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", _ingestionJobKey: " + to_string(ingestionJobKey) +
							", changeFormatPhysicalPathKey: " + to_string(changeFormatPhysicalPathKey)
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() +
							"_mmsEngineDBFacade->saveVariantContentMetadata "
							"failed" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", _ingestionJobKey: " + to_string(ingestionJobKey)
						);

						if (fs::exists(mmsChangeFileFormatAssetPathName))
						{
							SPDLOG_INFO(
								string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
								to_string(ingestionJobKey) + ", mmsChangeFileFormatAssetPathName: " + mmsChangeFileFormatAssetPathName
							);

							fs::remove_all(mmsChangeFileFormatAssetPathName);
						}

						throw e;
					}
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "change file format failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string() + "change file format failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex);
				+", dependencies.size(): " + to_string(dependencies.size());
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "ChangeFileFormat failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			string() + "ChangeFileFormat failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
	}
}
