
#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"

void MMSEngineProcessor::manageVODProxy(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		SPDLOG_INFO(
			string() + "manageVODProxy" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		if (dependencies.size() < 1)
		{
			string errorMessage = string() + "Wrong source number to be proxied" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// aggiungiomo 'encodersDetails' in ingestion parameters. In questo oggetto json mettiamo
		// l'encodersPool che viene realmente utilizzato dall'MMS (MMSEngine::EncoderProxy).
		// In questo modo è possibile cambiare tramite API l'encoder per fare uno switch di un ingestionJob da un encoder ad un'altro
		{
			string taskEncodersPoolLabel = JSONUtils::asString(parametersRoot, "encodersPool", "");

			json encodersDetailsRoot;

			encodersDetailsRoot["encodersPoolLabel"] = taskEncodersPoolLabel;

			if (JSONUtils::isMetadataPresent(parametersRoot, "internalMMS"))
				parametersRoot["internalMMS"]["encodersDetails"] = encodersDetailsRoot;
			else
			{
				json internalMMSRoot;
				internalMMSRoot["encodersDetails"] = encodersDetailsRoot;
				parametersRoot["internalMMS"] = internalMMSRoot;
			}

			_mmsEngineDBFacade->updateIngestionJobMetadataContent(ingestionJobKey, JSONUtils::toString(parametersRoot));
		}

		json outputsRoot;
		bool timePeriod = false;
		int64_t utcProxyPeriodStart = -1;
		int64_t utcProxyPeriodEnd = -1;
		bool defaultBroadcast = false;
		{
			string field = "timePeriod";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				timePeriod = JSONUtils::asBool(parametersRoot, field, false);
				if (timePeriod)
				{
					field = "schedule";
					if (!JSONUtils::isMetadataPresent(parametersRoot, field))
					{
						string errorMessage = string() + "Field is not present or it is null" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					// Validator validator(_logger, _mmsEngineDBFacade,
					// _configuration);

					json proxyPeriodRoot = parametersRoot[field];

					field = "start";
					if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
					{
						string errorMessage = string() + "Field is not present or it is null" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
					utcProxyPeriodStart = Datetime::parseUtcStringToUtcInSecs(proxyPeriodStart);

					field = "end";
					if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
					{
						string errorMessage = string() + "Field is not present or it is null" +
											  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
					utcProxyPeriodEnd = Datetime::parseUtcStringToUtcInSecs(proxyPeriodEnd);
				}
			}

			field = "defaultBroadcast";
			defaultBroadcast = JSONUtils::asBool(parametersRoot, field, false);

			field = "outputs";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				outputsRoot = parametersRoot[field];
			else // if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs",
				 // false))
				outputsRoot = parametersRoot["Outputs"];
		}

		MMSEngineDBFacade::ContentType vodContentType;

		json inputsRoot = json::array();
		// 2021-12-22: in case of a Broadcaster, we may have a playlist
		// (inputsRoot)
		//		already ready
		if (JSONUtils::isMetadataPresent(parametersRoot, "internalMMS") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"], "broadcaster") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"]["broadcaster"], "broadcasterInputsRoot"))
		{
			inputsRoot = parametersRoot["internalMMS"]["broadcaster"]["broadcasterInputsRoot"];
		}
		else
		{
			vector<tuple<int64_t, string, string, string>> sources;

			for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
			{
				string sourcePhysicalPathName;
				int64_t sourcePhysicalPathKey;

				int64_t key;
				// MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

				tie(key, vodContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				SPDLOG_INFO(
					string() + "manageVODProxy" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", key: " + to_string(key)
				);

				int64_t sourceMediaItemKey;
				string mediaItemTitle;
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
					tie(sourcePhysicalPathKey, sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

					sourceMediaItemKey = key;

					warningIfMissing = false;
					tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> mediaItemKeyDetails =
						_mmsEngineDBFacade->getMediaItemKeyDetails(
							workspace->_workspaceKey, sourceMediaItemKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

					tie(ignore, mediaItemTitle, ignore, ignore, ignore, ignore) = mediaItemKeyDetails;
				}
				else
				{
					tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;

					sourcePhysicalPathKey = key;

					bool warningIfMissing = false;
					tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemKeyDetails =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, sourcePhysicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato
							// appena aggiunto
							true
						);

					tie(sourceMediaItemKey, ignore, mediaItemTitle, ignore, ignore, ignore, ignore, ignore, ignore) = mediaItemKeyDetails;
				}

				// int64_t durationInMilliSeconds =
				// 	_mmsEngineDBFacade->getMediaDurationInMilliseconds(
				// 		-1, sourcePhysicalPathKey);

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

						sourcePhysicalPathKey,

						-1, // ingestionJobKey,	(in case of live)
						-1, // deliveryCode,

						365 * 24 * 60 * 60, // ttlInSeconds, 365 days!!!
						999999,				// maxRetries,
						false,				// playerIPToBeAuthorized
						false,				// save,
						"MMS_SignedURL",	// deliveryType,

						false, // warningIfMissingMediaItemKey,
						true,  // filteredByStatistic
						""	   // userId (it is not needed it
							   // filteredByStatistic is true
					);

					tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
				}

				sources.push_back(make_tuple(sourcePhysicalPathKey, mediaItemTitle, sourcePhysicalPathName, sourcePhysicalDeliveryURL));
			}

			/* 2023-01-01: normalmente, nel caso di un semplica Task di
			   VODProxy, il drawTextDetails viene inserito all'interno
			   dell'OutputRoot, Nel caso pero' di un Live Channel, per capire il
			   campo broadcastDrawTextDetails, vedi il commento all'interno del
			   metodo java CatraMMSBroadcaster::buildVODProxyJsonForBroadcast
			*/
			json filtersRoot = nullptr;

			if (JSONUtils::isMetadataPresent(parametersRoot, "broadcastFilters"))
				filtersRoot = parametersRoot["broadcastFilters"];

			string otherInputOptions = JSONUtils::asString(parametersRoot, "otherInputOptions");

			// same json structure is used in
			// API_Ingestion::changeLiveProxyPlaylist
			json vodInputRoot = _mmsEngineDBFacade->getVodInputRoot(vodContentType, sources, filtersRoot, otherInputOptions);

			json inputRoot;
			{
				string field = "vodInput";
				inputRoot[field] = vodInputRoot;

				field = "timePeriod";
				inputRoot[field] = timePeriod;

				field = "utcScheduleStart";
				inputRoot[field] = utcProxyPeriodStart;

				field = "sUtcScheduleStart";
				inputRoot[field] = Datetime::utcToUtcString(utcProxyPeriodStart);

				field = "utcScheduleEnd";
				inputRoot[field] = utcProxyPeriodEnd;

				field = "sUtcScheduleEnd";
				inputRoot[field] = Datetime::utcToUtcString(utcProxyPeriodEnd);

				if (defaultBroadcast)
				{
					field = "defaultBroadcast";
					inputRoot[field] = defaultBroadcast;
				}
			}

			inputsRoot.push_back(inputRoot);
		}

		json localOutputsRoot =
			getReviewedOutputsRoot(outputsRoot, workspace, ingestionJobKey, vodContentType == MMSEngineDBFacade::ContentType::Image ? true : false);

		// the only reason we may have a failure should be in case the vod is
		// missing/removed
		long waitingSecondsBetweenAttemptsInCaseOfErrors = 30;
		long maxAttemptsNumberInCaseOfErrors = 3;

		_mmsEngineDBFacade->addEncoding_VODProxyJob(
			workspace, ingestionJobKey, inputsRoot,

			utcProxyPeriodStart, localOutputsRoot, maxAttemptsNumberInCaseOfErrors, waitingSecondsBetweenAttemptsInCaseOfErrors,
			_mmsWorkflowIngestionURL
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			string() + "manageVODProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageVODProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageVODProxy failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
