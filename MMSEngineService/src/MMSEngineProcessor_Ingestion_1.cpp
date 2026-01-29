
#include "CheckIngestionTimes.h"
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;

void MMSEngineProcessor::handleCheckIngestionEvent()
{
	try
	{
		if (isMaintenanceMode())
		{
			LOG_INFO(
				"Received handleCheckIngestionEvent, not managed it because of MaintenanceMode"
				", _processorIdentifier: {}",
				_processorIdentifier
			);

			return;
		}

		vector<tuple<int64_t, string, shared_ptr<Workspace>, string, string, MMSEngineDBFacade::IngestionType,
			MMSEngineDBFacade::IngestionStatus>> ingestionsToBeManaged;

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

			if (!newThreadPermission(_processorsThreadsNumber))
			{
				LOG_WARN(
					"Not enough available threads to manage Tasks involving more threads"
					", _processorIdentifier: {}"
					", _processorsThreadsNumber.use_count(): {}",
					_processorIdentifier, _processorsThreadsNumber.use_count()
				);

				onlyTasksNotInvolvingMMSEngineThreads = true;
			}

			_mmsEngineDBFacade->getIngestionsToBeManaged(
				ingestionsToBeManaged, _processorMMS, _maxIngestionJobsPerEvent, _timeBeforeToPrepareResourcesInMinutes,
				onlyTasksNotInvolvingMMSEngineThreads
			);

			LOG_INFO("getIngestionsToBeManaged result"
				", _processorIdentifier: {}"
				", ingestionsToBeManaged.size: {}", _processorIdentifier, ingestionsToBeManaged.size()
			);
		}
		catch (exception &e)
		{
			LOG_ERROR("getIngestionsToBeManaged failed"
				", _processorIdentifier: {}"
				", exception: {}", _processorIdentifier, e.what()
			);

			throw;
		}

		for (const auto& [ingestionJobKey, ingestionJobLabel, workspace,
			ingestionDate, metaDataContent, ingestionType, ingestionStatus] : ingestionsToBeManaged)
		{
			try
			{
				string sourceReference;

				LOG_INFO(
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
						_mmsEngineDBFacade->checkWorkspaceStorageAndMaxIngestionNumber(workspace->_workspaceKey);
				}
				catch (exception &e)
				{
					LOG_ERROR("checkWorkspaceStorageAndMaxIngestionNumber failed"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
					);
					string errorMessage = e.what();

					LOG_INFO("Update IngestionJob"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", IngestionStatus: End_WorkspaceReachedMaxStorageOrIngestionNumber"
						", errorMessage: {}", _processorIdentifier, ingestionJobKey, e.what()
					);
					try
					{
						_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey,
							MMSEngineDBFacade::IngestionStatus::End_WorkspaceReachedMaxStorageOrIngestionNumber, e.what()
						);
					}
					catch (exception &ex)
					{
						LOG_INFO("Update IngestionJob failed"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", IngestionStatus: End_WorkspaceReachedMaxStorageOrIngestionNumber"
							", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
						);
					}

					throw;
				}

				if (ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress ||
					ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress ||
					ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress ||
					ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
				{
					// source binary download or upload terminated (sourceBinaryTransferred is true)

					string sourceFileName = to_string(ingestionJobKey) + "_source";
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

					LOG_INFO("addEvent: EVENT_TYPE (INGESTASSETEVENT)"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", getEventKey().first: {}"
						", getEventKey().second: {}", _processorIdentifier, ingestionJobKey,
						event->getEventKey().first, event->getEventKey().second
					);
				}
				else // Start_TaskQueued
				{
					json parametersRoot;
					try
					{
						parametersRoot = JSONUtils::toJson<json>(metaDataContent);
					}
					catch (exception &e)
					{
						string errorMessage = std::format("metadata json is not well format"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", metaDataContent: {}", _processorIdentifier, ingestionJobKey, metaDataContent);
						LOG_ERROR(string() + errorMessage);

						LOG_INFO("Update IngestionJob"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", IngestionStatus: End_ValidationMetadataFailed"
							", errorMessage: {}", _processorIdentifier, ingestionJobKey, errorMessage
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, errorMessage
							);
						}
						catch (exception &ex)
						{
							LOG_INFO("Update IngestionJob failed"
								", _processorIdentifier: {}"
								", ingestionJobKey: {}"
								", IngestionStatus: End_ValidationMetadataFailed"
								", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
							);
						}

						throw runtime_error(errorMessage);
					}

					vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies;

					try
					{
						Validator validator(_mmsEngineDBFacade, _configurationRoot);
						if (ingestionType == MMSEngineDBFacade::IngestionType::GroupOfTasks)
							Validator::validateGroupOfTasksMetadata(workspace->_workspaceKey, parametersRoot);
						else
							dependencies = validator.validateSingleTaskMetadata(workspace->_workspaceKey, ingestionType, parametersRoot);
						LOG_INFO("Validator"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", ingestionType: {}"
							", dependencies.size: {}",
							_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionType), dependencies.size()
						);
					}
					catch (exception &e)
					{
						LOG_ERROR(
							"validateMetadata failed"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
						);

						string errorMessage = e.what();

						LOG_INFO(
							"Update IngestionJob"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", IngestionStatus: {}"
							", errorMessage: {}", _processorIdentifier, ingestionJobKey, "End_ValidationMetadataFailed", errorMessage
						);
						try
						{
							_mmsEngineDBFacade->updateIngestionJob(
								ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, errorMessage
							);
						}
						catch (exception &ex)
						{
							LOG_INFO("Update IngestionJob failed"
								", _processorIdentifier: {}"
								", ingestionJobKey: {}"
								", IngestionStatus: End_ValidationMetadataFailed"
								", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
							);
						}

						throw runtime_error(errorMessage);
					}

					{
						switch (ingestionType)
						{
						case MMSEngineDBFacade::IngestionType::GroupOfTasks:
						{
							try
							{
								manageGroupOfTasks(ingestionJobKey, workspace, parametersRoot);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageGroupOfTasks failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO("Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}", _processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &e)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, e.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::AddContent:
						{
							MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
							string mediaSourceURL;
							string mediaFileFormat;
							string md5FileCheckSum;
							int fileSizeInBytes;
							int64_t encodingProfileKey;
							bool externalReadOnlyStorage;
							try
							{
								tie(nextIngestionStatus, mediaSourceURL, mediaFileFormat, encodingProfileKey, md5FileCheckSum, fileSizeInBytes,
									externalReadOnlyStorage) = getMediaSourceDetails(ingestionJobKey, workspace, ingestionType, parametersRoot);
							}
							catch (exception &e)
							{
								LOG_ERROR("getMediaSourceDetails failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO("Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_ValidationMediaSourceFailed"
									", errorMessage: {}", _processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_ValidationMetadataFailed"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
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

									LOG_INFO("addEvent: EVENT_TYPE (INGESTASSETEVENT)"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", getEventKey().first: {}"
										", getEventKey().second: {}", _processorIdentifier, ingestionJobKey,
										event->getEventKey().first, event->getEventKey().second
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
										/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent
										 * 2021-06-19: we still have to check the thread limit because, in case handleCheckIngestionEvent gets 20
										 * 		events, we have still to postpone all the events overcoming the thread limit
										 */
										if (!newThreadPermission(_processorsThreadsNumber))
										{
											LOG_WARN(
												"Not enough available threads to manage downloadMediaSourceFileThread, activity is postponed"
												", _processorIdentifier: {}"
												", ingestionJobKey: {}"
												", _processorsThreadsNumber.use_count(): {}",
												_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
											);

											string errorMessage;
											string processorMMS;

											LOG_INFO(
												"Update IngestionJob"
												", _processorIdentifier: {}"
												", ingestionJobKey: {}"
												", IngestionStatus: {}"
												", errorMessage: {}"
												", processorMMS: {}",
												_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
												processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
										}
										else
										{
											string errorMessage;
											string processorMMS;

											LOG_INFO(
												"Update IngestionJob"
												", _processorIdentifier: {}"
												", ingestionJobKey: {}"
												", IngestionStatus: {}"
												", errorMessage: {}"
												", processorMMS: {}",
												_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(nextIngestionStatus), errorMessage,
												processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);

											// 2021-09-02: regenerateTimestamps
											// is used only
											//	in case of streaming-to-mp4
											//	(see
											// docs/TASK_01_Add_Content_JSON_Format.txt)
											bool regenerateTimestamps = false;
											if (mediaFileFormat == "streaming-to-mp4")
												regenerateTimestamps = JSONUtils::as<bool>(parametersRoot, "regenerateTimestamps", false);

											thread downloadMediaSource(
												&MMSEngineProcessor::downloadMediaSourceFileThread, this, _processorsThreadsNumber, mediaSourceURL,
												regenerateTimestamps, m3u8TarGzOrStreaming, ingestionJobKey, workspace
											);
											downloadMediaSource.detach();
										}
									}
									else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress)
									{
										/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent
										 * 2021-06-19: we still have to check the thread limit because, in case handleCheckIngestionEvent gets 20
										 * 		events, we have still to postpone all the events overcoming the thread limit
										 */
										if (!newThreadPermission(_processorsThreadsNumber))
										{
											LOG_WARN(
												"Not enough available threads to manage moveMediaSourceFileThread, activity is postponed"
												", _processorIdentifier: {}"
												", ingestionJobKey: {}"
												", _processorsThreadsNumber.use_count(): {}",
												_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
											);

											string errorMessage;
											string processorMMS;

											LOG_INFO(
												"Update IngestionJob"
												", _processorIdentifier: {}"
												", ingestionJobKey: {}"
												", IngestionStatus: {}"
												", errorMessage: {}"
												", processorMMS: {}",
												_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
												processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
										}
										else
										{
											string errorMessage;
											string processorMMS;

											LOG_INFO(
												"Update IngestionJob"
												", _processorIdentifier: {}"
												", ingestionJobKey: {}"
												", IngestionStatus: {}"
												", errorMessage: {}"
												", processorMMS: {}",
												_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(nextIngestionStatus), errorMessage,
												processorMMS
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
										/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent
										 * 2021-06-19: we still have to check the thread limit because, in case handleCheckIngestionEvent gets 20
										 * 		events, we have still to postpone all the events overcoming the thread limit
										 */
										if (!newThreadPermission(_processorsThreadsNumber))
										{
											LOG_WARN(
												"Not enough available threads to manage copyMediaSourceFileThread, activity is postponed"
												", _processorIdentifier: {}"
												", ingestionJobKey: {}"
												", _processorsThreadsNumber.use_count(): {}",
												_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
											);

											string errorMessage;
											string processorMMS;

											LOG_INFO(
												"Update IngestionJob"
												", _processorIdentifier: {}"
												", ingestionJobKey: {}"
												", IngestionStatus: {}"
												", errorMessage: {}"
												", processorMMS: {}",
												_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
												processorMMS
											);
											_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
										}
										else
										{
											string errorMessage;
											string processorMMS;

											LOG_INFO(
												"Update IngestionJob"
												", _processorIdentifier: {}"
												", ingestionJobKey: {}"
												", IngestionStatus: {}"
												", errorMessage: {}"
												", processorMMS: {}",
												_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(nextIngestionStatus), errorMessage,
												processorMMS
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
										string errorMessage;
										string processorMMS;

										LOG_INFO(
											"Update IngestionJob"
											", _processorIdentifier: {}"
											", ingestionJobKey: {}"
											", IngestionStatus: {}"
											", errorMessage: {}"
											", processorMMS: {}",
											_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(nextIngestionStatus), errorMessage,
											processorMMS
										);
										_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, nextIngestionStatus, errorMessage, processorMMS);
									}
								}
							}
							catch (exception &e)
							{
								string errorMessage = std::format("Downloading media source or update "
									"Ingestion job failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what());
								LOG_ERROR(string() + errorMessage);

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::RemoveContent:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent
								 * 2021-06-19: we still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage removeContentThread, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
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
							catch (exception &e)
							{
								LOG_ERROR("removeContentThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::FTPDelivery:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent
								 * 2021-06-19: we still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage ftpDeliveryContentThread, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										"_processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread ftpDeliveryContentThread(
										&MMSEngineProcessor::ftpDeliveryContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as reference because it will change soon by the parent thread
									);
									ftpDeliveryContentThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("ftpDeliveryContentThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::LocalCopy:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								if (!_localCopyTaskEnabled)
								{
									string errorMessage = std::format("Local-Copy Task is not enabled "
										"in this MMS deploy"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}", _processorIdentifier, ingestionJobKey);
									LOG_ERROR(string() + errorMessage);

									throw runtime_error(errorMessage);
								}

								/*
								// threads check is done inside localCopyContentTask localCopyContentTask(
										ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
								*/
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent
								 * 2021-06-19: we still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage localCopyContent, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread localCopyContentThread(
										&MMSEngineProcessor::localCopyContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as reference because it will change soon by the parent thread
									);
									localCopyContentThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("localCopyContentThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::HTTPCallback:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* threads check is done inside httpCallbackTask httpCallbackTask(
										ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
								*/
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage http callback, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread httpCallbackThread(
										&MMSEngineProcessor::httpCallbackThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as reference because it will change soon by the parent thread
									);
									httpCallbackThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("httpCallbackThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::Encode:
						{
							try
							{
								manageEncodeTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageEncodeTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::VideoSpeed:
						{
							try
							{
								manageVideoSpeedTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR(
									string() + "manageVideoSpeedTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									", ingestionJobKey: " + to_string(ingestionJobKey) + ", exception: " + e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::PictureInPicture:
						{
							try
							{
								managePictureInPictureTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("managePictureInPictureTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::IntroOutroOverlay:
						{
							try
							{
								manageIntroOutroOverlayTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageIntroOutroOverlayTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::AddSilentAudio:
						{
							try
							{
								manageAddSilentAudioTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageAddSilentAudioTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, + e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::Frame:
						case MMSEngineDBFacade::IngestionType::PeriodicalFrames:
						case MMSEngineDBFacade::IngestionType::IFrames:
						case MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames:
						case MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
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
									/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent
									 * 2021-06-19: we still have to check the thread limit because, in case handleCheckIngestionEvent gets 20
									 * events, we have still to postpone all the events overcoming the thread limit
									 */
									if (!newThreadPermission(_processorsThreadsNumber))
									{
										LOG_WARN(
											"Not enough available threads to manage changeFileFormatThread, activity is postponed"
											", _processorIdentifier: {}"
											", ingestionJobKey: {}"
											", _processorsThreadsNumber.use_count(): {}",
											_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
										);

										string errorMessage;
										string processorMMS;

										LOG_INFO(
											"Update IngestionJob"
											", _processorIdentifier: {}"
											", ingestionJobKey: {}"
											", IngestionStatus: {}"
											", errorMessage: {}"
											", processorMMS: {}",
											_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
											processorMMS
										);
										_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
									}
									else
									{
										thread generateAndIngestFrameThread(
											&MMSEngineProcessor::generateAndIngestFrameThread, this, _processorsThreadsNumber, ingestionJobKey,
											workspace, ingestionType, parametersRoot,
											// it cannot be passed as reference because it will change soon by the parent thread
											dependencies
										);
										generateAndIngestFrameThread.detach();
									}
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("generateAndIngestFramesTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::Slideshow:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageSlideShowTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageSlideShowTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::ConcatDemuxer:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage manageConcatThread, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread manageConcatThread(
										&MMSEngineProcessor::manageConcatThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,

										// it cannot be passed as reference because it will change soon by the parent thread
										dependencies
									);
									manageConcatThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("manageConcatThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::Cut:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage manageCutMediaThread, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread manageCutMediaThread(
										&MMSEngineProcessor::manageCutMediaThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as reference because it will change soon by the parent thread
									);
									manageCutMediaThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("manageCutMediaThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::ExtractTracks:
						{
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage extractTracksContentThread, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread extractTracksContentThread(
										&MMSEngineProcessor::extractTracksContentThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as reference because it will change soon by the parent thread
									);
									extractTracksContentThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("extractTracksContentThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::OverlayImageOnVideo:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageOverlayImageOnVideoTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageOverlayImageOnVideoTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::OverlayTextOnVideo:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageOverlayTextOnVideoTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageOverlayTextOnVideoTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::EmailNotification:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage email notification, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread emailNotificationThread(
										&MMSEngineProcessor::emailNotificationThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as reference because it will change soon by the parent thread
									);
									emailNotificationThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("emailNotificationThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::CheckStreaming:
						{
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage check streaming, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
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
							catch (exception &e)
							{
								LOG_ERROR("checkStreamingThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::MediaCrossReference:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageMediaCrossReferenceTask(ingestionJobKey, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageMediaCrossReferenceTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::PostOnFacebook:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage post on facebook, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread postOnFacebookThread(
										&MMSEngineProcessor::postOnFacebookThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as reference because it will change soon by the parent thread
									);
									postOnFacebookThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("postOnFacebookThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::PostOnYouTube:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage post on youtube, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread postOnYouTubeThread(
										&MMSEngineProcessor::postOnYouTubeThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as reference because it will change soon by the parent thread
									);
									postOnYouTubeThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("postOnYouTubeTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::FaceRecognition:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageFaceRecognitionMediaTask(ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageFaceRecognitionMediaTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::FaceIdentification:
						{
							// mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
							try
							{
								manageFaceIdentificationMediaTask(ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageFaceIdentificationMediaTask failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::LiveRecorder:
						{
							try
							{
								manageLiveRecorder(ingestionJobKey, ingestionJobLabel, ingestionStatus, workspace, parametersRoot);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageLiveRecorder failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::LiveProxy:
						{
							try
							{
								manageLiveProxy(ingestionJobKey, ingestionStatus, workspace, parametersRoot);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageLiveProxy failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::VODProxy:
						{
							try
							{
								manageVODProxy(ingestionJobKey, ingestionStatus, workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageVODProxy failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::Countdown:
						{
							try
							{
								manageCountdown(ingestionJobKey, ingestionStatus, /* ingestionDate, */ workspace, parametersRoot, dependencies);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageCountdown failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::LiveGrid:
						{
							try
							{
								manageLiveGrid(ingestionJobKey, ingestionStatus, workspace, parametersRoot);
							}
							catch (exception &e)
							{
								LOG_ERROR("manageLiveGrid failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::LiveCut:
						{
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage manageLiveCutThread, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
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
							catch (exception &e)
							{
								LOG_ERROR("manageLiveCutThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::YouTubeLiveBroadcast:
						{
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent
								 * 2021-06-19: we still have to check the thread limit because, in case
								 *handleCheckIngestionEvent gets 20 events, we have still to postpone all the events
								 *overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage YouTubeLiveBroadcast, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
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
							catch (exception &e)
							{
								LOG_ERROR("youTubeLiveBroadcastThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::FacebookLiveBroadcast:
						{
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent
								 * 2021-06-19: we still have to check the thread limit because, in case
								 *handleCheckIngestionEvent gets 20 events, we have still to postpone all the events
								 *overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage facebookLiveBroadcastThread, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
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
							catch (exception &e)
							{
								LOG_ERROR("facebookLiveBroadcastThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						case MMSEngineDBFacade::IngestionType::ChangeFileFormat:
						{
							try
							{
								/* 2021-02-19: check on threads is already done in handleCheckIngestionEvent 2021-06-19: we
								 *still have to check the thread limit because, in case handleCheckIngestionEvent gets
								 *20 events, we have still to postpone all the events overcoming the thread limit
								 */
								if (!newThreadPermission(_processorsThreadsNumber))
								{
									LOG_WARN(
										"Not enough available threads to manage changeFileFormatThread, activity is postponed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", _processorsThreadsNumber.use_count(): {}",
										_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
									);

									string errorMessage;
									string processorMMS;

									LOG_INFO(
										"Update IngestionJob"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: {}"
										", errorMessage: {}"
										", processorMMS: {}",
										_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionStatus), errorMessage,
										processorMMS
									);
									_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
								}
								else
								{
									thread changeFileFormatThread(
										&MMSEngineProcessor::changeFileFormatThread, this, _processorsThreadsNumber, ingestionJobKey, workspace,
										parametersRoot,
										dependencies // it cannot be passed as reference because it will change soon by the parent thread
									);
									changeFileFormatThread.detach();
								}
							}
							catch (exception &e)
							{
								LOG_ERROR("changeFileFormatThread failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
								);

								string errorMessage = e.what();

								LOG_INFO(
									"Update IngestionJob"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_IngestionFailure"
									", errorMessage: {}",
									_processorIdentifier, ingestionJobKey, errorMessage
								);
								try
								{
									_mmsEngineDBFacade->updateIngestionJob(
										ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, errorMessage
									);
								}
								catch (exception &ex)
								{
									LOG_INFO("Update IngestionJob failed"
										", _processorIdentifier: {}"
										", ingestionJobKey: {}"
										", IngestionStatus: End_IngestionFailure"
										", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
									);
								}

								throw runtime_error(errorMessage);
							}
							break;
						}
						default:
						{
							string errorMessage = std::format("Unknown IngestionType"
								", _processorIdentifier: {}"
								", ingestionJobKey: {}"
								", ingestionType: {}",
								_processorIdentifier, ingestionJobKey, MMSEngineDBFacade::toString(ingestionType));
							LOG_ERROR(string() + errorMessage);

							LOG_INFO(
								"Update IngestionJob"
								", _processorIdentifier: {}"
								", ingestionJobKey: {}"
								", IngestionStatus: End_ValidationMediaSourceFailed"
								", errorMessage: {}",
								_processorIdentifier, ingestionJobKey, errorMessage
							);
							try
							{
								_mmsEngineDBFacade->updateIngestionJob(
									ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, errorMessage
								);
							}
							catch (exception &ex)
							{
								LOG_INFO("Update IngestionJob failed"
									", _processorIdentifier: {}"
									", ingestionJobKey: {}"
									", IngestionStatus: End_ValidationMediaSourceFailed"
									", errorMessage: {}", _processorIdentifier, ingestionJobKey, ex.what()
								);
							}

							throw runtime_error(errorMessage);
						}
						}
					}
				}
			}
			catch (exception &e)
			{
				LOG_ERROR(
					"Exception managing the Ingestion entry"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
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

			LOG_DEBUG("addEvent: EVENT_TYPE (MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION)"
				", _processorIdentifier: {}"
				", getEventKey().first: {}"
				", getEventKey().second: {}", _processorIdentifier,
				event->getEventKey().first, event->getEventKey().second
			);
		}
	}
	catch (exception& e)
	{
		LOG_ERROR("handleCheckIngestionEvent failed"
			", _processorIdentifier: {}"
			", exception: {}", _processorIdentifier, e.what()
		);
	}
}
