
#include "MMSEngineProcessor.h"
#include "JSONUtils.h"
#include "FFMpeg.h"
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
#include "catralibraries/StringUtils.h"
#include "catralibraries/ProcessUtility.h"
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

void MMSEngineProcessor::extractTracksContentThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "extractTracksContentThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "extractTracksContentThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured media to be used to extract a track" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		vector<pair<string, int>> tracksToBeExtracted;
		string outputFileFormat;
		{
			{
				string field = "Tracks";
				json tracksToot = parametersRoot[field];
				if (tracksToot.size() == 0)
				{
					string errorMessage = string() + "No correct number of Tracks" + ", tracksToot.size: " + to_string(tracksToot.size());
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				for (int trackIndex = 0; trackIndex < tracksToot.size(); trackIndex++)
				{
					json trackRoot = tracksToot[trackIndex];

					field = "TrackType";
					if (!JSONUtils::isMetadataPresent(trackRoot, field))
					{
						string sTrackRoot = JSONUtils::toString(trackRoot);

						string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field + ", sTrackRoot: " + sTrackRoot;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					string trackType = JSONUtils::asString(trackRoot, field, "");

					int trackNumber = 0;
					field = "TrackNumber";
					trackNumber = JSONUtils::asInt(trackRoot, field, 0);

					tracksToBeExtracted.push_back(make_pair(trackType, trackNumber));
				}
			}

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

				string mmsAssetPathName;

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(ignore, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
				}
				else
				{
					tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
				}

				{
					string localSourceFileName;
					string extractTrackMediaPathName;
					{
						localSourceFileName = to_string(ingestionJobKey) + "_" + to_string(key) + "_extractTrack" + "." + outputFileFormat;

						string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
						extractTrackMediaPathName = workspaceIngestionRepository + "/" + localSourceFileName;
					}

					FFMpeg ffmpeg(_configurationRoot, _logger);

					ffmpeg.extractTrackMediaToIngest(ingestionJobKey, mmsAssetPathName, tracksToBeExtracted, extractTrackMediaPathName);

					SPDLOG_INFO(
						string() + "extractTrackMediaToIngest done" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", extractTrackMediaPathName: " + extractTrackMediaPathName
					);

					string title;
					int64_t imageOfVideoMediaItemKey = -1;
					int64_t cutOfVideoMediaItemKey = -1;
					int64_t cutOfAudioMediaItemKey = -1;
					double startTimeInSeconds = 0.0;
					double endTimeInSeconds = 0.0;
					string mediaMetaDataContent = generateMediaMetadataToIngest(
						ingestionJobKey, outputFileFormat, title, imageOfVideoMediaItemKey, cutOfVideoMediaItemKey, cutOfAudioMediaItemKey,
						startTimeInSeconds, endTimeInSeconds, parametersRoot
					);

					{
						shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent = make_shared<LocalAssetIngestionEvent>();
						/*
						shared_ptr<LocalAssetIngestionEvent>
						localAssetIngestionEvent =
						_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(
								MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);
						*/

						localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

						localAssetIngestionEvent->setExternalReadOnlyStorage(false);
						localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
						localAssetIngestionEvent->setIngestionSourceFileName(localSourceFileName);
						localAssetIngestionEvent->setMMSSourceFileName(localSourceFileName);
						localAssetIngestionEvent->setWorkspace(workspace);
						localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
						localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(
							/* it + 1 == dependencies.end() ? true : */
							false
						);

						// to manage a ffmpeg bug generating a corrupted/wrong
						// avgFrameRate, we will force the concat file to have
						// the same avgFrameRate of the source media Uncomment
						// next statements in case the problem is still present
						// event in case of the ExtractTracks task if
						// (forcedAvgFrameRate != "" && concatContentType ==
						// MMSEngineDBFacade::ContentType::Video)
						//    localAssetIngestionEvent->setForcedAvgFrameRate(forcedAvgFrameRate);

						localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

						handleLocalAssetIngestionEvent(processorsThreadsNumber, *localAssetIngestionEvent);
						/*
						shared_ptr<Event2>    event =
						dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
						_multiEventsSet->addEvent(event);

						SPDLOG_INFO(string() + "addEvent: EVENT_TYPE
						(INGESTASSETEVENT)"
							+ ", _processorIdentifier: " +
						to_string(_processorIdentifier)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", getEventKey().first: " +
						to_string(event->getEventKey().first)
							+ ", getEventKey().second: " +
						to_string(event->getEventKey().second));
						*/
					}
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "extract track failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
				string errorMessage = string() + "extract track failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
			string() + "Extracting tracks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
			string() + "Extracting tracks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
