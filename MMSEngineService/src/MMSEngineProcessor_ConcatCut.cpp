
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "catralibraries/DateTime.h"
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

void MMSEngineProcessor::manageConcatThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "manageConcatThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "manageConcatThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() < 1)
		{
			string errorMessage = string() + "No enough media to be concatenated" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::ContentType concatContentType;
		bool concatContentTypeInitialized = false;
		vector<string> sourcePhysicalPaths;
		string forcedAvgFrameRate;

		// In case the first and the last chunk will have TimeCode, we will
		// report them in the new content
		int64_t utcStartTimeInMilliSecs = -1;
		int64_t utcEndTimeInMilliSecs = -1;
		bool firstMedia = true;
		string lastUserData;

		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			int64_t key;
			MMSEngineDBFacade::ContentType referenceContentType;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			SPDLOG_INFO(
				string() + "manageConcatThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", key: " + to_string(key)
			);

			int64_t sourceMediaItemKey;
			int64_t sourcePhysicalPathKey;
			string sourcePhysicalPath;
			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;
				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(sourcePhysicalPathKey, sourcePhysicalPath, ignore, ignore, ignore, ignore, ignore) =
					physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName;

				sourceMediaItemKey = key;
			}
			else
			{
				tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(sourcePhysicalPath, ignore, ignore, ignore, ignore, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;

				sourcePhysicalPathKey = key;

				bool warningIfMissing = false;
				tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, sourcePhysicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

				tie(sourceMediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
			}

			sourcePhysicalPaths.push_back(sourcePhysicalPath);

			bool warningIfMissing = false;
			tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemKeyDetails =
				_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
					workspace->_workspaceKey, sourcePhysicalPathKey, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);

			MMSEngineDBFacade::ContentType contentType;
			{
				int64_t localMediaItemKey;
				string localTitle;
				string ingestionDate;
				int64_t localIngestionJobKey;
				tie(localMediaItemKey, contentType, localTitle, lastUserData, ingestionDate, localIngestionJobKey, ignore, ignore, ignore) =
					mediaItemKeyDetails;
			}

			if (firstMedia)
			{
				firstMedia = false;

				if (lastUserData != "")
				{
					// try to retrieve time codes
					json sourceUserDataRoot = JSONUtils::toJson(lastUserData);

					string field = "mmsData";
					if (JSONUtils::isMetadataPresent(sourceUserDataRoot, field))
					{
						json sourceMmsDataRoot = sourceUserDataRoot[field];

						string utcStartTimeInMilliSecsField = "utcStartTimeInMilliSecs";
						// string utcChunkStartTimeField = "utcChunkStartTime";
						if (JSONUtils::isMetadataPresent(sourceMmsDataRoot, utcStartTimeInMilliSecsField))
							utcStartTimeInMilliSecs = JSONUtils::asInt64(sourceMmsDataRoot, utcStartTimeInMilliSecsField, 0);
						/*
						else if (JSONUtils::isMetadataPresent(sourceMmsDataRoot,
						utcChunkStartTimeField))
						{
							utcStartTimeInMilliSecs =
						JSONUtils::asInt64(sourceMmsDataRoot,
						utcChunkStartTimeField, 0); utcStartTimeInMilliSecs *=
						1000;
						}
						*/
					}
				}
			}

			if (!concatContentTypeInitialized)
			{
				concatContentType = contentType;
				if (concatContentType != MMSEngineDBFacade::ContentType::Video && concatContentType != MMSEngineDBFacade::ContentType::Audio)
				{
					string errorMessage = string() +
										  "It is not possible to concatenate a media that is not "
										  "video or audio" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", concatContentType: " + MMSEngineDBFacade::toString(concatContentType);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				if (concatContentType != contentType)
				{
					string errorMessage =
						string() + "Not all the References have the same ContentType" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", contentType: " + MMSEngineDBFacade::toString(contentType) +
						", concatContentType: " + MMSEngineDBFacade::toString(concatContentType);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			// to manage a ffmpeg bug generating a corrupted/wrong avgFrameRate,
			// we will force the concat file to have the same avgFrameRate of
			// the source media
			if (concatContentType == MMSEngineDBFacade::ContentType::Video && forcedAvgFrameRate == "")
			{
				/*
				tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
				videoDetails =
				_mmsEngineDBFacade->getVideoDetails(sourceMediaItemKey,
				sourcePhysicalPathKey);

				tie(ignore, ignore, ignore,
					ignore, ignore, ignore, forcedAvgFrameRate,
					ignore, ignore, ignore, ignore, ignore)
					= videoDetails;
				*/
				vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
				vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

				_mmsEngineDBFacade->getVideoDetails(
					sourceMediaItemKey, sourcePhysicalPathKey,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true, videoTracks, audioTracks
				);
				if (videoTracks.size() == 0)
				{
					string errorMessage = string() + "No video track are present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey);

					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack = videoTracks[0];

				tie(ignore, ignore, ignore, ignore, ignore, forcedAvgFrameRate, ignore, ignore, ignore) = videoTrack;
			}
		}

		SPDLOG_INFO(
			string() + "manageConcatThread, retrying time code" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", utcStartTimeInMilliSecs: " + to_string(utcStartTimeInMilliSecs) +
			", lastUserData: " + lastUserData
		);

		if (utcStartTimeInMilliSecs != -1)
		{
			if (lastUserData != "")
			{
				// try to retrieve time codes
				json sourceUserDataRoot = JSONUtils::toJson(lastUserData);

				string field = "mmsData";
				if (JSONUtils::isMetadataPresent(sourceUserDataRoot, field))
				{
					json sourceMmsDataRoot = sourceUserDataRoot[field];

					string utcEndTimeInMilliSecsField = "utcEndTimeInMilliSecs";
					// string utcChunkEndTimeField = "utcChunkEndTime";
					if (JSONUtils::isMetadataPresent(sourceMmsDataRoot, utcEndTimeInMilliSecsField))
						utcEndTimeInMilliSecs = JSONUtils::asInt64(sourceMmsDataRoot, utcEndTimeInMilliSecsField, 0);
					/*
					else if (JSONUtils::isMetadataPresent(sourceMmsDataRoot,
					utcChunkEndTimeField))
					{
						utcEndTimeInMilliSecs =
					JSONUtils::asInt64(sourceMmsDataRoot, utcChunkEndTimeField,
					0); utcEndTimeInMilliSecs *= 1000;
					}
					*/
				}
			}

			// utcStartTimeInMilliSecs and utcEndTimeInMilliSecs will be set in
			// parametersRoot
			if (utcStartTimeInMilliSecs != -1 && utcEndTimeInMilliSecs != -1)
			{
				json destUserDataRoot;

				/*
				{
					string json = JSONUtils::toString(parametersRoot);

					SPDLOG_INFO(string() + "manageConcatThread"
						+ ", _processorIdentifier: " +
				to_string(_processorIdentifier)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", parametersRoot: " + json
					);
				}
				*/

				string field = "userData";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
					destUserDataRoot = parametersRoot[field];

				json destMmsDataRoot;

				field = "mmsData";
				if (JSONUtils::isMetadataPresent(destUserDataRoot, field))
					destMmsDataRoot = destUserDataRoot[field];

				field = "utcStartTimeInMilliSecs";
				if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
					destMmsDataRoot.erase(field);
				destMmsDataRoot[field] = utcStartTimeInMilliSecs;

				field = "utcEndTimeInMilliSecs";
				if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
					destMmsDataRoot.erase(field);
				destMmsDataRoot[field] = utcEndTimeInMilliSecs;

				// next statements will provoke an std::exception in case
				// parametersRoot -> UserData is a string (i.e.: "userData" :
				// "{\"matchId\": 363615, \"groupName\": \"CI\",
				//		\"homeTeamName\": \"Pescara Calcio\", \"awayTeamName\":
				//\"Olbia Calcio 1905\",
				//		\"start\": 1629398700000 }")
				//	and NOT a json

				field = "mmsData";
				destUserDataRoot[field] = destMmsDataRoot;

				field = "userData";
				parametersRoot[field] = destUserDataRoot;
			}
		}

		// this is a concat, so destination file name shall have the same
		// extension as the source file name
		string fileFormat;
		size_t extensionIndex = sourcePhysicalPaths.front().find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = string() +
								  "No fileFormat (extension of the file) found in "
								  "sourcePhysicalPath" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", sourcePhysicalPaths.front(): " + sourcePhysicalPaths.front();
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		fileFormat = sourcePhysicalPaths.front().substr(extensionIndex + 1);

		string localSourceFileName = to_string(ingestionJobKey) + "_concat" + "." + fileFormat // + "_source"
			;

		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
		string concatenatedMediaPathName = workspaceIngestionRepository + "/" + localSourceFileName;

		if (sourcePhysicalPaths.size() == 1)
		{
			string sourcePhysicalPath = sourcePhysicalPaths.at(0);
			SPDLOG_INFO(
				string() + "Coping" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourcePhysicalPath: " + sourcePhysicalPath +
				", concatenatedMediaPathName: " + concatenatedMediaPathName
			);

			fs::copy(sourcePhysicalPath, concatenatedMediaPathName, fs::copy_options::recursive);
		}
		else
		{
			FFMpeg ffmpeg(_configurationRoot, _logger);
			ffmpeg.concat(
				ingestionJobKey, concatContentType == MMSEngineDBFacade::ContentType::Video ? true : false, sourcePhysicalPaths,
				concatenatedMediaPathName
			);
		}

		SPDLOG_INFO(
			string() + "generateConcatMediaToIngest done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", concatenatedMediaPathName: " + concatenatedMediaPathName
		);

		double maxDurationInSeconds = 0.0;
		double extraSecondsToCutWhenMaxDurationIsReached = 0.0;
		string field = "MaxDurationInSeconds";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			maxDurationInSeconds = JSONUtils::asDouble(parametersRoot, field, 0.0);

			field = "ExtraSecondsToCutWhenMaxDurationIsReached";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				extraSecondsToCutWhenMaxDurationIsReached = JSONUtils::asDouble(parametersRoot, field, 0.0);

				if (extraSecondsToCutWhenMaxDurationIsReached >= abs(maxDurationInSeconds))
					extraSecondsToCutWhenMaxDurationIsReached = 0.0;
			}
		}
		SPDLOG_INFO(
			string() + "duration check" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", _ingestionJobKey: " + to_string(ingestionJobKey) + ", maxDurationInSeconds: " + to_string(maxDurationInSeconds) +
			", extraSecondsToCutWhenMaxDurationIsReached: " + to_string(extraSecondsToCutWhenMaxDurationIsReached)
		);
		if (maxDurationInSeconds != 0.0)
		{
			tuple<int64_t, long, json> mediaInfoDetails;
			vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
			vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
			int64_t durationInMilliSeconds;

			SPDLOG_INFO(
				string() + "Calling ffmpeg.getMediaInfo" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", _ingestionJobKey: " + to_string(ingestionJobKey) + ", concatenatedMediaPathName: " + concatenatedMediaPathName
			);
			int timeoutInSeconds = 20;
			bool isMMSAssetPathName = true;
			FFMpeg ffmpeg(_configurationRoot, _logger);
			mediaInfoDetails =
				ffmpeg.getMediaInfo(ingestionJobKey, isMMSAssetPathName, timeoutInSeconds, concatenatedMediaPathName, videoTracks, audioTracks);

			// tie(durationInMilliSeconds, ignore,
			//	ignore, ignore, ignore, ignore, ignore, ignore,
			//	ignore, ignore, ignore, ignore) = mediaInfo;
			tie(durationInMilliSeconds, ignore, ignore) = mediaInfoDetails;

			SPDLOG_INFO(
				string() + "duration check" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", _ingestionJobKey: " + to_string(ingestionJobKey) + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds) +
				", maxDurationInSeconds: " + to_string(maxDurationInSeconds) +
				", extraSecondsToCutWhenMaxDurationIsReached: " + to_string(extraSecondsToCutWhenMaxDurationIsReached)
			);
			if (durationInMilliSeconds > abs(maxDurationInSeconds) * 1000)
			{
				string localCutSourceFileName = to_string(ingestionJobKey) + "_concat_cut" + "." + fileFormat // + "_source"
					;

				string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
				string cutMediaPathName = workspaceIngestionRepository + "/" + localCutSourceFileName;

				string cutType = "KeyFrameSeeking";
				double startTimeInSeconds;
				double endTimeInSeconds;
				if (maxDurationInSeconds < 0.0)
				{
					startTimeInSeconds = ((durationInMilliSeconds / 1000) - (abs(maxDurationInSeconds) - extraSecondsToCutWhenMaxDurationIsReached));
					endTimeInSeconds = durationInMilliSeconds / 1000;
				}
				else
				{
					startTimeInSeconds = 0.0;
					endTimeInSeconds = maxDurationInSeconds - extraSecondsToCutWhenMaxDurationIsReached;
				}
				int framesNumber = -1;

				SPDLOG_INFO(
					string() + "Calling ffmpeg.cut" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", _ingestionJobKey: " + to_string(ingestionJobKey) + ", concatenatedMediaPathName: " + concatenatedMediaPathName +
					", cutType: " + cutType + ", startTimeInSeconds: " + to_string(startTimeInSeconds) +
					", endTimeInSeconds: " + to_string(endTimeInSeconds) + ", framesNumber: " + to_string(framesNumber)
				);

				ffmpeg.cutWithoutEncoding(
					ingestionJobKey, concatenatedMediaPathName, concatContentType == MMSEngineDBFacade::ContentType::Video ? true : false, cutType,
					"", // startKeyFramesSeekingInterval,
					"", // endKeyFramesSeekingInterval,
					to_string(startTimeInSeconds), to_string(endTimeInSeconds), framesNumber, cutMediaPathName
				);

				SPDLOG_INFO(
					string() + "cut done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", cutMediaPathName: " + cutMediaPathName
				);

				localSourceFileName = localCutSourceFileName;

				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", concatenatedMediaPathName: " + concatenatedMediaPathName
				);

				fs::remove_all(concatenatedMediaPathName);
			}
		}

		{
			string title;
			int64_t imageOfVideoMediaItemKey = -1;
			int64_t cutOfVideoMediaItemKey = -1;
			int64_t cutOfAudioMediaItemKey = -1;
			double startTimeInSeconds = 0.0;
			double endTimeInSeconds = 0.0;
			string mediaMetaDataContent = generateMediaMetadataToIngest(
				ingestionJobKey,
				// concatContentType == MMSEngineDBFacade::ContentType::Video ?
				// true : false,
				fileFormat, title, imageOfVideoMediaItemKey, cutOfVideoMediaItemKey, cutOfAudioMediaItemKey, startTimeInSeconds, endTimeInSeconds,
				parametersRoot
			);

			{
				shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
					_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
					);

				localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
				localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
				localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

				localAssetIngestionEvent->setExternalReadOnlyStorage(false);
				localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
				localAssetIngestionEvent->setIngestionSourceFileName(localSourceFileName);
				localAssetIngestionEvent->setMMSSourceFileName(localSourceFileName);
				localAssetIngestionEvent->setWorkspace(workspace);
				localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
				localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

				// to manage a ffmpeg bug generating a corrupted/wrong
				// avgFrameRate, we will force the concat file to have the same
				// avgFrameRate of the source media
				if (forcedAvgFrameRate != "" && concatContentType == MMSEngineDBFacade::ContentType::Video)
					localAssetIngestionEvent->setForcedAvgFrameRate(forcedAvgFrameRate);

				localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

				shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
				_multiEventsSet->addEvent(event);

				SPDLOG_INFO(
					string() + "addEvent: EVENT_TYPE (INGESTASSETEVENT)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", getEventKey().first: " + to_string(event->getEventKey().first) +
					", getEventKey().second: " + to_string(event->getEventKey().second)
				);
			}
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageConcatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
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

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageConcatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
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

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::manageCutMediaThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "manageCutMediaThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "manageCutMediaThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong number of media to be cut" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		int64_t sourceMediaItemKey;
		int64_t sourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType referenceContentType;
		string sourceAssetPathName;
		string sourceRelativePath;
		string sourceFileName;
		string sourceFileExtension;
		int64_t sourceDurationInMilliSecs;
		string sourcePhysicalDeliveryURL;
		string sourceTranscoderStagingAssetPathName;
		bool stopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
			sourceFileExtension, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName,
			stopIfReferenceProcessingError) = dependencyInfo;

		string userData;
		{
			bool warningIfMissing = false;

			tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> mediaItemDetails =
				_mmsEngineDBFacade->getMediaItemKeyDetails(
					workspace->_workspaceKey, sourceMediaItemKey, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);

			tie(ignore, ignore, userData, ignore, ignore, ignore) = mediaItemDetails;
		}

		if (referenceContentType != MMSEngineDBFacade::ContentType::Video && referenceContentType != MMSEngineDBFacade::ContentType::Audio)
		{
			string errorMessage = string() + "It is not possible to cut a media that is not video or audio" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", contentType: " + MMSEngineDBFacade::toString(referenceContentType);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// abbiamo bisogno del source frame rate in un paio di casi sotto
		string forcedAvgFrameRate;
		int framesPerSecond = -1;
		{
			try
			{
				if (referenceContentType == MMSEngineDBFacade::ContentType::Video)
				{
					vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
					vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

					_mmsEngineDBFacade->getVideoDetails(
						sourceMediaItemKey, sourcePhysicalPathKey,
						// 2022-12-18: MIK potrebbe essere stato appena aggiunto
						true, videoTracks, audioTracks
					);
					if (videoTracks.size() == 0)
					{
						string errorMessage = string() + "No video track are present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey);

						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack = videoTracks[0];

					tie(ignore, ignore, ignore, ignore, ignore, forcedAvgFrameRate, ignore, ignore, ignore) = videoTrack;

					if (forcedAvgFrameRate != "")
					{
						// es: 25/1
						size_t index = forcedAvgFrameRate.find("/");
						if (index == string::npos)
							framesPerSecond = stoi(forcedAvgFrameRate);
						else
						{
							int frames = stoi(forcedAvgFrameRate.substr(0, index));
							int seconds = stoi(forcedAvgFrameRate.substr(index + 1));
							if (seconds != 0) // I saw: 0/0
								framesPerSecond = frames / seconds;
							SPDLOG_INFO(
								"forcedAvgFrameRate: {}"
								"frames: {}"
								"seconds: {}"
								"framesPerSecond: {}",
								forcedAvgFrameRate, frames, seconds, framesPerSecond
							);
						}
					}
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "_mmsEngineDBFacade->getVideoDetails failed" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			catch (exception &e)
			{
				string errorMessage = string() + "_mmsEngineDBFacade->getVideoDetails failed" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "manageCutMediaThread frame rate" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", forcedAvgFrameRate" + forcedAvgFrameRate
		);

		// check start time / end time
		int framesNumber = -1;
		string startTime;
		string endTime = "0.0";
		{
			string field = "startTime";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			startTime = JSONUtils::asString(parametersRoot, field, "");

			field = "endTime";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				if (referenceContentType == MMSEngineDBFacade::ContentType::Audio)
				{
					// endTime in case of Audio is mandatory
					// because we cannot use the other option (FramesNumber)

					string errorMessage = string() +
										  "Field is not present or it is null, endTimeInSeconds "
										  "in case of Audio is mandatory because we cannot use "
										  "the other option (FramesNumber)" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			endTime = JSONUtils::asString(parametersRoot, field, "");

			if (referenceContentType == MMSEngineDBFacade::ContentType::Video)
			{
				field = "framesNumber";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
					framesNumber = JSONUtils::asInt(parametersRoot, field, 0);
			}

			// 2021-02-05: default is set to true because often we have the
			// error
			//	endTimeInSeconds is bigger of few milliseconds of the duration
			// of the media 	For this reason this field is set to true by
			// default
			bool fixEndTimeIfOvercomeDuration = true;
			field = "FixEndTimeIfOvercomeDuration";
			fixEndTimeIfOvercomeDuration = JSONUtils::asBool(parametersRoot, field, true);

			// startTime/endTime potrebbero avere anche il formato HH:MM:SS:FF.
			// Questo formato è stato utile per il cut di file mxf ma non è
			// supportato da ffmpeg (il formato supportati da ffmpeg sono quelli
			// gestiti da FFMpeg::timeToSeconds) Per cui qui riconduciamo il
			// formato HH:MM:SS:FF a quello gestito da ffmpeg HH:MM:SS.<decimi
			// di secondo>.
			{
				if (count_if(startTime.begin(), startTime.end(), [](char c) { return c == ':'; }) == 3)
				{
					int framesIndex = startTime.find_last_of(":");
					double frames = stoi(startTime.substr(framesIndex + 1));

					// se ad esempio sono 4 frames su 25 frames al secondo
					//	la parte decimale del secondo richiesta dal formato
					// ffmpeg sarà 16, 	cioè: (4/25)*100

					int decimals = (frames / ((double)framesPerSecond)) * 100;
					string newStartTime = startTime.substr(0, framesIndex) + "." + to_string(decimals);
					SPDLOG_INFO(
						"conversion from HH:MM:SS:FF to ffmeg format"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", startTime: {}"
						", newStartTime: {}",
						_processorIdentifier, ingestionJobKey, startTime, newStartTime
					);
					startTime = newStartTime;
				}
				if (count_if(endTime.begin(), endTime.end(), [](char c) { return c == ':'; }) == 3)
				{
					int framesIndex = endTime.find_last_of(":");
					double frames = stoi(endTime.substr(framesIndex + 1));

					// se ad esempio sono 4 frames su 25 frames al secondo
					//	la parte decimale del secondo richiesta dal formato
					// ffmpeg sarà 16, 	cioè: (4/25)*100

					int decimals = (frames / ((double)framesPerSecond)) * 100;
					string newEndTime = endTime.substr(0, framesIndex) + "." + to_string(decimals);
					SPDLOG_INFO(
						"conversion from HH:MM:SS:FF to ffmeg format"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", endTime: {}"
						", newEndTime: {}",
						_processorIdentifier, ingestionJobKey, endTime, newEndTime
					);
					endTime = newEndTime;
				}
			}
			double startTimeInSeconds = FFMpeg::timeToSeconds(ingestionJobKey, startTime).first;
			double endTimeInSeconds = FFMpeg::timeToSeconds(ingestionJobKey, endTime).first;

			field = "timesRelativeToMetaDataField";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string timesRelativeToMetaDataField = JSONUtils::asString(parametersRoot, field, "");

				string metaData;
				{
					bool warningIfMissing = false;

					metaData = _mmsEngineDBFacade->getPhysicalPathDetails(
						sourcePhysicalPathKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena aggiunto
						true
					);
				}

				if (metaData == "")
				{
					string errorMessage = fmt::format(
						"timesRelativeToMetaDataField cannot be applied "
						"because source media does not have metaData"
						", ingestionJobKey: {}"
						", sourcePhysicalPathKey: {}",
						ingestionJobKey, sourcePhysicalPathKey
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				json metaDataRoot = JSONUtils::toJson(metaData);

				string timeCode = JSONUtils::asString(metaDataRoot, timesRelativeToMetaDataField, "");
				if (timeCode == "")
				{
					string errorMessage = fmt::format(
						"timesRelativeToMetaDataField cannot be applied "
						"because source media has metaData but does not have "
						"the timecode"
						", ingestionJobKey: {}"
						", sourcePhysicalPathKey: {}"
						", metaData: {}"
						", timesRelativeToMetaDataField: {}",
						ingestionJobKey, sourcePhysicalPathKey, metaData, timesRelativeToMetaDataField
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				if (count_if(timeCode.begin(), timeCode.end(), [](char c) { return c == ':'; }) == 3)
				{
					int framesIndex = timeCode.find_last_of(":");
					double frames = stoi(timeCode.substr(framesIndex + 1));

					// se ad esempio sono 4 frames su 25 frames al secondo
					//	la parte decimale del secondo richiesta dal formato
					// ffmpeg sarà 16, 	cioè: (4/25)*100

					int decimals = (frames / ((double)framesPerSecond)) * 100;
					string newTimeCode = timeCode.substr(0, framesIndex) + "." + to_string(decimals);
					SPDLOG_INFO(
						"conversion from HH:MM:SS:FF to ffmeg format"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", timeCode: {}"
						", newTimeCode: {}",
						_processorIdentifier, ingestionJobKey, timeCode, newTimeCode
					);
					timeCode = newTimeCode;
				}

				long startTimeInCentsOfSeconds = FFMpeg::timeToSeconds(ingestionJobKey, startTime).second;
				long endTimeInCentsOfSeconds = FFMpeg::timeToSeconds(ingestionJobKey, endTime).second;
				long relativeTimeInCentsOfSeconds = FFMpeg::timeToSeconds(ingestionJobKey, timeCode).second;

				long newStartTimeInCentsOfSeconds = startTimeInCentsOfSeconds - relativeTimeInCentsOfSeconds;
				string newStartTime = FFMpeg::centsOfSecondsToTime(ingestionJobKey, newStartTimeInCentsOfSeconds);

				long newEndTimeInCentsOfSeconds = endTimeInCentsOfSeconds - relativeTimeInCentsOfSeconds;
				string newEndTime = FFMpeg::centsOfSecondsToTime(ingestionJobKey, newEndTimeInCentsOfSeconds);

				SPDLOG_INFO(
					"correction because of timesRelativeToMetaDataField"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", timeCode: {}"
					", relativeTimeInCentsOfSeconds: {}"
					", startTimeInCentsOfSeconds: {}"
					", startTime: {}"
					", newStartTime: {}"
					", endTimeInCentsOfSeconds: {}"
					", endTime: {}"
					", newEndTime: {}",
					_processorIdentifier, ingestionJobKey, timeCode, relativeTimeInCentsOfSeconds, startTimeInCentsOfSeconds, startTime, newStartTime,
					endTimeInCentsOfSeconds, endTime, newEndTime
				);

				startTimeInSeconds = newStartTimeInCentsOfSeconds / 100;
				startTime = newStartTime;

				endTimeInSeconds = newEndTimeInCentsOfSeconds / 100;
				endTime = newEndTime;
			}

			if (framesNumber == -1)
			{
				// il prossimo controllo possiamo farlo solo nel caso la stringa
				// non sia nel formato HH:MM:SS
				if (endTimeInSeconds < 0)
				{
					// if negative, it has to be subtract by the
					// durationInMilliSeconds
					double newEndTimeInSeconds = (sourceDurationInMilliSecs - (endTimeInSeconds * -1000)) / 1000;

					endTime = to_string(newEndTimeInSeconds);

					SPDLOG_ERROR(
						string() + "endTime was changed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
						", startTime: " + startTime + ", endTime: " + endTime + ", newEndTimeInSeconds: " + to_string(newEndTimeInSeconds) +
						", sourceDurationInMilliSecs: " + to_string(sourceDurationInMilliSecs)
					);
				}
			}

			if (startTimeInSeconds < 0.0)
			{
				startTime = "0.0";

				SPDLOG_INFO(
					string() + "startTime was changed to 0.0 because it is negative" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", fixEndTimeIfOvercomeDuration: " +
					to_string(fixEndTimeIfOvercomeDuration) + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
					", previousStartTimeInSeconds: " + to_string(startTimeInSeconds) + ", new startTime: " + startTime +
					", sourceDurationInMilliSecs (input media): " + to_string(sourceDurationInMilliSecs)
				);

				startTimeInSeconds = 0.0;
			}

			if (startTimeInSeconds > endTimeInSeconds)
			{
				string errorMessage =
					string() +
					"Cut was not done because startTimeInSeconds is bigger "
					"than endTimeInSeconds" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) + ", startTimeInSeconds: " + to_string(startTimeInSeconds) +
					", endTimeInSeconds: " + to_string(endTimeInSeconds) + ", sourceDurationInMilliSecs: " + to_string(sourceDurationInMilliSecs);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (framesNumber == -1)
			{
				if (sourceDurationInMilliSecs < endTimeInSeconds * 1000)
				{
					if (fixEndTimeIfOvercomeDuration)
					{
						endTime = to_string(sourceDurationInMilliSecs / 1000);

						SPDLOG_INFO(
							string() +
							"endTimeInSeconds was changed to "
							"durationInMilliSeconds" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", fixEndTimeIfOvercomeDuration: " + to_string(fixEndTimeIfOvercomeDuration) +
							", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
							", previousEndTimeInSeconds: " + to_string(endTimeInSeconds) + ", new endTimeInSeconds: " + endTime +
							", sourceDurationInMilliSecs (input media): " + to_string(sourceDurationInMilliSecs)
						);
					}
					else
					{
						string errorMessage =
							string() +
							"Cut was not done because endTimeInSeconds is "
							"bigger than durationInMilliSeconds (input media)" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", video sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
							", startTimeInSeconds: " + to_string(startTimeInSeconds) + ", endTimeInSeconds: " + to_string(endTimeInSeconds) +
							", sourceDurationInMilliSecs (input media): " + to_string(sourceDurationInMilliSecs);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}
		}

		int64_t newUtcStartTimeInMilliSecs = -1;
		int64_t newUtcEndTimeInMilliSecs = -1;
		{
			// In case the media has TimeCode, we will report them in the new
			// content
			if (framesNumber == -1 && userData != "")
			{
				// try to retrieve time codes
				json sourceUserDataRoot = JSONUtils::toJson(userData);

				int64_t utcStartTimeInMilliSecs = -1;
				int64_t utcEndTimeInMilliSecs = -1;

				string field = "mmsData";
				if (JSONUtils::isMetadataPresent(sourceUserDataRoot, field))
				{
					json sourceMmsDataRoot = sourceUserDataRoot[field];

					string utcStartTimeInMilliSecsField = "utcStartTimeInMilliSecs";
					// string utcChunkStartTimeField = "utcChunkStartTime";
					if (JSONUtils::isMetadataPresent(sourceMmsDataRoot, utcStartTimeInMilliSecsField))
						utcStartTimeInMilliSecs = JSONUtils::asInt64(sourceMmsDataRoot, utcStartTimeInMilliSecsField, 0);
					/*
					else if (JSONUtils::isMetadataPresent(sourceMmsDataRoot,
					utcChunkStartTimeField))
					{
						utcStartTimeInMilliSecs =
					JSONUtils::asInt64(sourceMmsDataRoot,
					utcChunkStartTimeField, 0); utcStartTimeInMilliSecs *= 1000;
					}
					*/

					if (utcStartTimeInMilliSecs != -1)
					{
						string utcEndTimeInMilliSecsField = "utcEndTimeInMilliSecs";
						// string utcChunkEndTimeField = "utcChunkEndTime";
						if (JSONUtils::isMetadataPresent(sourceMmsDataRoot, utcEndTimeInMilliSecsField))
							utcEndTimeInMilliSecs = JSONUtils::asInt64(sourceMmsDataRoot, utcEndTimeInMilliSecsField, 0);
						/*
						else if (JSONUtils::isMetadataPresent(sourceMmsDataRoot,
						utcChunkEndTimeField))
						{
							utcEndTimeInMilliSecs =
						JSONUtils::asInt64(sourceMmsDataRoot,
						utcChunkEndTimeField, 0); utcEndTimeInMilliSecs *= 1000;
						}
						*/

						// utcStartTimeInMilliSecs and utcEndTimeInMilliSecs
						// will be set in parametersRoot
						if (utcStartTimeInMilliSecs != -1 && utcEndTimeInMilliSecs != -1)
						{
							newUtcStartTimeInMilliSecs = utcStartTimeInMilliSecs;
							newUtcStartTimeInMilliSecs += ((int64_t)(FFMpeg::timeToSeconds(ingestionJobKey, startTime).second * 10));
							newUtcEndTimeInMilliSecs =
								utcStartTimeInMilliSecs + ((int64_t)(FFMpeg::timeToSeconds(ingestionJobKey, endTime).second * 10));
						}
					}
				}
			}
		}

		string field = "cutType";
		string cutType = JSONUtils::asString(parametersRoot, field, "KeyFrameSeeking");

		SPDLOG_INFO(
			string() + "manageCutMediaThread new start/end" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", cutType: " + cutType + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey) +
			", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey) + ", sourceAssetPathName: " + sourceAssetPathName +
			", sourceDurationInMilliSecs: " + to_string(sourceDurationInMilliSecs) + ", framesNumber: " + to_string(framesNumber) +
			", startTime: " + startTime + ", endTime: " + endTime + ", newUtcStartTimeInMilliSecs: " + to_string(newUtcStartTimeInMilliSecs) +
			", newUtcEndTimeInMilliSecs: " + to_string(newUtcEndTimeInMilliSecs)
		);
		if (cutType == "KeyFrameSeeking" || cutType == "FrameAccurateWithoutEncoding" || cutType == "KeyFrameSeekingInterval")
		{
			string outputFileFormat;
			field = "outputFileFormat";
			outputFileFormat = JSONUtils::asString(parametersRoot, field, "");

			SPDLOG_INFO(
				string() + "1 manageCutMediaThread new start/end" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey)
			);

			// this is a cut so destination file name shall have the same
			// extension as the source file name
			string fileFormat;
			if (outputFileFormat == "")
			{
				string sourceFileExtensionWithoutDot = sourceFileExtension.size() > 0 ? sourceFileExtension.substr(1) : sourceFileExtension;

				if (sourceFileExtensionWithoutDot == "m3u8")
					fileFormat = "ts";
				else
					fileFormat = sourceFileExtensionWithoutDot;
			}
			else
			{
				fileFormat = outputFileFormat;
			}

			SPDLOG_INFO(
				string() + "manageCutMediaThread file format" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", fileFormat: " + fileFormat
			);

			if (newUtcStartTimeInMilliSecs != -1 && newUtcEndTimeInMilliSecs != -1)
			{
				json destUserDataRoot;

				field = "userData";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
					destUserDataRoot = parametersRoot[field];

				json destMmsDataRoot;

				field = "mmsData";
				if (JSONUtils::isMetadataPresent(destUserDataRoot, field))
					destMmsDataRoot = destUserDataRoot[field];

				field = "utcStartTimeInMilliSecs";
				if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
					destMmsDataRoot.erase(field);
				destMmsDataRoot[field] = newUtcStartTimeInMilliSecs;

				field = "utcStartTimeInMilliSecs_str";
				{
					time_t newUtcStartTimeInSecs = newUtcStartTimeInMilliSecs / 1000;
					// i.e.: 2021-02-26T15:41:15Z
					string utcToUtcString = DateTime::utcToUtcString(newUtcStartTimeInSecs);
					utcToUtcString.insert(utcToUtcString.size() - 1, "." + to_string(newUtcStartTimeInMilliSecs % 1000));
					destMmsDataRoot[field] = utcToUtcString;
				}

				field = "utcEndTimeInMilliSecs";
				if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
					destMmsDataRoot.erase(field);
				destMmsDataRoot[field] = newUtcEndTimeInMilliSecs;

				field = "utcEndTimeInMilliSecs_str";
				{
					time_t newUtcEndTimeInSecs = newUtcEndTimeInMilliSecs / 1000;
					// i.e.: 2021-02-26T15:41:15Z
					string utcToUtcString = DateTime::utcToUtcString(newUtcEndTimeInSecs);
					utcToUtcString.insert(utcToUtcString.size() - 1, "." + to_string(newUtcEndTimeInMilliSecs % 1000));
					destMmsDataRoot[field] = utcToUtcString;
				}

				field = "mmsData";
				destUserDataRoot[field] = destMmsDataRoot;

				field = "userData";
				parametersRoot[field] = destUserDataRoot;
			}

			SPDLOG_INFO(
				string() + "manageCutMediaThread user data management" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey)
			);

			string localSourceFileName = to_string(ingestionJobKey) + "_cut" + "." + fileFormat // + "_source"
				;

			string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
			string cutMediaPathName = workspaceIngestionRepository + "/" + localSourceFileName;

			FFMpeg ffmpeg(_configurationRoot, _logger);
			ffmpeg.cutWithoutEncoding(
				ingestionJobKey, sourceAssetPathName, referenceContentType == MMSEngineDBFacade::ContentType::Video ? true : false, cutType,
				"", // startKeyFramesSeekingInterval,
				"", // endKeyFramesSeekingInterval,
				startTime, endTime, framesNumber, cutMediaPathName
			);

			SPDLOG_INFO(
				string() + "cut done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", cutMediaPathName: " + cutMediaPathName
			);

			string title;
			int64_t imageOfVideoMediaItemKey = -1;
			int64_t cutOfVideoMediaItemKey = -1;
			int64_t cutOfAudioMediaItemKey = -1;
			if (referenceContentType == MMSEngineDBFacade::ContentType::Video)
				cutOfVideoMediaItemKey = sourceMediaItemKey;
			else if (referenceContentType == MMSEngineDBFacade::ContentType::Audio)
				cutOfAudioMediaItemKey = sourceMediaItemKey;
			string mediaMetaDataContent = generateMediaMetadataToIngest(
				ingestionJobKey, fileFormat, title, imageOfVideoMediaItemKey, cutOfVideoMediaItemKey, cutOfAudioMediaItemKey,
				FFMpeg::timeToSeconds(ingestionJobKey, startTime).first, FFMpeg::timeToSeconds(ingestionJobKey, endTime).first, parametersRoot
			);

			{
				shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
					_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
					);

				localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
				localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
				localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

				localAssetIngestionEvent->setExternalReadOnlyStorage(false);
				localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
				localAssetIngestionEvent->setIngestionSourceFileName(localSourceFileName);
				localAssetIngestionEvent->setMMSSourceFileName(localSourceFileName);
				localAssetIngestionEvent->setWorkspace(workspace);
				localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
				localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
				// to manage a ffmpeg bug generating a corrupted/wrong
				// avgFrameRate, we will force the concat file to have the same
				// avgFrameRate of the source media
				if (forcedAvgFrameRate != "" && referenceContentType == MMSEngineDBFacade::ContentType::Video)
					localAssetIngestionEvent->setForcedAvgFrameRate(forcedAvgFrameRate);

				localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

				shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
				_multiEventsSet->addEvent(event);

				SPDLOG_INFO(
					string() + "addEvent: EVENT_TYPE (LOCALASSETINGESTIONEVENT)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", getEventKey().first: " + to_string(event->getEventKey().first) +
					", getEventKey().second: " + to_string(event->getEventKey().second)
				);
			}
		}
		else
		{
			// FrameAccurateWithEncoding

			MMSEngineDBFacade::EncodingPriority encodingPriority;
			string field = "encodingPriority";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
			else
				encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));

			int64_t encodingProfileKey;
			json encodingProfileDetailsRoot;
			{
				string keyField = "encodingProfileKey";
				string labelField = "encodingProfileLabel";
				if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
				{
					encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
				}
				else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
				{
					string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

					MMSEngineDBFacade::ContentType videoContentType = MMSEngineDBFacade::ContentType::Video;
					encodingProfileKey =
						_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, videoContentType, encodingProfileLabel);
				}
				else
				{
					string errorMessage = string() + "Both fields are not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + keyField +
										  ", Field: " + labelField;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				{
					string jsonEncodingProfile;

					tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
						_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
					tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

					encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
				}
			}

			string encodedFileName;
			{
				/*
				string fileFormat =
				JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat",
				""); string fileFormatLowerCase;
				fileFormatLowerCase.resize(fileFormat.size());
				transform(fileFormat.begin(), fileFormat.end(),
				fileFormatLowerCase.begin(),
					[](unsigned char c){return tolower(c); } );
				*/

				encodedFileName = to_string(ingestionJobKey) + "_" + to_string(encodingProfileKey) +
								  getEncodedFileExtensionByEncodingProfile(encodingProfileDetailsRoot); // "." + fileFormatLowerCase
				;
			}

			string encodedTranscoderStagingAssetPathName; // used in case of
														  // external encoder
			fs::path encodedNFSStagingAssetPathName;
			{
				bool removeLinuxPathIfExist = false;
				bool neededForTranscoder = true;

				encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
					neededForTranscoder,
					workspace->_directoryName,	// workspaceDirectoryName
					to_string(ingestionJobKey), // directoryNamePrefix
					"/",						// relativePath,
					// as specified by doc
					// (TASK_01_Add_Content_JSON_Format.txt), in case of hls
					// and external encoder (binary is ingested through
					// PUSH), the directory inside the tar.gz has to be
					// 'content'
					encodedFileName, // content
					-1,				 // _encodingItem->_mediaItemKey, not used because
									 // encodedFileName is not ""
					-1,				 // _encodingItem->_physicalPathKey, not used because
									 // encodedFileName is not ""
					removeLinuxPathIfExist
				);

				encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
			}

			_mmsEngineDBFacade->addEncoding_CutFrameAccurate(
				workspace, ingestionJobKey, sourceMediaItemKey, sourcePhysicalPathKey, sourceAssetPathName, sourceDurationInMilliSecs,
				sourceFileExtension, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName, endTime, encodingProfileKey,
				encodingProfileDetailsRoot, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL,
				_mmsBinaryIngestionURL, _mmsIngestionURL, encodingPriority, newUtcStartTimeInMilliSecs, newUtcEndTimeInMilliSecs
			);
		}
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			string() + "manageCutMediaThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
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

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageCutMediaThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
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

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageCutMediaThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
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

		// it's a thread, no throw
		// throw e;
		return;
	}
}
