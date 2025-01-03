
#include "CheckIngestionTimes.h"
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"

void MMSEngineProcessor::handleCheckIngestionEvent()
{
	try
	{
		if (isMaintenanceMode())
		{
			SPDLOG_INFO(
				"Received handleCheckIngestionEvent, not managed it because of MaintenanceMode"
				", _processorIdentifier: {}",
				_processorIdentifier
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
				string sourceReference;

				auto [ingestionJobKey, ingestionJobLabel, workspace, ingestionDate, metaDataContent, ingestionType, ingestionStatus] =
					ingestionToBeManaged;

				SPDLOG_INFO(
					"json to be processed"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", ingestionJobLabel: {}"
					", workspace->_workspaceKey: {}"
					", ingestionDate: {}"
					", ingestionType: {}"
					", ingestionStatus: {}"
					", metaDataContent: {}",
					_processorIdentifier, ingestionJobKey, ingestionJobLabel, workspace->_workspaceKey, ingestionDate,
					MMSEngineDBFacade::toString(ingestionType), MMSEngineDBFacade::toString(ingestionStatus), metaDataContent
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
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 20 events, we have still to postpone all the events overcoming the thread limit
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
