
#include "CheckIngestionTimes.h"
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"
#include "catralibraries/StringUtils.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <openssl/evp.h>

#define MD5BUFFERSIZE 16384
// #include <fstream>
/*
#include <sstream>
#include <stdio.h>

#include "CheckEncodingTimes.h"
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
#include "catralibraries/System.h"
#include <iomanip>
#include <regex>
// #include "EMailSender.h"
#include "Magick++.h"
// #include <openssl/md5.h>
#include "spdlog/spdlog.h"
#include <openssl/evp.h>

#define MD5BUFFERSIZE 16384
*/

void MMSEngineProcessor::handleCheckIngestionEvent()
{
	try
	{
		if (isMaintenanceMode())
		{
			SPDLOG_INFO(
				string() +
				"Received handleCheckIngestionEvent, not managed it because of "
				"MaintenanceMode" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			return;
		}

		vector<tuple<int64_t, string, shared_ptr<Workspace>, string, string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus>>
			ingestionsToBeManaged;

		try
		{
			// getIngestionsToBeManaged
			//	- in case we reached the max number of threads in MMS Engine,
			//		we still have to call getIngestionsToBeManaged
			//		but it has to return ONLY tasks that do not involve creation
			// of threads 		(a lot of important tasks 		do not involve
			// threads in MMS Engine) 	That is to avoid to block every thing in
			// case we reached the max number of threads 	in MMS Engine
			bool onlyTasksNotInvolvingMMSEngineThreads = false;

			int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
			if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
			{
				_logger->warn(
					string() +
					"Not enough available threads to manage Tasks involving "
					"more threads" +
					", _processorIdentifier: " + to_string(_processorIdentifier) +
					", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count()) +
					", _processorThreads + maxAdditionalProcessorThreads: " + to_string(_processorThreads + maxAdditionalProcessorThreads)
				);

				onlyTasksNotInvolvingMMSEngineThreads = true;
			}

			_mmsEngineDBFacade->getIngestionsToBeManaged(
				ingestionsToBeManaged, _processorMMS, _maxIngestionJobsPerEvent, _timeBeforeToPrepareResourcesInMinutes,
				onlyTasksNotInvolvingMMSEngineThreads
			);

			SPDLOG_INFO(
				string() + "getIngestionsToBeManaged result" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionsToBeManaged.size: " + to_string(ingestionsToBeManaged.size())
			);
		}
		catch (AlreadyLocked &e)
		{
			_logger->warn(
				string() + "getIngestionsToBeManaged failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", exception: " + e.what()
			);

			return;
			// throw e;
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				string() + "getIngestionsToBeManaged failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", exception: " + e.what()
			);

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() + "getIngestionsToBeManaged failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", exception: " + e.what()
			);

			throw e;
		}

		for (tuple<int64_t, string, shared_ptr<Workspace>, string, string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus>
				 ingestionToBeManaged : ingestionsToBeManaged)
		{
			int64_t ingestionJobKey;
			try
			{
				string ingestionJobLabel;
				shared_ptr<Workspace> workspace;
				string ingestionDate;
				string metaDataContent;
				string sourceReference;
				MMSEngineDBFacade::IngestionType ingestionType;
				MMSEngineDBFacade::IngestionStatus ingestionStatus;

				tie(ingestionJobKey, ingestionJobLabel, workspace, ingestionDate, metaDataContent, ingestionType, ingestionStatus) =
					ingestionToBeManaged;

				SPDLOG_INFO(
					string() + "json to be processed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", ingestionJobLabel: " + ingestionJobLabel +
					", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", ingestionDate: " + ingestionDate +
					", ingestionType: " + MMSEngineDBFacade::toString(ingestionType) +
					", ingestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", metaDataContent: " + metaDataContent
				);

				try
				{
					if (ingestionType != MMSEngineDBFacade::IngestionType::RemoveContent)
					{
						_mmsEngineDBFacade->checkWorkspaceStorageAndMaxIngestionNumber(workspace->_workspaceKey);
					}
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						string() + "checkWorkspaceStorageAndMaxIngestionNumber failed" + ", _processorIdentifier: " +
						to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
					);
					string errorMessage = e.what();

					SPDLOG_INFO(
						string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) +
						", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + e.what()
					);
					try
					{
						_mmsEngineDBFacade->updateIngestionJob(
							ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_WorkspaceReachedMaxStorageOrIngestionNumber, e.what()
						);
					}
					catch (runtime_error &re)
					{
						SPDLOG_INFO(
							string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + re.what()
						);
					}
					catch (exception &ex)
					{
						SPDLOG_INFO(
							string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + ex.what()
						);
					}

					throw e;
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						string() + "checkWorkspaceStorageAndMaxIngestionNumber failed" + ", _processorIdentifier: " +
						to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
					);
					string errorMessage = e.what();

					SPDLOG_INFO(
						string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) +
						", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + e.what()
					);
					try
					{
						_mmsEngineDBFacade->updateIngestionJob(
							ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_WorkspaceReachedMaxStorageOrIngestionNumber, e.what()
						);
					}
					catch (runtime_error &re)
					{
						SPDLOG_INFO(
							string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + re.what()
						);
					}
					catch (exception &ex)
					{
						SPDLOG_INFO(
							string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_WorkspaceReachedMaxStorageOrIngestionNumber" + ", errorMessage: " + ex.what()
						);
					}

					throw e;
				}

				if (ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress ||
					ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress ||
					ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress ||
					ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
				{
					// source binary download or uploaded terminated

					string sourceFileName = to_string(ingestionJobKey) + "_source";

					{
						shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
							_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(
								MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
							);

						localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

						localAssetIngestionEvent->setExternalReadOnlyStorage(false);
						localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
						localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
						localAssetIngestionEvent->setMMSSourceFileName("");
						localAssetIngestionEvent->setWorkspace(workspace);
						localAssetIngestionEvent->setIngestionType(ingestionType);
						localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

						localAssetIngestionEvent->setMetadataContent(metaDataContent);

						shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
						_multiEventsSet->addEvent(event);

						SPDLOG_INFO(
							string() + "addEvent: EVENT_TYPE (INGESTASSETEVENT)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", getEventKey().first: " + to_string(event->getEventKey().first) +
							", getEventKey().second: " + to_string(event->getEventKey().second)
						);
					}
				}
				else // Start_TaskQueued
				{
					json parametersRoot;
					try
					{
						parametersRoot = JSONUtils::toJson(metaDataContent);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string("metadata json is not well format") +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", metaDataContent: " + metaDataContent;
						SPDLOG_ERROR(string() + errorMessage);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
							", errorMessage: " + errorMessage + ", processorMMS: " + ""
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, errorMessage
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + ex.what()
							);
						}

						throw runtime_error(errorMessage);
					}

					vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies;

					try
					{
						Validator validator(_logger, _mmsEngineDBFacade, _configurationRoot);
						if (ingestionType == MMSEngineDBFacade::IngestionType::GroupOfTasks)
							validator.validateGroupOfTasksMetadata(workspace->_workspaceKey, parametersRoot);
						else
							dependencies = validator.validateSingleTaskMetadata(workspace->_workspaceKey, ingestionType, parametersRoot);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "validateMetadata failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
						);

						string errorMessage = e.what();

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
							", errorMessage: " + errorMessage + ", processorMMS: " + ""
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, errorMessage
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + ex.what()
							);
						}

						throw runtime_error(errorMessage);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "validateMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
						);

						string errorMessage = e.what();

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
							", errorMessage: " + errorMessage + ", processorMMS: " + ""
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, errorMessage
							);
						}
						catch (runtime_error &re)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + re.what()
							);
						}
						catch (exception &ex)
						{
							SPDLOG_INFO(
								string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
								", errorMessage: " + ex.what()
							);
						}

						throw runtime_error(errorMessage);
					}

					{
						if (ingestionType == MMSEngineDBFacade::IngestionType::GroupOfTasks)
						{
							try
							{
								manageGroupOfTasks(ingestionJobKey, workspace, parametersRoot);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageGroupOfTasks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageGroupOfTasks failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::AddContent)
						{
							MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
							string mediaSourceURL;
							string mediaFileFormat;
							string md5FileCheckSum;
							int fileSizeInBytes;
							bool externalReadOnlyStorage;
							try
							{
								tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int, bool> mediaSourceDetails =
									getMediaSourceDetails(ingestionJobKey, workspace, ingestionType, parametersRoot);

								tie(nextIngestionStatus, mediaSourceURL, mediaFileFormat, md5FileCheckSum, fileSizeInBytes, externalReadOnlyStorage) =
									mediaSourceDetails;
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "getMediaSourceDetails failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "getMediaSourceDetails failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMetadataFailed" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}

							try
							{
								if (externalReadOnlyStorage)
								{
									shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent =
										_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(
											MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT
										);

									localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
									localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
									localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

									localAssetIngestionEvent->setExternalReadOnlyStorage(true);
									localAssetIngestionEvent->setExternalStorageMediaSourceURL(mediaSourceURL);
									localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
									// localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
									// localAssetIngestionEvent->setMMSSourceFileName("");
									localAssetIngestionEvent->setWorkspace(workspace);
									localAssetIngestionEvent->setIngestionType(ingestionType);
									localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

									localAssetIngestionEvent->setMetadataContent(metaDataContent);

									shared_ptr<Event2> event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
									_multiEventsSet->addEvent(event);

									SPDLOG_INFO(
										string() +
										"addEvent: EVENT_TYPE "
										"(INGESTASSETEVENT)" +
										", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
										to_string(ingestionJobKey) + ", getEventKey().first: " + to_string(event->getEventKey().first) +
										", getEventKey().second: " + to_string(event->getEventKey().second)
									);
								}
								else
								{
									// 0: no m3u8
									// 1: m3u8 by .tar.gz
									// 2: streaming (it will be saved as .mp4)
									int m3u8TarGzOrStreaming = 0;
									if (mediaFileFormat == "m3u8-tar.gz")
										m3u8TarGzOrStreaming = 1;
									else if (mediaFileFormat == "streaming-to-mp4")
										m3u8TarGzOrStreaming = 2;

									if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress)
									{
										/* 2021-02-19: check on threads is
										 *already done in
										 *handleCheckIngestionEvent 2021-06-19:
										 *we still have to check the thread
										 *limit because, in case
										 *handleCheckIngestionEvent gets 20
										 *events, we have still to postpone all
										 *the events overcoming the thread limit
										 */
										int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
										if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
										{
											_logger->warn(
												string() +
												"Not enough available threads "
												"to manage "
												"downloadMediaSourceFileThread,"
												" activity is postponed" +
												", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", "
												"_processorsThreadsNumber.use_"
												"count(): " +
												to_string(_processorsThreadsNumber.use_count()) +
												", _processorThreads + "
												"maxAdditionalProcessorThreads:"
												" " +
												to_string(_processorThreads + maxAdditionalProcessorThreads)
											);

											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
										}
										else
										{
											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);

											// 2021-09-02: regenerateTimestamps
											// is used only
											//	in case of streaming-to-mp4
											//	(see
											// docs/TASK_01_Add_Content_JSON_Format.txt)
											bool regenerateTimestamps = false;
											if (mediaFileFormat == "streaming-to-mp4")
												regenerateTimestamps = JSONUtils::asBool(parametersRoot, "regenerateTimestamps", false);

											thread downloadMediaSource(
												&MMSEngineProcessor::downloadMediaSourceFileThread, this, _processorsThreadsNumber, mediaSourceURL,
												regenerateTimestamps, m3u8TarGzOrStreaming, ingestionJobKey, workspace
											);
											downloadMediaSource.detach();
										}
									}
									else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress)
									{
										/* 2021-02-19: check on threads is
										 *already done in
										 *handleCheckIngestionEvent 2021-06-19:
										 *we still have to check the thread
										 *limit because, in case
										 *handleCheckIngestionEvent gets 20
										 *events, we have still to postpone all
										 *the events overcoming the thread limit
										 */
										int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
										if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
										{
											_logger->warn(
												string() +
												"Not enough available threads "
												"to manage "
												"moveMediaSourceFileThread, "
												"activity is postponed" +
												", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", "
												"_processorsThreadsNumber.use_"
												"count(): " +
												to_string(_processorsThreadsNumber.use_count()) +
												", _processorThreads + "
												"maxAdditionalProcessorThreads:"
												" " +
												to_string(_processorThreads + maxAdditionalProcessorThreads)
											);

											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
										}
										else
										{
											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);

											thread moveMediaSource(
												&MMSEngineProcessor::moveMediaSourceFileThread, this, _processorsThreadsNumber, mediaSourceURL,
												m3u8TarGzOrStreaming, ingestionJobKey, workspace
											);
											moveMediaSource.detach();
										}
									}
									else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress)
									{
										/* 2021-02-19: check on threads is
										 *already done in
										 *handleCheckIngestionEvent 2021-06-19:
										 *we still have to check the thread
										 *limit because, in case
										 *handleCheckIngestionEvent gets 20
										 *events, we have still to postpone all
										 *the events overcoming the thread limit
										 */
										int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
										if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
										{
											_logger->warn(
												string() +
												"Not enough available threads "
												"to manage "
												"copyMediaSourceFileThread, "
												"activity is postponed" +
												", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", "
												"_processorsThreadsNumber.use_"
												"count(): " +
												to_string(_processorsThreadsNumber.use_count()) +
												", _processorThreads + "
												"maxAdditionalProcessorThreads:"
												" " +
												to_string(_processorThreads + maxAdditionalProcessorThreads)
											);

											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
										}
										else
										{
											string errorMessage = "";
											string processorMMS = "";

											SPDLOG_INFO(
												string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												", ingestionJobKey: " + to_string(ingestionJobKey) +
												", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
												", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);

											thread copyMediaSource(
												&MMSEngineProcessor::copyMediaSourceFileThread, this, _processorsThreadsNumber, mediaSourceURL,
												m3u8TarGzOrStreaming, ingestionJobKey, workspace
											);
											copyMediaSource.detach();
										}
									}
									else // if (nextIngestionStatus ==
										 // MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
									{
										string errorMessage = "";
										string processorMMS = "";

										SPDLOG_INFO(
											string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											", ingestionJobKey: " + to_string(ingestionJobKey) +
											", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
											", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
										);
										_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);
									}
								}
							}
							catch (exception &e)
							{
								string errorMessage = string("Downloading media source or update "
															 "Ingestion job failed") +
													  ", _processorIdentifier: " + to_string(_processorIdentifier) +
													  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what();
								SPDLOG_ERROR(string() + errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::RemoveContent)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/*
								removeContentTask(
										ingestionJobKey,
										workspace,
										parametersRoot,
										dependencies);
								*/
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage removeContentThread, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread removeContentThread(
										&MMSEngineProcessor::removeContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									removeContentThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "removeContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "removeContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::FTPDelivery)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/*
								ftpDeliveryContentTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage ftpDeliveryContentThread, "
										"activity is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread ftpDeliveryContentThread(
										&MMSEngineProcessor::ftpDeliveryContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									ftpDeliveryContentThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "ftpDeliveryContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "ftpDeliveryContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LocalCopy)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								if (!_localCopyTaskEnabled)
								{
									string errorMessage = string("Local-Copy Task is not enabled "
																 "in this MMS deploy") +
														  ", _processorIdentifier: " + to_string(_processorIdentifier) +
														  ", ingestionJobKey: " + to_string(ingestionJobKey);
									SPDLOG_ERROR(string() + errorMessage);

									throw runtime_error(errorMessage);
								}

								/*
								// threads check is done inside
								localCopyContentTask localCopyContentTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage localCopyContent, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread localCopyContentThread(
										&MMSEngineProcessor::localCopyContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									localCopyContentThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "localCopyContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "localCopyContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::HTTPCallback)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/*
								// threads check is done inside httpCallbackTask
								httpCallbackTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage http callback, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread httpCallbackThread(
										&MMSEngineProcessor::httpCallbackThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									httpCallbackThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "httpCallbackThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "httpCallbackThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Encode)
						{
							try
							{
								manageEncodeTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageEncodeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageEncodeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::VideoSpeed)
						{
							try
							{
								manageVideoSpeedTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageVideoSpeedTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageVideoSpeedTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::PictureInPicture)
						{
							try
							{
								managePictureInPictureTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "managePictureInPictureTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "managePictureInPictureTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::IntroOutroOverlay)
						{
							try
							{
								manageIntroOutroOverlayTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageIntroOutroOverlayTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageIntroOutroOverlayTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::AddSilentAudio)
						{
							try
							{
								manageAddSilentAudioTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageAddSilentAudioTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageAddSilentAudioTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Frame ||
								 ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames ||
								 ingestionType == MMSEngineDBFacade::IngestionType::IFrames ||
								 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames ||
								 ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames ||
									ingestionType == MMSEngineDBFacade::IngestionType::IFrames ||
									ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames ||
									ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
								{
									// adds an encoding job
									manageGenerateFramesTask(ingestionJobKey, workspace, ingestionType, parametersRoot, dependencies);
								}
								else // Frame
								{
									/* 2021-02-19: check on threads is already
									 *done in handleCheckIngestionEvent
									 * 2021-06-19: we still have to check the
									 *thread limit because, in case
									 *handleCheckIngestionEvent gets 20 events,
									 *		we have still to postpone all the
									 *events overcoming the thread limit
									 */
									int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
									if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
									{
										_logger->warn(
											string() +
											"Not enough available threads to "
											"manage changeFileFormatThread, "
											"activity is postponed" +
											", _processorIdentifier: " + to_string(_processorIdentifier) +
											", ingestionJobKey: " + to_string(ingestionJobKey) +
											", "
											"_processorsThreadsNumber.use_"
											"count(): " +
											to_string(_processorsThreadsNumber.use_count()) +
											", _processorThreads + "
											"maxAdditionalProcessorThreads: " +
											to_string(_processorThreads + maxAdditionalProcessorThreads)
										);

										string errorMessage = "";
										string processorMMS = "";

										SPDLOG_INFO(
											string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
											", ingestionJobKey: " + to_string(ingestionJobKey) +
											", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
											", processorMMS: " + processorMMS
										);
										_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
									}
									else
									{
										thread generateAndIngestFrameThread(
											&MMSEngineProcessor::generateAndIngestFrameThread, this, _processorsThreadsNumber, ingestionJobKey,
											workspace, ingestionType, parametersRoot,
											// it cannot be passed as reference
											// because it will change soon by
											// the parent thread
											dependencies
										);
										generateAndIngestFrameThread.detach();
										/*
										generateAndIngestFramesTask(
											ingestionJobKey,
											workspace,
											ingestionType,
											parametersRoot,
											dependencies);
										*/
									}
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "generateAndIngestFramesTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "generateAndIngestFramesTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Slideshow)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageSlideShowTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageSlideShowTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageSlideShowTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::ConcatDemuxer)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage manageConcatThread, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread manageConcatThread(
										&MMSEngineProcessor::manageConcatThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,

										// it cannot be passed as reference
										// because it will change soon by the
										// parent thread
										dependencies
									);
									manageConcatThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageConcatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageConcatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Cut)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage manageCutMediaThread, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread manageCutMediaThread(
										&MMSEngineProcessor::manageCutMediaThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									manageCutMediaThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageCutMediaThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageCutMediaThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							/*
							try
							{
								generateAndIngestCutMediaTask(
										ingestionJobKey,
										workspace,
										parametersRoot,
										dependencies);
							}
							catch(runtime_error& e)
							{
								SPDLOG_ERROR(string() +
							"generateAndIngestCutMediaTask failed"
									+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(string() + "Update
							IngestionJob"
									+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
									+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
									+ ", IngestionStatus: " +
							"End_IngestionFailure"
									+ ", errorMessage: " + errorMessage
									+ ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob
							(ingestionJobKey,
										MMSEngineDBFacade::IngestionStatus::End_IngestionFailure,
										errorMessage
										);
								}
								catch(runtime_error& re)
								{
									SPDLOG_INFO(string() + "Update
							IngestionJob failed"
										+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", IngestionStatus: " +
							"End_IngestionFailure"
										+ ", errorMessage: " + re.what()
									);
								}
								catch(exception& ex)
								{
									SPDLOG_INFO(string() + "Update
							IngestionJob failed"
										+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", IngestionStatus: " +
							"End_IngestionFailure"
										+ ", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch(exception& e)
							{
								SPDLOG_ERROR(string() +
							"generateAndIngestCutMediaTask failed"
									+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(string() + "Update
							IngestionJob"
									+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
									+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
									+ ", IngestionStatus: " +
							"End_IngestionFailure"
									+ ", errorMessage: " + errorMessage
									+ ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob
							(ingestionJobKey,
										MMSEngineDBFacade::IngestionStatus::End_IngestionFailure,
										errorMessage
										);
								}
								catch(runtime_error& re)
								{
									SPDLOG_INFO(string() + "Update
							IngestionJob failed"
										+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", IngestionStatus: " +
							"End_IngestionFailure"
										+ ", errorMessage: " + re.what()
									);
								}
								catch(exception& ex)
								{
									SPDLOG_INFO(string() + "Update
							IngestionJob failed"
										+ ", _processorIdentifier: " +
							to_string(_processorIdentifier)
										+ ", ingestionJobKey: " +
							to_string(ingestionJobKey)
										+ ", IngestionStatus: " +
							"End_IngestionFailure"
										+ ", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							*/
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::ExtractTracks)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage extractTracksContentThread, "
										"activity is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread extractTracksContentThread(
										&MMSEngineProcessor::extractTracksContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									extractTracksContentThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "extractTracksContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "extractTracksContentThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayImageOnVideo)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageOverlayImageOnVideoTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageOverlayImageOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageOverlayImageOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayTextOnVideo)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageOverlayTextOnVideoTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageOverlayTextOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageOverlayTextOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::EmailNotification)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage email notification, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread emailNotificationThread(
										&MMSEngineProcessor::emailNotificationThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									emailNotificationThread.detach();
								}
								/*
								manageEmailNotificationTask(
										ingestionJobKey,
										workspace,
										parametersRoot,
										dependencies);
								*/
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "emailNotificationThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "emailNotificationThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::CheckStreaming)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage check streaming, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread checkStreamingThread(
										&MMSEngineProcessor::checkStreamingThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot
									);
									checkStreamingThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "checkStreamingThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "checkStreamingThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::MediaCrossReference)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageMediaCrossReferenceTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::PostOnFacebook)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage post on facebook, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread postOnFacebookThread(
										&MMSEngineProcessor::postOnFacebookThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									postOnFacebookThread.detach();
								}
								/*
								postOnFacebookTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "postOnFacebookThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "postOnFacebookThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::PostOnYouTube)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage post on youtube, activity is "
										"postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread postOnYouTubeThread(
										&MMSEngineProcessor::postOnYouTubeThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									postOnYouTubeThread.detach();
								}
								/*
								postOnYouTubeTask(
										ingestionJobKey,
										ingestionStatus,
										workspace,
										parametersRoot,
										dependencies);
								*/
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "postOnYouTubeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "postOnYouTubeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::FaceRecognition)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageFaceRecognitionMediaTask(ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageFaceRecognitionMediaTask failed" + ", _processorIdentifier: " +
									to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageFaceRecognitionMediaTask failed" + ", _processorIdentifier: " +
									to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::FaceIdentification)
						{
							// mediaItemKeysDependency is present because
							// checked by
							// _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageFaceIdentificationMediaTask(ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageFaceIdentificationMediaTask failed" + ", _processorIdentifier: " +
									to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageFaceIdentificationMediaTask failed" + ", _processorIdentifier: " +
									to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveRecorder)
						{
							try
							{
								manageLiveRecorder(ingestionJobKey, ingestionJobLabel, ingestionStatus, workspace, parametersRoot);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveRecorder failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveRecorder failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveProxy)
						{
							try
							{
								manageLiveProxy(ingestionJobKey, ingestionStatus, workspace, parametersRoot);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::VODProxy)
						{
							try
							{
								manageVODProxy(ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageVODProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageVODProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::Countdown)
						{
							try
							{
								manageCountdown(ingestionJobKey, ingestionStatus, /* ingestionDate, */ workspace, parametersRoot, dependencies);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveGrid)
						{
							try
							{
								manageLiveGrid(ingestionJobKey, ingestionStatus, workspace, parametersRoot);
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveGrid failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveGrid failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveCut)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage manageLiveCutThread, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									string segmenterType = "hlsSegmenter";
									// string segmenterType = "streamSegmenter";
									if (segmenterType == "hlsSegmenter")
									{
										thread manageLiveCutThread(
											&MMSEngineProcessor::manageLiveCutThread_hlsSegmenter, this, _processorsThreadsNumber, ingestionJobKey,
											ingestionJobLabel, workspace, parametersRoot
										);
										manageLiveCutThread.detach();
									}
									else
									{
										thread manageLiveCutThread(
											&MMSEngineProcessor::manageLiveCutThread_streamSegmenter, this, _processorsThreadsNumber, ingestionJobKey,
											workspace, parametersRoot
										);
										manageLiveCutThread.detach();
									}
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::YouTubeLiveBroadcast)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 * in handleCheckIngestionEvent
								 * 2021-06-19: we still have to check the thread
								 *limit because, in case
								 *handleCheckIngestionEvent gets 20 events, we
								 *have still to postpone all the events
								 *overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage YouTubeLiveBroadcast, activity "
										"is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread youTubeLiveBroadcastThread(
										&MMSEngineProcessor::youTubeLiveBroadcastThread, this, _processorsThreadsNumber, ingestionJobKey,
										ingestionJobLabel, workspace, parametersRoot
									);
									youTubeLiveBroadcastThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "youTubeLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "youTubeLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::FacebookLiveBroadcast)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 * in handleCheckIngestionEvent
								 * 2021-06-19: we still have to check the thread
								 *limit because, in case
								 *handleCheckIngestionEvent gets 20 events, we
								 *have still to postpone all the events
								 *overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage facebookLiveBroadcastThread, "
										"activity is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread facebookLiveBroadcastThread(
										&MMSEngineProcessor::facebookLiveBroadcastThread, this, _processorsThreadsNumber, ingestionJobKey,
										ingestionJobLabel, workspace, parametersRoot
									);
									facebookLiveBroadcastThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "facebookLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "facebookLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else if (ingestionType == MMSEngineDBFacade::IngestionType::ChangeFileFormat)
						{
							try
							{
								/* 2021-02-19: check on threads is already done
								 *in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because,
								 *		in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the
								 *events overcoming the thread limit
								 */
								int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
								if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
								{
									_logger->warn(
										string() +
										"Not enough available threads to "
										"manage changeFileFormatThread, "
										"activity is postponed" +
										", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", "
										"_processorsThreadsNumber.use_count():"
										" " +
										to_string(_processorsThreadsNumber.use_count()) +
										", _processorThreads + "
										"maxAdditionalProcessorThreads: " +
										to_string(_processorThreads + maxAdditionalProcessorThreads)
									);

									string errorMessage = "";
									string processorMMS = "";

									SPDLOG_INFO(
										string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) +
										", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus) + ", errorMessage: " + errorMessage +
										", processorMMS: " + processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread changeFileFormatThread(
										&MMSEngineProcessor::changeFileFormatThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as
													 // reference because it
													 // will change soon by the
													 // parent thread
									);
									changeFileFormatThread.detach();
								}
							}
							catch (runtime_error &e)
							{
								SPDLOG_ERROR(
									string() + "changeFileFormatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									string() + "changeFileFormatThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								SPDLOG_INFO(
									string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
									", errorMessage: " + errorMessage + ", processorMMS: " + ""
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (runtime_error &re)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + re.what()
									);
								}
								catch (exception &ex)
								{
									SPDLOG_INFO(
										string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
										", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" +
										", errorMessage: " + ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
						}
						else
						{
							string errorMessage = string("Unknown IngestionType") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) +
												  ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
							SPDLOG_ERROR(string() + errorMessage);

							SPDLOG_INFO(
								string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
								", errorMessage: " + errorMessage + ", processorMMS: " + ""
							);
							try
							{
								_mmsEngineDBFacade->updateIngestionJob(
									ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, errorMessage
								);
							}
							catch (runtime_error &re)
							{
								SPDLOG_INFO(
									string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
									", errorMessage: " + re.what()
								);
							}
							catch (exception &ex)
							{
								SPDLOG_INFO(
									string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_ValidationMediaSourceFailed" +
									", errorMessage: " + ex.what()
								);
							}

							throw runtime_error(errorMessage);
						}
					}
				}
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "Exception managing the Ingestion entry" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "Exception managing the Ingestion entry" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
				);
			}
		}

		if (ingestionsToBeManaged.size() >= _maxIngestionJobsPerEvent)
		{
			shared_ptr<Event2> event = _multiEventsSet->getEventsFactory()->getFreeEvent<Event2>(MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTIONEVENT);

			event->setSource(MMSENGINEPROCESSORNAME);
			event->setDestination(MMSENGINEPROCESSORNAME);
			event->setExpirationTimePoint(chrono::system_clock::now());

			_multiEventsSet->addEvent(event);

			SPDLOG_DEBUG(
				string() + "addEvent: EVENT_TYPE" + ", MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION" + ", getEventKey().first: " +
				to_string(event->getEventKey().first) + ", getEventKey().second: " + to_string(event->getEventKey().second)
			);
		}
	}
	catch (...)
	{
		SPDLOG_ERROR(string() + "handleCheckIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier));
	}
}

