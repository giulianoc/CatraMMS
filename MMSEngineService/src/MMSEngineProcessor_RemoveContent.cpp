
#include "MMSEngineProcessor.h"
/*
#include <stdio.h>

#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "FFMpeg.h"
#include "GEOInfoTimes.h"
#include "JSONUtils.h"
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

void MMSEngineProcessor::removeContentThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "removeContentThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "removeContentThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be removed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
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

				// check if there are ingestion dependencies on this media item
				{
					if (dependencyType == Validator::DependencyType::MediaItemKey)
					{
						bool warningIfMissing = false;
						tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> mediaItemKeyDetails =
							_mmsEngineDBFacade->getMediaItemKeyDetails(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						MMSEngineDBFacade::ContentType localContentType;
						string localTitle;
						string localUserData;
						string localIngestionDate;
						int64_t ingestionJobKeyOfItemToBeRemoved;
						tie(localContentType, localTitle, localUserData, localIngestionDate, ignore, ingestionJobKeyOfItemToBeRemoved) =
							mediaItemKeyDetails;

						int ingestionDependenciesNumber = _mmsEngineDBFacade->getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
							ingestionJobKeyOfItemToBeRemoved,
							// 2022-12-18: importante essere sicuri
							true
						);
						if (ingestionDependenciesNumber > 0)
						{
							string errorMessage = string() +
												  "MediaItem cannot be removed because there are "
												  "still ingestion dependencies" +
												  ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) +
												  ", ingestionDependenciesNumber not finished: " + to_string(ingestionDependenciesNumber) +
												  ", ingestionJobKeyOfItemToBeRemoved: " + to_string(ingestionJobKeyOfItemToBeRemoved);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						bool warningIfMissing = false;
						tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemDetails =
							_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						int64_t localMediaItemKey;
						MMSEngineDBFacade::ContentType localContentType;
						string localTitle;
						string localUserData;
						string localIngestionDate;
						int64_t ingestionJobKeyOfItemToBeRemoved;
						tie(localMediaItemKey, localContentType, localTitle, localUserData, localIngestionDate, ingestionJobKeyOfItemToBeRemoved,
							ignore, ignore, ignore) = mediaItemDetails;

						int ingestionDependenciesNumber = _mmsEngineDBFacade->getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
							ingestionJobKeyOfItemToBeRemoved,
							// 2022-12-18: importante essere sicuri
							true
						);
						if (ingestionDependenciesNumber > 0)
						{
							string errorMessage = string() +
												  "MediaItem cannot be removed because there are "
												  "still ingestion dependencies" +
												  ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) +
												  ", ingestionDependenciesNumber not finished: " + to_string(ingestionDependenciesNumber) +
												  ", ingestionJobKeyOfItemToBeRemoved: " + to_string(ingestionJobKeyOfItemToBeRemoved);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					SPDLOG_INFO(
						string() + "removeMediaItem" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", mediaItemKey: " + to_string(key)
					);
					_mmsStorage->removeMediaItem(key);
				}
				else
				{
					SPDLOG_INFO(
						string() + "removePhysicalPath" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", physicalPathKey: " + to_string(key)
					);
					_mmsStorage->removePhysicalPath(key);
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "Remove Content failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
				string errorMessage = string() + "Remove Content failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
			string() + "removeContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
			string() + "removeContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