void MMSEngineProcessor::handleLocalAssetIngestionEventThread(
	shared_ptr<long> processorsThreadsNumber, LocalAssetIngestionEvent localAssetIngestionEvent
)
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleLocalAssetIngestionEventThread", _processorIdentifier, _processorsThreadsNumber.use_count(),
		localAssetIngestionEvent.getIngestionJobKey()
	);

	try
	{
		// 2023-11-23: inizialmente handleLocalAssetIngestionEvent era inclusa
		// in handleLocalAssetIngestionEventThread. Poi le funzioni sono state
		// divise perche handleLocalAssetIngestionEvent viene chiamata da
		// diversi threads e quindi non poteva istanziare ThreadStatistic in
		// quanto si sarebbe utilizzato lo stesso threadId per due istanze di
		// ThreadStatistic e avremmo avuto errore quando, all'interno di
		// ThreadStatistic, si sarebbe cercato di inserire il threadId nella
		// mappa
		handleLocalAssetIngestionEvent(processorsThreadsNumber, localAssetIngestionEvent);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", localAssetIngestionEvent.getMetadataContent(): " + localAssetIngestionEvent.getMetadataContent() + ", exception: " + e.what()
		);

		// throw e;
		return; // return because it is a thread
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		// throw e;
		return; // return because it is a thread
	}
}

void MMSEngineProcessor::handleLocalAssetIngestionEvent(shared_ptr<long> processorsThreadsNumber, LocalAssetIngestionEvent localAssetIngestionEvent)
{

	SPDLOG_INFO(
		string() + "handleLocalAssetIngestionEvent" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", ingestionSourceFileName: " +
		localAssetIngestionEvent.getIngestionSourceFileName() + ", metadataContent: " + localAssetIngestionEvent.getMetadataContent() +
		", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
	);

	json parametersRoot;
	try
	{
		string sMetadataContent = localAssetIngestionEvent.getMetadataContent();

		// LF and CR create problems to the json parser...
		while (sMetadataContent.size() > 0 && (sMetadataContent.back() == 10 || sMetadataContent.back() == 13))
			sMetadataContent.pop_back();

		parametersRoot = JSONUtils::toJson(sMetadataContent);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "parsing parameters failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", localAssetIngestionEvent.getMetadataContent(): " + localAssetIngestionEvent.getMetadataContent() + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "validateMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}

	fs::path binaryPathName;
	string externalStorageRelativePathName;
	try
	{
		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			string workspaceIngestionBinaryPathName;

			workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(localAssetIngestionEvent.getWorkspace());
			workspaceIngestionBinaryPathName.append("/").append(localAssetIngestionEvent.getIngestionSourceFileName());

			string field = "fileFormat";
			string fileFormat = JSONUtils::asString(parametersRoot, field, "");
			if (fileFormat == "streaming-to-mp4")
			{
				// .mp4 is used in
				// 1. downloadMediaSourceFileThread (when the streaming-to-mp4 is
				// downloaded in a .mp4 file
				// 2. here, handleLocalAssetIngestionEvent (when the
				// IngestionRepository file name
				//		is built "consistent" with the above step no. 1)
				// 3. handleLocalAssetIngestionEvent (when the MMS file name is
				// generated)
				binaryPathName = workspaceIngestionBinaryPathName + ".mp4";
			}
			else if (fileFormat == "m3u8-tar.gz")
			{
				// 2023-03-19: come specificato in
				// TASK_01_Add_Content_JSON_Format.txt, in caso di
				//	fileFormat == "m3u8-tar.gz" ci sono due opzioni:
				//	1. in case of copy:// or move:// sourceURL, the tar.gz file
				// name will be the same name 		of the internal directory.
				// In questo caso
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments è stato
				// già chiamato dai metodi
				// MMSEngineProcessor::moveMediaSourceFileThread 		e
				// MMSEngineProcessor::copyMediaSourceFileThread. Era importante
				// che i due precedenti 		metodi chiamassero
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments perchè
				// solo 		loro sapevano il nome del file .tar.gz e quindi
				// il nome della directory contenuta 		nel file .tar.gz.
				// In questo scenario quindi,
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments è stato
				// già 		chiamato e workspaceIngestionBinaryPathName è la
				// directory <ingestionJobKey>_source
				//	2. in caso di <download> o <upload tramite PUSH>, come
				// indicato 		in TASK_01_Add_Content_JSON_Format.txt, il
				// .tar.gz conterrà una directory dal nome
				// "content". 		2.1 In caso di <download>, il metodo
				// MMSEngineProcessor::downloadMediaSourceFileThread
				// chiama lui stesso
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments, per
				// cui, anche 			in questo caso,
				// workspaceIngestionBinaryPathName è la directory
				//<ingestionJobKey>_source 		2.2 In caso di <upload tramite
				// PUSH>, abbiamo evitato che API::uploadedBinary
				// chiamasse
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments perchè
				// manageTar... 			potrebbe impiegare anche parecchi
				// minuti e l'API non puo' rimanere appesa 			per diversi
				// minuti, avremmo timeout del load balancer e/o dei clients in
				// generale. 			Quindi, solo per questo scenario,
				// chiamiamo qui il metodo
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments
				// Possiamo distinguere questo caso dagli altri perchè, non
				// essendo stato chiamato 			il metodo
				// MMSStorage::manageTarFileInCaseOfIngestionOfSegments, avremmo
				// il file 			<ingestionJobKey>_source.tar.gz e non, come
				// nei casi precedenti, 			la directory
				// <ingestionJobKey>_source.

				// i.e.:
				// /var/catramms/storage/IngestionRepository/users/8/2848783_source.tar.gz
				string localWorkspaceIngestionBinaryPathName = workspaceIngestionBinaryPathName + ".tar.gz";
				if (fs::exists(localWorkspaceIngestionBinaryPathName) && fs::is_regular_file(localWorkspaceIngestionBinaryPathName))
				{
					// caso 2.2 sopra
					try
					{
						string localSourceBinaryPathFile = "/content.tar.gz";

						_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(
							localAssetIngestionEvent.getIngestionJobKey(), localWorkspaceIngestionBinaryPathName,
							_mmsStorage->getWorkspaceIngestionRepository(localAssetIngestionEvent.getWorkspace()), localSourceBinaryPathFile
						);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments "
													 "failed") +
											  ", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
											  ", localWorkspaceIngestionBinaryPathName: " + localWorkspaceIngestionBinaryPathName;
						SPDLOG_ERROR(string() + errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				// i.e.:
				// /var/catramms/storage/IngestionRepository/users/8/2848783_source
				binaryPathName = workspaceIngestionBinaryPathName;
			}
			else
				binaryPathName = workspaceIngestionBinaryPathName;
		}
		else
		{
			string mediaSourceURL = localAssetIngestionEvent.getExternalStorageMediaSourceURL();

			string externalStoragePrefix("externalStorage://");
			if (!(mediaSourceURL.size() >= externalStoragePrefix.size() &&
				  0 == mediaSourceURL.compare(0, externalStoragePrefix.size(), externalStoragePrefix)))
			{
				string errorMessage =
					string("mediaSourceURL is not an externalStorage reference") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mediaSourceURL: " + mediaSourceURL;

				SPDLOG_ERROR(string() + errorMessage);

				throw runtime_error(errorMessage);
			}
			externalStorageRelativePathName = mediaSourceURL.substr(externalStoragePrefix.length());
			binaryPathName = _mmsStorage->getMMSRootRepository() / ("ExternalStorage_" + localAssetIngestionEvent.getWorkspace()->_directoryName);
			binaryPathName /= externalStorageRelativePathName;
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "binaryPathName initialization failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "binaryPathName initialization failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}

	SPDLOG_INFO(
		string() + "binaryPathName" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
	);

	string metadataFileContent;
	Validator validator(_logger, _mmsEngineDBFacade, _configurationRoot);
	try
	{
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies;

		dependencies = validator.validateSingleTaskMetadata(
			localAssetIngestionEvent.getWorkspace()->_workspaceKey, localAssetIngestionEvent.getIngestionType(), parametersRoot
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "validateMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", localAssetIngestionEvent.getMetadataContent(): " + localAssetIngestionEvent.getMetadataContent() + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "validateMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMetadataFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}

	MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
	string mediaFileFormat;
	string md5FileCheckSum;
	int fileSizeInBytes;
	bool externalReadOnlyStorage;
	try
	{
		string mediaSourceURL;

		tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int, bool> mediaSourceDetails = getMediaSourceDetails(
			localAssetIngestionEvent.getIngestionJobKey(), localAssetIngestionEvent.getWorkspace(), localAssetIngestionEvent.getIngestionType(),
			parametersRoot
		);

		tie(nextIngestionStatus, mediaSourceURL, mediaFileFormat, md5FileCheckSum, fileSizeInBytes, externalReadOnlyStorage) = mediaSourceDetails;

		// in case of youtube url, the real URL to be used has to be calcolated
		// Here the mediaFileFormat is retrieved
		{
			string youTubePrefix1("https://www.youtube.com/");
			string youTubePrefix2("https://youtu.be/");
			if ((mediaSourceURL.size() >= youTubePrefix1.size() && 0 == mediaSourceURL.compare(0, youTubePrefix1.size(), youTubePrefix1)) ||
				(mediaSourceURL.size() >= youTubePrefix2.size() && 0 == mediaSourceURL.compare(0, youTubePrefix2.size(), youTubePrefix2)))
			{
				FFMpeg ffmpeg(_configurationRoot, _logger);
				pair<string, string> streamingURLDetails =
					ffmpeg.retrieveStreamingYouTubeURL(localAssetIngestionEvent.getIngestionJobKey(), mediaSourceURL);

				tie(ignore, mediaFileFormat) = streamingURLDetails;

				SPDLOG_INFO(
					string() + "Retrieve streaming YouTube URL" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", initial YouTube URL: " + mediaSourceURL
				);
			}
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "getMediaSourceDetails failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "getMediaSourceDetails failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}

	try
	{
		validateMediaSourceFile(
			localAssetIngestionEvent.getIngestionJobKey(), binaryPathName.string(), mediaFileFormat, md5FileCheckSum, fileSizeInBytes
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "validateMediaSourceFile failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "validateMediaSourceFile failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", exception: " + e.what()
		);

		string errorMessage = e.what();

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
			", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", IngestionStatus: " + "End_ValidationMediaSourceFailed" + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}

	string mediaSourceFileName;
	string mmsAssetPathName;
	string relativePathToBeUsed;
	long mmsPartitionUsed;
	try
	{
		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			mediaSourceFileName = localAssetIngestionEvent.getMMSSourceFileName();
			if (mediaSourceFileName == "")
			{
				mediaSourceFileName = localAssetIngestionEvent.getIngestionSourceFileName();
				// .mp4 is used in
				// 1. downloadMediaSourceFileThread (when the streaming-to-mp4 is
				// downloaded in a .mp4 file
				// 2. handleLocalAssetIngestionEvent (when the
				// IngestionRepository file name
				//		is built "consistent" with the above step no. 1)
				// 3. here, handleLocalAssetIngestionEvent (when the MMS file
				// name is generated)
				if (mediaFileFormat == "streaming-to-mp4")
					mediaSourceFileName += ".mp4";
				else if (mediaFileFormat == "m3u8-tar.gz")
					; // mediaSourceFileName is like "2131450_source"
				else
					mediaSourceFileName += ("." + mediaFileFormat);
			}

			relativePathToBeUsed = _mmsEngineDBFacade->nextRelativePathToBeUsed(localAssetIngestionEvent.getWorkspace()->_workspaceKey);

			bool isDirectory = fs::is_directory(binaryPathName);

			unsigned long mmsPartitionIndexUsed;
			bool deliveryRepositoriesToo = true;
			mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
				localAssetIngestionEvent.getIngestionJobKey(), binaryPathName, localAssetIngestionEvent.getWorkspace()->_directoryName,
				mediaSourceFileName, relativePathToBeUsed, &mmsPartitionIndexUsed,
				// &sourceFileType,
				deliveryRepositoriesToo, localAssetIngestionEvent.getWorkspace()->_territories
			);
			mmsPartitionUsed = mmsPartitionIndexUsed;

			// if (mediaFileFormat == "m3u8")
			if (isDirectory)
				relativePathToBeUsed += (mediaSourceFileName + "/");
		}
		else
		{
			mmsAssetPathName = binaryPathName.string();
			mmsPartitionUsed = -1;

			size_t fileNameIndex = externalStorageRelativePathName.find_last_of("/");
			if (fileNameIndex == string::npos)
			{
				string errorMessage = string() + "No fileName found in externalStorageRelativePathName" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
									  ", externalStorageRelativePathName: " + externalStorageRelativePathName;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			relativePathToBeUsed = externalStorageRelativePathName.substr(0, fileNameIndex + 1);
			mediaSourceFileName = externalStorageRelativePathName.substr(fileNameIndex + 1);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsStorage->moveAssetInMMSRepository failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsStorage->moveAssetInMMSRepository failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", binaryPathName: " + binaryPathName.string()
				);

				fs::remove_all(binaryPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		throw e;
	}

	string m3u8FileName;
	if (mediaFileFormat == "m3u8-tar.gz")
	{
		// in this case mmsAssetPathName refers a directory and we need to find
		// out the m3u8 file name

		try
		{
			for (fs::directory_entry const &entry : fs::directory_iterator(mmsAssetPathName))
			{
				try
				{
					if (!entry.is_regular_file())
						continue;

					// string m3u8Suffix(".m3u8");
					// if (entry.path().filename().string().size() >= m3u8Suffix.size() &&
					// 	0 == entry.path().filename().string().compare(
					// 		 entry.path().filename().string().size() - m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix
					//  ))
					if (entry.path().filename().string().ends_with(".m3u8"))
					{
						m3u8FileName = entry.path().filename().string();

						break;
					}
				}
				catch (runtime_error &e)
				{
					string errorMessage = string() + "listing directory failed" + ", e.what(): " + e.what();
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
				catch (exception &e)
				{
					string errorMessage = string() + "listing directory failed" + ", e.what(): " + e.what();
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
			}

			if (m3u8FileName == "")
			{
				string errorMessage = string() + "m3u8 file not found" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
									  ", mmsAssetPathName: " + mmsAssetPathName;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			mediaSourceFileName = m3u8FileName;
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				string() + "retrieving m3u8 file failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove directory" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() + "retrieving m3u8 file failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
	}

	MMSEngineDBFacade::ContentType contentType;

	tuple<int64_t, long, json> mediaInfoDetails;
	vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
	vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;

	int imageWidth = -1;
	int imageHeight = -1;
	string imageFormat;
	int imageQuality = -1;
	if (validator.isVideoAudioFileFormat(mediaFileFormat))
	{
		try
		{
			FFMpeg ffmpeg(_configurationRoot, _logger);
			// tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
			// mediaInfo;

			int timeoutInSeconds = 20;
			bool isMMSAssetPathName = true;
			if (mediaFileFormat == "m3u8-tar.gz")
				mediaInfoDetails = ffmpeg.getMediaInfo(
					localAssetIngestionEvent.getIngestionJobKey(), isMMSAssetPathName, timeoutInSeconds, mmsAssetPathName + "/" + m3u8FileName,
					videoTracks, audioTracks
				);
			else
				mediaInfoDetails = ffmpeg.getMediaInfo(
					localAssetIngestionEvent.getIngestionJobKey(), isMMSAssetPathName, timeoutInSeconds, mmsAssetPathName, videoTracks, audioTracks
				);

			int64_t durationInMilliSeconds = -1;
			long bitRate = -1;
			tie(durationInMilliSeconds, bitRate, ignore) = mediaInfoDetails;

			SPDLOG_INFO(
				string() + "ffmpeg.getMediaInfo" + ", mmsAssetPathName: " + mmsAssetPathName +
				", durationInMilliSeconds: " + to_string(durationInMilliSeconds) + ", bitRate: " + to_string(bitRate) +
				", videoTracks.size: " + to_string(videoTracks.size()) + ", audioTracks.size: " + to_string(audioTracks.size())
			);

			/*
			tie(durationInMilliSeconds, bitRate,
				videoCodecName, videoProfile, videoWidth, videoHeight,
			videoAvgFrameRate, videoBitRate, audioCodecName, audioSampleRate,
			audioChannels, audioBitRate) = mediaInfo;
			*/

			/*
			 * 2019-10-13: commented because I guess the avg frame rate returned
			by ffmpeg is OK
			 * avg frame rate format is: total duration / total # of frames
			if (localAssetIngestionEvent.getForcedAvgFrameRate() != "")
			{
				SPDLOG_INFO(string() + "handleLocalAssetIngestionEvent.
			Forced Avg Frame Rate"
					+ ", current avgFrameRate: " + videoAvgFrameRate
					+ ", forced avgFrameRate: " +
			localAssetIngestionEvent.getForcedAvgFrameRate()
				);

				videoAvgFrameRate =
			localAssetIngestionEvent.getForcedAvgFrameRate();
			}
			*/

			if (videoTracks.size() == 0)
				contentType = MMSEngineDBFacade::ContentType::Audio;
			else
				contentType = MMSEngineDBFacade::ContentType::Video;
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				string() + "EncoderVideoAudioProxy::getMediaInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);
					fs::remove_all(mmsAssetPathName);
					/*
					size_t fileNameIndex = mmsAssetPathName.find_last_of("/");
					if (fileNameIndex == string::npos)
					{
						string errorMessage = string() + "No fileName found
					in mmsAssetPathName"
							+ ", _processorIdentifier: " +
					to_string(_processorIdentifier)
							+ ", ingestionJobKey: " +
					to_string(localAssetIngestionEvent.getIngestionJobKey())
							+ ", mmsAssetPathName: " + mmsAssetPathName
						;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourcePathName = mmsAssetPathName;
					string destBinaryPathName =
					"/var/catramms/storage/MMSWorkingAreaRepository/Staging" +
					mmsAssetPathName.substr(fileNameIndex);
					SPDLOG_INFO(string() + "Moving"
						+ ", _processorIdentifier: " +
					to_string(_processorIdentifier)
						+ ", ingestionJobKey: " +
					to_string(localAssetIngestionEvent.getIngestionJobKey())
						+ ", sourcePathName: " + sourcePathName
						+ ", destBinaryPathName: " + destBinaryPathName
					);
					int64_t elapsedInSeconds =
					MMSStorage::move(localAssetIngestionEvent.getIngestionJobKey(),
					sourcePathName, destBinaryPathName, _logger);
					*/
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() +
				"EncoderVideoAudioProxy::getVideoOrAudioDurationInMilliSeconds "
				"failed" +
				", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
	}
	else if (validator.isImageFileFormat(mediaFileFormat))
	{
		try
		{
			SPDLOG_INFO(
				string() + "Processing through Magick" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
			);
			Magick::Image imageToEncode;

			imageToEncode.read(mmsAssetPathName.c_str());

			imageWidth = imageToEncode.columns();
			imageHeight = imageToEncode.rows();
			imageFormat = imageToEncode.magick();
			imageQuality = imageToEncode.quality();

			contentType = MMSEngineDBFacade::ContentType::Image;
		}
		catch (Magick::WarningCoder &e)
		{
			// Process coder warning while loading file (e.g. TIFF warning)
			// Maybe the user will be interested in these warnings (or not).
			// If a warning is produced while loading an image, the image
			// can normally still be used (but not if the warning was about
			// something important!)
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw runtime_error(e.what());
			// throw e;
		}
		catch (Magick::Warning &e)
		{
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw runtime_error(e.what());
			// throw e;
		}
		catch (Magick::ErrorFileOpen &e)
		{
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw runtime_error(e.what());
			// throw e;
		}
		catch (Magick::Error &e)
		{
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw runtime_error(e.what());
			// throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() + "ImageMagick failed to retrieve width and height failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
			);

			if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
			{
				try
				{
					SPDLOG_INFO(
						string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
					);

					fs::remove_all(mmsAssetPathName);
				}
				catch (runtime_error &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_INFO(
						string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
					);
				}
			}

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
				", errorMessage: " + e.what()
			);
			try
			{
				_mmsEngineDBFacade->updateIngestionJob(
					localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
				);
			}
			catch (runtime_error &re)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
				);
			}
			catch (exception &ex)
			{
				SPDLOG_INFO(
					string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
				);
			}

			throw e;
		}
	}
	else
	{
		string errorMessage = string("Unknown mediaFileFormat") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							  ", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
							  ", mmsAssetPathName: " + mmsAssetPathName;

		SPDLOG_ERROR(string() + errorMessage);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
			", errorMessage: " + errorMessage
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw runtime_error(errorMessage);
	}

	// int64_t mediaItemKey;
	try
	{
		unsigned long long sizeInBytes;
		if (mediaFileFormat == "m3u8-tar.gz")
		{
			sizeInBytes = 0;
			// recursive_directory_iterator, by default, does not follow sym
			// links
			for (fs::directory_entry const &entry : fs::recursive_directory_iterator(mmsAssetPathName))
			{
				if (entry.is_regular_file())
					sizeInBytes += entry.file_size();
			}
		}
		else
			sizeInBytes = fs::file_size(mmsAssetPathName);

		int64_t variantOfMediaItemKey = -1;
		{
			string variantOfMediaItemKeyField = "variantOfMediaItemKey";
			string variantOfUniqueNameField = "variantOfUniqueName";
			string variantOfIngestionJobKeyField = "VariantOfIngestionJobKey";
			if (JSONUtils::isMetadataPresent(parametersRoot, variantOfMediaItemKeyField))
			{
				variantOfMediaItemKey = JSONUtils::asInt64(parametersRoot, variantOfMediaItemKeyField, -1);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, variantOfUniqueNameField))
			{
				bool warningIfMissing = false;

				string variantOfUniqueName = JSONUtils::asString(parametersRoot, variantOfUniqueNameField, "");

				pair<int64_t, MMSEngineDBFacade::ContentType> mediaItemKeyDetails = _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
					localAssetIngestionEvent.getWorkspace()->_workspaceKey, variantOfUniqueName, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena
					// aggiunto
					true
				);
				tie(variantOfMediaItemKey, ignore) = mediaItemKeyDetails;
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, variantOfIngestionJobKeyField))
			{
				int64_t variantOfIngestionJobKey = JSONUtils::asInt64(parametersRoot, variantOfIngestionJobKeyField, -1);
				vector<tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType>> mediaItemsDetails;
				bool warningIfMissing = false;

				_mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
					localAssetIngestionEvent.getWorkspace()->_workspaceKey, variantOfIngestionJobKey, -1, mediaItemsDetails, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);

				if (mediaItemsDetails.size() != 1)
				{
					string errorMessage = string("IngestionJob does not refer the correct media "
												 "Items number") +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", variantOfIngestionJobKey: " + to_string(variantOfIngestionJobKey) +
										  ", workspaceKey: " + to_string(localAssetIngestionEvent.getWorkspace()->_workspaceKey) +
										  ", mediaItemsDetails.size(): " + to_string(mediaItemsDetails.size());
					SPDLOG_ERROR(string() + errorMessage);

					throw runtime_error(errorMessage);
				}

				tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType> mediaItemsDetailsReturn = mediaItemsDetails[0];
				tie(variantOfMediaItemKey, ignore, ignore) = mediaItemsDetailsReturn;
			}
		}

		// 2022-12-30: indipendentemente se si tratta di una variante o di un
		// source,
		//	è possibile indicare encodingProfileKey
		//	Ad esempio: se si esegue un Task OverlayText dove si specifica
		// l'encoding profile, 	il file generato e ingestato in MMS è un source
		// ed ha anche uno specifico profilo.
		int64_t encodingProfileKey = -1;
		{
			string field = "encodingProfileKey";
			encodingProfileKey = JSONUtils::asInt64(parametersRoot, field, -1);
		}

		if (variantOfMediaItemKey == -1)
		{
			SPDLOG_INFO(
				string() + "_mmsEngineDBFacade->saveSourceContentMetadata..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", encodingProfileKey: " + to_string(encodingProfileKey) + ", contentType: " + MMSEngineDBFacade::toString(contentType) +
				", ExternalReadOnlyStorage: " + to_string(localAssetIngestionEvent.getExternalReadOnlyStorage()) +
				", relativePathToBeUsed: " + relativePathToBeUsed + ", mediaSourceFileName: " + mediaSourceFileName +
				", mmsPartitionUsed: " + to_string(mmsPartitionUsed) + ", sizeInBytes: " + to_string(sizeInBytes)

				+ ", videoTracks.size: " + to_string(videoTracks.size()) + ", audioTracks.size: " + to_string(audioTracks.size())

				+ ", imageWidth: " + to_string(imageWidth) + ", imageHeight: " + to_string(imageHeight) + ", imageFormat: " + imageFormat +
				", imageQuality: " + to_string(imageQuality)
			);

			pair<int64_t, int64_t> mediaItemKeyAndPhysicalPathKey = _mmsEngineDBFacade->saveSourceContentMetadata(
				localAssetIngestionEvent.getWorkspace(), localAssetIngestionEvent.getIngestionJobKey(),
				localAssetIngestionEvent.getIngestionRowToBeUpdatedAsSuccess(), contentType, encodingProfileKey, parametersRoot,
				localAssetIngestionEvent.getExternalReadOnlyStorage(), relativePathToBeUsed, mediaSourceFileName, mmsPartitionUsed, sizeInBytes,

				// video-audio
				mediaInfoDetails, videoTracks, audioTracks,

				// image
				imageWidth, imageHeight, imageFormat, imageQuality
			);

			int64_t mediaItemKey = mediaItemKeyAndPhysicalPathKey.first;

			SPDLOG_INFO(
				string() + "Added a new ingested content" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
				to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mediaItemKey: " + to_string(mediaItemKeyAndPhysicalPathKey.first) +
				", physicalPathKey: " + to_string(mediaItemKeyAndPhysicalPathKey.second)
			);
		}
		else
		{
			string externalDeliveryTechnology;
			string externalDeliveryURL;
			{
				string field = "externalDeliveryTechnology";
				externalDeliveryTechnology = JSONUtils::asString(parametersRoot, field, "");

				field = "externalDeliveryURL";
				externalDeliveryURL = JSONUtils::asString(parametersRoot, field, "");
			}

			int64_t physicalItemRetentionInMinutes = -1;
			{
				string field = "physicalItemRetention";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string retention = JSONUtils::asString(parametersRoot, field, "1d");
					physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
				}
			}

			int64_t sourceIngestionJobKey = -1;
			// in case of an encoding generated by the External Transcoder,
			// we have to insert into MMS_IngestionJobOutput
			// of the ingestion job
			{
				string field = "userData";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					json userDataRoot = parametersRoot[field];

					field = "mmsData";
					if (JSONUtils::isMetadataPresent(userDataRoot, field))
					{
						json mmsDataRoot = userDataRoot[field];

						if (JSONUtils::isMetadataPresent(mmsDataRoot, "externalTranscoder"))
						{
							field = "ingestionJobKey";
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot, field, -1);
						}
					}
				}
			}

			SPDLOG_INFO(
				string() + "_mmsEngineDBFacade->saveVariantContentMetadata.." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", workspaceKey: " + to_string(localAssetIngestionEvent.getWorkspace()->_workspaceKey) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", sourceIngestionJobKey: " + to_string(sourceIngestionJobKey) + ", variantOfMediaItemKey: " + to_string(variantOfMediaItemKey) +
				", ExternalReadOnlyStorage: " + to_string(localAssetIngestionEvent.getExternalReadOnlyStorage()) +
				", externalDeliveryTechnology: " + externalDeliveryTechnology + ", externalDeliveryURL: " + externalDeliveryURL

				+ ", mediaSourceFileName: " + mediaSourceFileName + ", relativePathToBeUsed: " + relativePathToBeUsed + ", mmsPartitionUsed: " +
				to_string(mmsPartitionUsed) + ", sizeInBytes: " + to_string(sizeInBytes) + ", encodingProfileKey: " + to_string(encodingProfileKey) +
				", physicalItemRetentionInMinutes: " + to_string(physicalItemRetentionInMinutes)

				+ ", videoTracks.size: " + to_string(videoTracks.size()) + ", audioTracks.size: " + to_string(audioTracks.size())

				+ ", imageWidth: " + to_string(imageWidth) + ", imageHeight: " + to_string(imageHeight) + ", imageFormat: " + imageFormat +
				", imageQuality: " + to_string(imageQuality)
			);

			int64_t physicalPathKey = _mmsEngineDBFacade->saveVariantContentMetadata(
				localAssetIngestionEvent.getWorkspace()->_workspaceKey, localAssetIngestionEvent.getIngestionJobKey(), sourceIngestionJobKey,
				variantOfMediaItemKey, localAssetIngestionEvent.getExternalReadOnlyStorage(), externalDeliveryTechnology, externalDeliveryURL,

				mediaSourceFileName, relativePathToBeUsed, mmsPartitionUsed, sizeInBytes, encodingProfileKey, physicalItemRetentionInMinutes,

				// video-audio
				mediaInfoDetails, videoTracks, audioTracks,

				// image
				imageWidth, imageHeight, imageFormat, imageQuality
			);
			SPDLOG_INFO(
				string() + "Added a new variant content" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) +
				", variantOfMediaItemKey,: " + to_string(variantOfMediaItemKey) + ", physicalPathKey: " + to_string(physicalPathKey)
			);

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
				to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_TaskSuccess" + ", errorMessage: " + ""
			);
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
				"" // errorMessage
			);
		}
	}
	catch (DeadlockFound &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey failed" + ", _processorIdentifier: " +
			to_string(_processorIdentifier) + ", workspaceKey: " + to_string(localAssetIngestionEvent.getWorkspace()->_workspaceKey) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what: " + e.what()
		);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw runtime_error(e.what());
	}
	catch (MediaItemKeyNotFound &e) // getMediaItemDetailsByIngestionJobKey failure
	{
		SPDLOG_ERROR(
			string() + "_mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey failed" + ", _processorIdentifier: " +
			to_string(_processorIdentifier) + ", workspaceKey: " + to_string(localAssetIngestionEvent.getWorkspace()->_workspaceKey) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what: " + e.what()
		);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw runtime_error(e.what());
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsEngineDBFacade->saveSourceContentMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", e.what: " + e.what()
		);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "_mmsEngineDBFacade->saveSourceContentMetadata failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey())
		);

		if (!localAssetIngestionEvent.getExternalReadOnlyStorage())
		{
			try
			{
				SPDLOG_INFO(
					string() + "Remove" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", mmsAssetPathName: " + mmsAssetPathName
				);

				fs::remove_all(mmsAssetPathName);
			}
			catch (runtime_error &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_INFO(
					string() + "remove failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + e.what()
				);
			}
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				localAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(localAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		throw e;
	}
}

void MMSEngineProcessor::handleMultiLocalAssetIngestionEventThread(
	shared_ptr<long> processorsThreadsNumber, MultiLocalAssetIngestionEvent multiLocalAssetIngestionEvent
)
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleMultiLocalAssetIngestionEventThread", _processorIdentifier, _processorsThreadsNumber.use_count(),
		multiLocalAssetIngestionEvent.getIngestionJobKey()
	);

	string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(multiLocalAssetIngestionEvent.getWorkspace());
	vector<string> generatedFramesFileNames;

	try
	{
		SPDLOG_INFO(
			string() + "handleMultiLocalAssetIngestionEventThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
			", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		// get files from file system
		{
			string generatedFrames_BaseFileName = to_string(multiLocalAssetIngestionEvent.getIngestionJobKey());

			for (fs::directory_entry const &entry : fs::directory_iterator(workspaceIngestionRepository))
			{
				try
				{
					if (!entry.is_regular_file())
						continue;

					if (entry.path().filename().string().size() >= generatedFrames_BaseFileName.size() &&
						0 == entry.path().filename().string().compare(0, generatedFrames_BaseFileName.size(), generatedFrames_BaseFileName))
						generatedFramesFileNames.push_back(entry.path().filename().string());
				}
				catch (runtime_error &e)
				{
					string errorMessage = string() + "listing directory failed" + ", e.what(): " + e.what();
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
				catch (exception &e)
				{
					string errorMessage = string() + "listing directory failed" + ", e.what(): " + e.what();
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
			}
		}

		// we have one ingestion job row and one or more generated frames to be
		// ingested One MIK in case of a .mjpeg One or more MIKs in case of .jpg
		// We want to update the ingestion row just once at the end,
		// in case of success or when an error happens.
		// To do this we will add a field in the localAssetIngestionEvent
		// structure (ingestionRowToBeUpdatedAsSuccess) and we will set it to
		// false except for the last frame where we will set to true In case of
		// error, handleLocalAssetIngestionEvent will update ingestion row and
		// we will not call anymore handleLocalAssetIngestionEvent for the next
		// frames When I say 'update the ingestion row', it's not just the
		// update but it is also manageIngestionJobStatusUpdate
		bool generatedFrameIngestionFailed = false;

		for (vector<string>::iterator it = generatedFramesFileNames.begin(); it != generatedFramesFileNames.end(); ++it)
		{
			string generatedFrameFileName = *it;

			if (generatedFrameIngestionFailed)
			{
				string workspaceIngestionBinaryPathName = workspaceIngestionRepository + "/" + generatedFrameFileName;

				SPDLOG_INFO(
					string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
					", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
				);
				fs::remove_all(workspaceIngestionBinaryPathName);
			}
			else
			{
				SPDLOG_INFO(
					string() + "Generated Frame to ingest" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
					to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", generatedFrameFileName: " + generatedFrameFileName
					// + ", textToBeReplaced: " + textToBeReplaced
					// + ", textToReplace: " + textToReplace
				);

				string fileFormat;
				size_t extensionIndex = generatedFrameFileName.find_last_of(".");
				if (extensionIndex == string::npos)
				{
					string errorMessage = string() +
										  "No fileFormat (extension of the file) found in "
										  "generatedFileName" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) +
										  ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
										  ", generatedFrameFileName: " + generatedFrameFileName;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				fileFormat = generatedFrameFileName.substr(extensionIndex + 1);

				//            if (mmsSourceFileName.find(textToBeReplaced) !=
				//            string::npos)
				//                mmsSourceFileName.replace(mmsSourceFileName.find(textToBeReplaced),
				//                textToBeReplaced.length(), textToReplace);

				SPDLOG_INFO(
					string() + "Generated Frame to ingest" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
					", new generatedFrameFileName: " + generatedFrameFileName + ", fileFormat: " + fileFormat
				);

				string title;
				{
					string field = "title";
					if (JSONUtils::isMetadataPresent(multiLocalAssetIngestionEvent.getParametersRoot(), field))
						title = JSONUtils::asString(multiLocalAssetIngestionEvent.getParametersRoot(), field, "");
					title += (" (" + to_string(it - generatedFramesFileNames.begin() + 1) + " / " + to_string(generatedFramesFileNames.size()) + ")");
				}
				int64_t imageOfVideoMediaItemKey = -1;
				int64_t cutOfVideoMediaItemKey = -1;
				int64_t cutOfAudioMediaItemKey = -1;
				double startTimeInSeconds = 0.0;
				double endTimeInSeconds = 0.0;
				string imageMetaDataContent = generateMediaMetadataToIngest(
					multiLocalAssetIngestionEvent.getIngestionJobKey(),
					// mjpeg,
					fileFormat, title, imageOfVideoMediaItemKey, cutOfVideoMediaItemKey, cutOfAudioMediaItemKey, startTimeInSeconds, endTimeInSeconds,
					multiLocalAssetIngestionEvent.getParametersRoot()
				);

				{
					// shared_ptr<LocalAssetIngestionEvent>
					// localAssetIngestionEvent =
					// _multiEventsSet->getEventsFactory()
					//        ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);
					shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent = make_shared<LocalAssetIngestionEvent>();

					localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
					localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
					localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

					localAssetIngestionEvent->setExternalReadOnlyStorage(false);
					localAssetIngestionEvent->setIngestionJobKey(multiLocalAssetIngestionEvent.getIngestionJobKey());
					localAssetIngestionEvent->setIngestionSourceFileName(generatedFrameFileName);
					localAssetIngestionEvent->setMMSSourceFileName(generatedFrameFileName);
					localAssetIngestionEvent->setWorkspace(multiLocalAssetIngestionEvent.getWorkspace());
					localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
					localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(it + 1 == generatedFramesFileNames.end() ? true : false);

					localAssetIngestionEvent->setMetadataContent(imageMetaDataContent);

					try
					{
						handleLocalAssetIngestionEvent(processorsThreadsNumber, *localAssetIngestionEvent);
					}
					catch (runtime_error &e)
					{
						generatedFrameIngestionFailed = true;

						SPDLOG_ERROR(
							string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", exception: " + e.what()
						);
					}
					catch (exception &e)
					{
						generatedFrameIngestionFailed = true;

						SPDLOG_ERROR(
							string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", exception: " + e.what()
						);
					}

					//                    shared_ptr<Event2>    event =
					//                    dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
					//                    _multiEventsSet->addEvent(event);
					//
					//                    SPDLOG_INFO(string() + "addEvent:
					//                    EVENT_TYPE (INGESTASSETEVENT)"
					//                        + ", _processorIdentifier: " +
					//                        to_string(_processorIdentifier)
					//                        + ", ingestionJobKey: " +
					//                        to_string(ingestionJobKey)
					//                        + ", getEventKey().first: " +
					//                        to_string(event->getEventKey().first)
					//                        + ", getEventKey().second: " +
					//                        to_string(event->getEventKey().second));
				}
			}
		}

		/*
		if (generatedFrameIngestionFailed)
		{
			SPDLOG_INFO(string() + "updater->updateEncodingJob
		PunctualError"
				+ ", _encodingItem->_encodingJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getEncodingJobKey())
				+ ", _encodingItem->_ingestionJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
			);

			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			// PunctualError is used because, in case it always happens, the
		encoding will never reach a final state int encodingFailureNumber =
		updater->updateEncodingJob (
					multiLocalAssetIngestionEvent->getEncodingJobKey(),
					MMSEngineDBFacade::EncodingError::PunctualError,    //
		ErrorBeforeEncoding, mediaItemKey, encodedPhysicalPathKey,
					multiLocalAssetIngestionEvent->getIngestionJobKey());

			SPDLOG_INFO(string() + "updater->updateEncodingJob
		PunctualError"
				+ ", _encodingItem->_encodingJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getEncodingJobKey())
				+ ", _encodingItem->_ingestionJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
				+ ", encodingFailureNumber: " + to_string(encodingFailureNumber)
			);
		}
		else
		{
			SPDLOG_INFO(string() + "updater->updateEncodingJob NoError"
				+ ", _encodingItem->_encodingJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getEncodingJobKey())
				+ ", _encodingItem->_ingestionJobKey: " +
		to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
			);

			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			updater->updateEncodingJob (
				multiLocalAssetIngestionEvent->getEncodingJobKey(),
				MMSEngineDBFacade::EncodingError::NoError,
				mediaItemKey, encodedPhysicalPathKey,
				multiLocalAssetIngestionEvent->getIngestionJobKey());
		}
		*/
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "handleMultiLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
			", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				multiLocalAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		for (vector<string>::iterator it = generatedFramesFileNames.begin(); it != generatedFramesFileNames.end(); ++it)
		{
			string workspaceIngestionBinaryPathName = workspaceIngestionRepository + "/" + *it;

			SPDLOG_INFO(
				string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
				", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
			);
			fs::remove_all(workspaceIngestionBinaryPathName);
		}

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "handleMultiLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey())
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", IngestionStatus: " + "End_IngestionFailure" +
			", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(
				multiLocalAssetIngestionEvent.getIngestionJobKey(), MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
			);
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) + ", errorMessage: " + ex.what()
			);
		}

		for (vector<string>::iterator it = generatedFramesFileNames.begin(); it != generatedFramesFileNames.end(); ++it)
		{
			string workspaceIngestionBinaryPathName = workspaceIngestionRepository + "/" + *it;

			SPDLOG_INFO(
				string() + "Remove file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent.getIngestionJobKey()) +
				", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
			);
			fs::remove_all(workspaceIngestionBinaryPathName);
		}

		throw e;
	}
}

tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int, bool> MMSEngineProcessor::getMediaSourceDetails(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot
)
{
	// only in case of externalReadOnlyStorage, nextIngestionStatus does not
	// change and we do not need it So I set it just to a state
	MMSEngineDBFacade::IngestionStatus nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::Start_TaskQueued;
	string mediaSourceURL;
	string mediaFileFormat;
	bool externalReadOnlyStorage;

	string field;
	if (ingestionType != MMSEngineDBFacade::IngestionType::AddContent)
	{
		string errorMessage = string() + "ingestionType is wrong" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	externalReadOnlyStorage = false;
	{
		field = "sourceURL";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
			mediaSourceURL = JSONUtils::asString(parametersRoot, field, "");

		field = "fileFormat";
		mediaFileFormat = JSONUtils::asString(parametersRoot, field, "");

		// string httpPrefix("http://");
		// string httpsPrefix("https://");
		// // string ftpPrefix("ftp://");
		// string ftpsPrefix("ftps://");
		// string movePrefix("move://"); // move:///dir1/dir2/.../file
		// string mvPrefix("mv://");
		// string copyPrefix("copy://");
		// string cpPrefix("cp://");
		// string externalStoragePrefix("externalStorage://");
		// if ((mediaSourceURL.size() >= httpPrefix.size() && 0 == mediaSourceURL.compare(0, httpPrefix.size(), httpPrefix)) ||
		// 	(mediaSourceURL.size() >= httpsPrefix.size() && 0 == mediaSourceURL.compare(0, httpsPrefix.size(), httpsPrefix)) ||
		// 	(mediaSourceURL.size() >= ftpPrefix.size() && 0 == mediaSourceURL.compare(0, ftpPrefix.size(), ftpPrefix)) ||
		// 	(mediaSourceURL.size() >= ftpsPrefix.size() && 0 == mediaSourceURL.compare(0, ftpsPrefix.size(), ftpsPrefix)))
		if (mediaSourceURL.starts_with("http://") || mediaSourceURL.starts_with("https://") || mediaSourceURL.starts_with("ftp://") ||
			mediaSourceURL.starts_with("ftps://"))
		{
			nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress;
		}
		else if (mediaSourceURL.starts_with("move://") || mediaSourceURL.starts_with("mv://"))
		{
			// else if ((mediaSourceURL.size() >= movePrefix.size() && 0 == mediaSourceURL.compare(0, movePrefix.size(), movePrefix)) ||
			// 	 (mediaSourceURL.size() >= mvPrefix.size() && 0 == mediaSourceURL.compare(0, mvPrefix.size(), mvPrefix)))
			nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress;
		}
		else if (mediaSourceURL.starts_with("copy://") || mediaSourceURL.starts_with("cp://"))
		// else if ((mediaSourceURL.size() >= copyPrefix.size() && 0 == mediaSourceURL.compare(0, copyPrefix.size(), copyPrefix)) ||
		// 	 (mediaSourceURL.size() >= cpPrefix.size() && 0 == mediaSourceURL.compare(0, cpPrefix.size(), cpPrefix)))
		{
			nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress;
		}
		else if (mediaSourceURL.starts_with("externalStorage://"))
		// else if (mediaSourceURL.size() >= externalStoragePrefix.size() &&
		// 	 0 == mediaSourceURL.compare(0, externalStoragePrefix.size(), externalStoragePrefix))
		{
			externalReadOnlyStorage = true;
		}
		else
		{
			nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress;
		}
	}

	string md5FileCheckSum;
	field = "MD5FileCheckSum";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		// MD5         md5;
		// char        md5RealDigest [32 + 1];

		md5FileCheckSum = JSONUtils::asString(parametersRoot, field, "");
	}

	int fileSizeInBytes = -1;
	field = "FileSizeInBytes";
	fileSizeInBytes = JSONUtils::asInt(parametersRoot, field, -1);

	/*
	tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int>
	mediaSourceDetails; get<0>(mediaSourceDetails) = nextIngestionStatus;
	get<1>(mediaSourceDetails) = mediaSourceURL;
	get<2>(mediaSourceDetails) = mediaFileFormat;
	get<3>(mediaSourceDetails) = md5FileCheckSum;
	get<4>(mediaSourceDetails) = fileSizeInBytes;
	*/

	SPDLOG_INFO(
		string() + "media source details" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", ingestionJobKey: " + to_string(ingestionJobKey) + ", nextIngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus) +
		", mediaSourceURL: " + mediaSourceURL + ", mediaFileFormat: " + mediaFileFormat + ", md5FileCheckSum: " + md5FileCheckSum +
		", fileSizeInBytes: " + to_string(fileSizeInBytes) + ", externalReadOnlyStorage: " + to_string(externalReadOnlyStorage)
	);

	return make_tuple(nextIngestionStatus, mediaSourceURL, mediaFileFormat, md5FileCheckSum, fileSizeInBytes, externalReadOnlyStorage);
}

void MMSEngineProcessor::validateMediaSourceFile(
	int64_t ingestionJobKey, string mediaSourcePathName, string mediaFileFormat, string md5FileCheckSum, int fileSizeInBytes
)
{

	if (mediaFileFormat == "m3u8-tar.gz")
	{
		// in this case it is a directory with segments inside
		bool dirExists = false;
		{
			chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
			do
			{
				if (fs::exists(mediaSourcePathName))
				{
					dirExists = true;
					break;
				}

				this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));
			} while (chrono::system_clock::now() < end);
		}

		if (!dirExists)
		{
			string errorMessage = string() +
								  "Media Source directory does not exist (it was not uploaded "
								  "yet)" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", mediaSourcePathName: " + mediaSourcePathName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else
	{
		// we added the following two parameters for the FileIO::fileExisting
		// method because, in the scenario where still MMS generates the file to
		// be ingested (i.e.: generate frames task and other tasks), and the NFS
		// is used, we saw sometimes FileIO::fileExisting returns false even if
		// the file is there. This is due because of NFS delay to present the
		// file
		bool fileExists = false;
		{
			chrono::system_clock::time_point end = chrono::system_clock::now() + chrono::milliseconds(_waitingNFSSync_maxMillisecondsToWait);
			do
			{
				if (fs::exists(mediaSourcePathName))
				{
					fileExists = true;
					break;
				}

				this_thread::sleep_for(chrono::milliseconds(_waitingNFSSync_milliSecondsWaitingBetweenChecks));
			} while (chrono::system_clock::now() < end);
		}
		if (!fileExists)
		{
			string errorMessage = string() + "Media Source file does not exist (it was not uploaded yet)" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", mediaSourcePathName: " + mediaSourcePathName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	// we just simplify and md5FileCheck is not done in case of segments
	if (mediaFileFormat != "m3u8-tar.gz" && md5FileCheckSum != "")
	{
		EVP_MD_CTX *ctx = EVP_MD_CTX_create();
		const EVP_MD *mdType = EVP_md5();
		EVP_MD_CTX_init(ctx);
		EVP_DigestInit_ex(ctx, mdType, nullptr);

		std::ifstream ifs;
		ifs.open(mediaSourcePathName, ios::binary | ios::in);
		if (!ifs.good())
		{
			string errorMessage = string() + "Media files to be opened" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSourcePathName: " + mediaSourcePathName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		char data[MD5BUFFERSIZE];
		size_t size;
		do
		{
			ifs.read(data, MD5BUFFERSIZE);
			size = ifs.gcount();
			for (size_t bytes_done = 0; bytes_done < size;)
			{
				size_t bytes = 4096;
				auto missing = size - bytes_done;

				if (missing < bytes)
					bytes = missing;

				auto dataStart = static_cast<void *>(static_cast<char *>(data) + bytes_done);
				EVP_DigestUpdate(ctx, dataStart, bytes);
				bytes_done += bytes;
			}
		} while (ifs.good());
		ifs.close();

		std::vector<unsigned char> md(EVP_MD_size(mdType));
		EVP_DigestFinal_ex(ctx, md.data(), nullptr);
		EVP_MD_CTX_destroy(ctx);

		std::ostringstream hashBuffer;

		for (auto c : md)
			hashBuffer << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(c);

		string md5RealDigest = hashBuffer.str();

		bool isCaseInsensitiveEqual =
			md5FileCheckSum.length() != md5RealDigest.length()
				? false
				: equal(
					  md5FileCheckSum.begin(), md5FileCheckSum.end(), md5RealDigest.begin(), [](int c1, int c2) { return toupper(c1) == toupper(c2); }
				  );

		if (!isCaseInsensitiveEqual)
		{
			string errorMessage = string() + "MD5 check failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSourcePathName: " + mediaSourcePathName +
								  ", md5FileCheckSum: " + md5FileCheckSum + ", md5RealDigest: " + md5RealDigest;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	// we just simplify and file size check is not done in case of segments
	if (mediaFileFormat != "m3u8-tar.gz" && fileSizeInBytes != -1)
	{
		unsigned long downloadedFileSizeInBytes = fs::file_size(mediaSourcePathName);

		if (fileSizeInBytes != downloadedFileSizeInBytes)
		{
			string errorMessage = string() + "FileSize check failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaSourcePathName: " + mediaSourcePathName +
								  ", metadataFileSizeInBytes: " + to_string(fileSizeInBytes) +
								  ", downloadedFileSizeInBytes: " + to_string(downloadedFileSizeInBytes);
			SPDLOG_ERROR(errorMessage);
			throw runtime_error(errorMessage);
		}
	}
}

int MMSEngineProcessor::progressDownloadCallback(
	int64_t ingestionJobKey, chrono::system_clock::time_point &lastTimeProgressUpdate, double &lastPercentageUpdated, bool &downloadingStoppedByUser,
	double dltotal, double dlnow, double ultotal, double ulnow
)
{

	chrono::system_clock::time_point now = chrono::system_clock::now();

	if (dltotal != 0 && (dltotal == dlnow || now - lastTimeProgressUpdate >= chrono::seconds(_progressUpdatePeriodInSeconds)))
	{
		double progress = (dlnow / dltotal) * 100;
		// int downloadingPercentage = floorf(progress * 100) / 100;
		// this is to have one decimal in the percentage
		double downloadingPercentage = ((double)((int)(progress * 10))) / 10;

		SPDLOG_INFO(
			"Download still running"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", downloadingPercentage: {}"
			", lastPercentageUpdated: {}"
			", downloadingStoppedByUser: {}"
			", dltotal: {}"
			", dlnow: {}"
			", ultotal: {}"
			", ulnow: {}",
			_processorIdentifier, ingestionJobKey, downloadingPercentage, lastPercentageUpdated, downloadingStoppedByUser, dltotal, dlnow, ultotal,
			ulnow
		);

		lastTimeProgressUpdate = now;

		if (lastPercentageUpdated != downloadingPercentage)
		{
			SPDLOG_INFO(
				"Update IngestionJob"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", downloadingPercentage: {}",
				_processorIdentifier, ingestionJobKey, downloadingPercentage
			);
			downloadingStoppedByUser = _mmsEngineDBFacade->updateIngestionJobSourceDownloadingInProgress(ingestionJobKey, downloadingPercentage);

			lastPercentageUpdated = downloadingPercentage;
		}

		if (downloadingStoppedByUser)
		{
			SPDLOG_INFO(
				"Download canceled by user"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", downloadingPercentage: {}"
				", downloadingStoppedByUser: {}",
				_processorIdentifier, ingestionJobKey, downloadingPercentage, downloadingStoppedByUser
			);

			return 1; // stop downloading
		}
	}

	return 0;
}

size_t curlDownloadCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
	chrono::system_clock::time_point start = chrono::system_clock::now();

	MMSEngineProcessor::CurlDownloadData *curlDownloadData = (MMSEngineProcessor::CurlDownloadData *)f;

	auto logger = spdlog::get("mmsEngineService");

	if (curlDownloadData->currentChunkNumber == 0)
	{
		(curlDownloadData->mediaSourceFileStream).open(curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::trunc);
		curlDownloadData->currentChunkNumber += 1;

		SPDLOG_INFO(
			"Opening binary file"
			", curlDownloadData -> ingestionJobKey: {}"
			", curlDownloadData -> destBinaryPathName: {}"
			", curlDownloadData->currentChunkNumber: {}"
			", curlDownloadData->currentTotalSize: {}"
			", curlDownloadData->maxChunkFileSize: {}",
			curlDownloadData->ingestionJobKey, curlDownloadData->destBinaryPathName, curlDownloadData->currentChunkNumber,
			curlDownloadData->currentTotalSize, curlDownloadData->maxChunkFileSize
		);
	}
	else if (curlDownloadData->currentTotalSize >= curlDownloadData->currentChunkNumber * curlDownloadData->maxChunkFileSize)
	{
		(curlDownloadData->mediaSourceFileStream).close();

		(curlDownloadData->mediaSourceFileStream).open(curlDownloadData->destBinaryPathName, ofstream::binary | ofstream::app);
		curlDownloadData->currentChunkNumber += 1;

		SPDLOG_INFO(
			"Opening binary file"
			", curlDownloadData -> ingestionJobKey: {}"
			", curlDownloadData -> destBinaryPathName: {}"
			", curlDownloadData->currentChunkNumber: {}"
			", curlDownloadData->currentTotalSize: {}"
			", curlDownloadData->maxChunkFileSize: {}"
			", tellp: {}",
			curlDownloadData->ingestionJobKey, curlDownloadData->destBinaryPathName, curlDownloadData->currentChunkNumber,
			curlDownloadData->currentTotalSize, curlDownloadData->maxChunkFileSize, (curlDownloadData->mediaSourceFileStream).tellp()
		);
	}

	curlDownloadData->mediaSourceFileStream.write(ptr, size * nmemb);
	curlDownloadData->currentTotalSize += (size * nmemb);

	// debug perchè avremmo tantissimi log con elapsed 0
	SPDLOG_DEBUG(
		"curlDownloadCallback"
		", ingestionJobKey: {}"
		", bytes written: {}"
		", elapsed (millisecs): {}",
		curlDownloadData->ingestionJobKey, size * nmemb, chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - start).count()
	);

	return size * nmemb;
};

void MMSEngineProcessor::downloadMediaSourceFileThread(
	shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL, bool regenerateTimestamps, int m3u8TarGzOrStreaming, int64_t ingestionJobKey,
	shared_ptr<Workspace> workspace
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "downloadMediaSourceFileThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	bool downloadingCompleted = false;

	SPDLOG_INFO(
		"downloadMediaSourceFileThread"
		", _processorIdentifier: {}"
		", ingestionJobKey: {}"
		", m3u8TarGzOrStreaming: {}"
		", _processorsThreadsNumber.use_count(): {}",
		_processorIdentifier, ingestionJobKey, m3u8TarGzOrStreaming, _processorsThreadsNumber.use_count()
	);
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

	string localSourceReferenceURL = sourceReferenceURL;
	int localM3u8TarGzOrStreaming = m3u8TarGzOrStreaming;
	// in case of youtube url, the real URL to be used has to be calcolated
	{
		if (sourceReferenceURL.starts_with("https://www.youtube.com/") || sourceReferenceURL.starts_with("https://youtu.be/"))
		// string youTubePrefix1("https://www.youtube.com/");
		// string youTubePrefix2("https://youtu.be/");
		// if ((sourceReferenceURL.size() >= youTubePrefix1.size() && 0 == sourceReferenceURL.compare(0, youTubePrefix1.size(), youTubePrefix1)) ||
		// 	(sourceReferenceURL.size() >= youTubePrefix2.size() && 0 == sourceReferenceURL.compare(0, youTubePrefix2.size(), youTubePrefix2)))
		{
			try
			{
				FFMpeg ffmpeg(_configurationRoot, _logger);
				pair<string, string> streamingURLDetails = ffmpeg.retrieveStreamingYouTubeURL(ingestionJobKey, sourceReferenceURL);

				string streamingYouTubeURL;
				tie(streamingYouTubeURL, ignore) = streamingURLDetails;

				SPDLOG_INFO(
					string() + "downloadMediaSourceFileThread. YouTube URL calculation" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", _ingestionJobKey: " + to_string(ingestionJobKey) +
					", initial YouTube URL: " + sourceReferenceURL + ", streaming YouTube URL: " + streamingYouTubeURL
				);

				localSourceReferenceURL = streamingYouTubeURL;

				// for sure localM3u8TarGzOrStreaming has to be false
				localM3u8TarGzOrStreaming = 0;
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "ffmpeg.retrieveStreamingYouTubeURL failed" + ", may be the YouTube URL is not available anymore" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", YouTube URL: " + sourceReferenceURL +
									  ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				SPDLOG_INFO(
					string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
					to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage
				);
				try
				{
					// to hide ffmpeg staff
					errorMessage = string() + "retrieveStreamingYouTubeURL failed" + ", may be the YouTube URL is not available anymore" +
								   ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								   ", YouTube URL: " + sourceReferenceURL + ", e.what(): " + e.what();
					_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage);
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
	}

	string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
	string destBinaryPathName = workspaceIngestionRepository + "/" + to_string(ingestionJobKey) + "_source";
	// 0: no m3u8
	// 1: m3u8 by .tar.gz
	// 2: m3u8 by streaming (it will be saved as .mp4)
	// .mp4 is used in
	// 1. downloadMediaSourceFileThread (when the streaming-to-mp4 is downloaded
	// in a .mp4 file
	// 2. handleLocalAssetIngestionEvent (when the IngestionRepository file name
	//		is built "consistent" with the above step no. 1)
	// 3. here, handleLocalAssetIngestionEvent (when the MMS file name is
	// generated)
	if (localM3u8TarGzOrStreaming == 1)
		destBinaryPathName = destBinaryPathName + ".tar.gz";
	else if (localM3u8TarGzOrStreaming == 2)
		destBinaryPathName = destBinaryPathName + ".mp4";

	if (localM3u8TarGzOrStreaming == 2)
	{
		try
		{
			SPDLOG_INFO(
				string() + "ffmpeg.streamingToFile" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
				to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL + ", destBinaryPathName: " + destBinaryPathName
			);

			// regenerateTimestamps (see
			// docs/TASK_01_Add_Content_JSON_Format.txt)
			FFMpeg ffmpeg(_configurationRoot, _logger);
			ffmpeg.streamingToFile(ingestionJobKey, regenerateTimestamps, sourceReferenceURL, destBinaryPathName);

			downloadingCompleted = true;

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", destBinaryPathName: " + destBinaryPathName +
				", downloadingCompleted: " + to_string(downloadingCompleted)
			);
			_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, downloadingCompleted);
		}
		catch (runtime_error &e)
		{
			string errorMessage = string() + "ffmpeg.streamingToFile failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL +
								  ", destBinaryPathName: " + destBinaryPathName + ", e.what(): " + e.what();
			SPDLOG_ERROR(errorMessage);

			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
				to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + errorMessage
			);
			try
			{
				// to hide ffmpeg staff
				errorMessage = string() + "streamingToFile failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							   ", ingestionJobKey: " + to_string(ingestionJobKey) + ", sourceReferenceURL: " + sourceReferenceURL +
							   ", destBinaryPathName: " + destBinaryPathName + ", e.what(): " + e.what();
				_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage);
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
	else
	{
		for (int attemptIndex = 0; attemptIndex < _maxDownloadAttemptNumber && !downloadingCompleted; attemptIndex++)
		{
			// 2023-12-20: questa variabile viene inizializzata a true nel
			// metodo progressDownloadCallback
			//	nel caso l'utente cancelli il download. Ci sono due problemi:
			//	1. l'utente non puo cancellare il download perchè attualmente la
			// GUI permette il kill/cancel 		in caso di encodingJob e, nel
			// Download, non abbiamo alcun encodingJob
			//	2. anche se questa variabile viene passata come 'reference' nel
			// metodo progressDownloadCallback 		il suo valore modificato non
			// ritorna qui, per cui la gestione di downloadingStoppedByUser =
			// true 		nella eccezione sotto non funziona
			bool downloadingStoppedByUser = false;

			try
			{
				SPDLOG_INFO(
					"Downloading"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", localSourceReferenceURL: {}"
					", destBinaryPathName: {}"
					", attempt: {}"
					", _maxDownloadAttemptNumber: {}",
					_processorIdentifier, ingestionJobKey, localSourceReferenceURL, destBinaryPathName, attemptIndex + 1, _maxDownloadAttemptNumber
				);

				if (attemptIndex == 0)
				{
					CurlDownloadData curlDownloadData;
					curlDownloadData.ingestionJobKey = ingestionJobKey;
					curlDownloadData.currentChunkNumber = 0;
					curlDownloadData.currentTotalSize = 0;
					curlDownloadData.destBinaryPathName = destBinaryPathName;
					curlDownloadData.maxChunkFileSize = _downloadChunkSizeInMegaBytes * 1000000;

					// fstream mediaSourceFileStream(destBinaryPathName,
					// ios::binary | ios::out);
					// mediaSourceFileStream.exceptions(ios::badbit |
					// ios::failbit);   // setting the exception mask FILE
					// *mediaSourceFileStream =
					// fopen(destBinaryPathName.c_str(), "wb");

					curlpp::Cleanup cleaner;
					curlpp::Easy request;

					// Set the writer callback to enable cURL
					// to write result in a memory area
					// request.setOpt(new
					// curlpp::options::WriteStream(&mediaSourceFileStream));

					// which timeout we have to use here???
					// request.setOpt(new
					// curlpp::options::Timeout(curlTimeoutInSeconds));

					curlpp::options::WriteFunctionCurlFunction curlDownloadCallbackFunction(curlDownloadCallback);
					curlpp::OptionTrait<void *, CURLOPT_WRITEDATA> curlDownloadDataData(&curlDownloadData);
					request.setOpt(curlDownloadCallbackFunction);
					request.setOpt(curlDownloadDataData);

					// localSourceReferenceURL:
					// ftp://user:password@host:port/path nel caso in cui
					// 'password' contenga '@', questo deve essere encodato con
					// %40
					request.setOpt(new curlpp::options::Url(localSourceReferenceURL));
					if (localSourceReferenceURL.starts_with("https"))
					// string httpsPrefix("https");
					// if (localSourceReferenceURL.size() >= httpsPrefix.size() &&
					// 	0 == localSourceReferenceURL.compare(0, httpsPrefix.size(), httpsPrefix))
					{
						// disconnect if we can't validate server's cert
						bool bSslVerifyPeer = false;
						curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
						request.setOpt(sslVerifyPeer);

						curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
						request.setOpt(sslVerifyHost);
					}

					chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
					double lastPercentageUpdated = -1.0;
					curlpp::types::ProgressFunctionFunctor functor = bind(
						&MMSEngineProcessor::progressDownloadCallback, this, ingestionJobKey, lastProgressUpdate, lastPercentageUpdated,
						downloadingStoppedByUser, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4
					);
					request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
					request.setOpt(new curlpp::options::NoProgress(0L));

					SPDLOG_INFO(
						string() + "Downloading media file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", localSourceReferenceURL: " + localSourceReferenceURL
					);
					request.perform();

					(curlDownloadData.mediaSourceFileStream).close();
				}
				else
				{
					_logger->warn(
						string() + "Coming from a download failure, trying to Resume" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey)
					);

					// FILE *mediaSourceFileStream =
					// fopen(destBinaryPathName.c_str(), "wb+");
					long long fileSize;
					{
						ofstream mediaSourceFileStream(destBinaryPathName, ofstream::binary | ofstream::app);
						fileSize = mediaSourceFileStream.tellp();
						mediaSourceFileStream.close();
					}

					CurlDownloadData curlDownloadData;
					curlDownloadData.ingestionJobKey = ingestionJobKey;
					curlDownloadData.destBinaryPathName = destBinaryPathName;
					curlDownloadData.maxChunkFileSize = _downloadChunkSizeInMegaBytes * 1000000;

					curlDownloadData.currentChunkNumber = fileSize % curlDownloadData.maxChunkFileSize;
					// fileSize = curlDownloadData.currentChunkNumber *
					// curlDownloadData.maxChunkFileSize;
					curlDownloadData.currentTotalSize = fileSize;

					SPDLOG_INFO(
						"Coming from a download failure, trying to Resume"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", destBinaryPathName: {}"
						", curlDownloadData.currentTotalSize/fileSize: {}"
						", curlDownloadData.currentChunkNumber: {}",
						_processorIdentifier, ingestionJobKey, destBinaryPathName, fileSize, curlDownloadData.currentChunkNumber
					);

					curlpp::Cleanup cleaner;
					curlpp::Easy request;

					// Set the writer callback to enable cURL
					// to write result in a memory area
					// request.setOpt(new
					// curlpp::options::WriteStream(&mediaSourceFileStream));

					curlpp::options::WriteFunctionCurlFunction curlDownloadCallbackFunction(curlDownloadCallback);
					curlpp::OptionTrait<void *, CURLOPT_WRITEDATA> curlDownloadDataData(&curlDownloadData);
					request.setOpt(curlDownloadCallbackFunction);
					request.setOpt(curlDownloadDataData);

					// which timeout we have to use here???
					// request.setOpt(new
					// curlpp::options::Timeout(curlTimeoutInSeconds));

					// Setting the URL to retrive.
					request.setOpt(new curlpp::options::Url(localSourceReferenceURL));
					// string httpsPrefix("https");
					// if (localSourceReferenceURL.size() >= httpsPrefix.size() &&
					// 	0 == localSourceReferenceURL.compare(0, httpsPrefix.size(), httpsPrefix))
					if (localSourceReferenceURL.starts_with("https"))
					{
						SPDLOG_INFO(string() + "Setting SslEngineDefault" + ", _processorIdentifier: " + to_string(_processorIdentifier));
						request.setOpt(new curlpp::options::SslEngineDefault());
					}

					chrono::system_clock::time_point lastTimeProgressUpdate = chrono::system_clock::now();
					double lastPercentageUpdated = -1.0;
					curlpp::types::ProgressFunctionFunctor functor = bind(
						&MMSEngineProcessor::progressDownloadCallback, this, ingestionJobKey, lastTimeProgressUpdate, lastPercentageUpdated,
						downloadingStoppedByUser, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4
					);
					request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
					request.setOpt(new curlpp::options::NoProgress(0L));

					if (fileSize > 2 * 1000 * 1000 * 1000)
						request.setOpt(new curlpp::options::ResumeFromLarge(fileSize));
					else
						request.setOpt(new curlpp::options::ResumeFrom(fileSize));

					SPDLOG_INFO(
						string() + "Resume Download media file" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", localSourceReferenceURL: " + localSourceReferenceURL +
						", resuming from fileSize: " + to_string(fileSize)
					);
					request.perform();

					(curlDownloadData.mediaSourceFileStream).close();
				}

				if (localM3u8TarGzOrStreaming == 1)
				{
					try
					{
						// by a convention, the directory inside the tar file
						// has to be named as 'content'
						string sourcePathName = "/content.tar.gz";

						_mmsStorage->manageTarFileInCaseOfIngestionOfSegments(
							ingestionJobKey, destBinaryPathName, workspaceIngestionRepository, sourcePathName
						);
					}
					catch (runtime_error &e)
					{
						string errorMessage = string("manageTarFileInCaseOfIngestionOfSegments "
													 "failed") +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) +
											  ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", localSourceReferenceURL: " + localSourceReferenceURL;

						SPDLOG_ERROR(string() + errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				downloadingCompleted = true;

				SPDLOG_INFO(
					string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", destBinaryPathName: " + destBinaryPathName +
					", downloadingCompleted: " + to_string(downloadingCompleted)
				);
				_mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred(ingestionJobKey, downloadingCompleted);
			}
			catch (curlpp::LogicError &e)
			{
				SPDLOG_ERROR(
					"Download failed (LogicError)"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", localSourceReferenceURL: {}"
					", downloadingStoppedByUser: {}"
					", exception: {}",
					_processorIdentifier, ingestionJobKey, localSourceReferenceURL, downloadingStoppedByUser, e.what()
				);

				if (downloadingStoppedByUser)
				{
					downloadingCompleted = true;
				}
				else
				{
					if (attemptIndex + 1 == _maxDownloadAttemptNumber)
					{
						SPDLOG_ERROR(
							string() + "Reached the max number of download attempts" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
						);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
							to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
							);
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
					else
					{
						SPDLOG_INFO(
							string() +
							"Download failed. sleeping before to attempt "
							"again" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", localSourceReferenceURL: " + localSourceReferenceURL +
							", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
						);
						this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
					}
				}
			}
			catch (curlpp::RuntimeError &e)
			{
				SPDLOG_ERROR(
					"Download failed (RuntimeError)"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", localSourceReferenceURL: {}"
					", downloadingStoppedByUser: {}"
					", exception: {}",
					_processorIdentifier, ingestionJobKey, localSourceReferenceURL, downloadingStoppedByUser, e.what()
				);

				if (downloadingStoppedByUser)
				{
					downloadingCompleted = true;
				}
				else
				{
					if (attemptIndex + 1 == _maxDownloadAttemptNumber)
					{
						SPDLOG_INFO(
							string() + "Reached the max number of download attempts" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
						);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
							to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
							);
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
					else
					{
						SPDLOG_INFO(
							string() +
							"Download failed. sleeping before to attempt "
							"again" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", localSourceReferenceURL: " + localSourceReferenceURL +
							", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
						);
						this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
					}
				}
			}
			catch (runtime_error e)
			{
				SPDLOG_ERROR(
					string() + "Download failed (runtime_error)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", localSourceReferenceURL: " + localSourceReferenceURL +
					", exception: " + e.what()
				);

				if (downloadingStoppedByUser)
				{
					downloadingCompleted = true;
				}
				else
				{
					if (attemptIndex + 1 == _maxDownloadAttemptNumber)
					{
						SPDLOG_INFO(
							string() + "Reached the max number of download attempts" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
						);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
							to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
							);
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
					else
					{
						SPDLOG_INFO(
							string() +
							"Download failed. sleeping before to attempt "
							"again" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", localSourceReferenceURL: " + localSourceReferenceURL +
							", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
						);
						this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
					}
				}
			}
			catch (exception e)
			{
				SPDLOG_ERROR(
					string() + "Download failed (exception)" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
					to_string(ingestionJobKey) + ", localSourceReferenceURL: " + localSourceReferenceURL + ", exception: " + e.what()
				);

				if (downloadingStoppedByUser)
				{
					downloadingCompleted = true;
				}
				else
				{
					if (attemptIndex + 1 == _maxDownloadAttemptNumber)
					{
						SPDLOG_INFO(
							string() + "Reached the max number of download attempts" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
						);

						SPDLOG_INFO(
							string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
							to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what()
							);
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
					else
					{
						SPDLOG_INFO(
							string() +
							"Download failed. sleeping before to attempt "
							"again" +
							", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", localSourceReferenceURL: " + localSourceReferenceURL +
							", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
						);
						this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
					}
				}
			}
		}
	}
}
