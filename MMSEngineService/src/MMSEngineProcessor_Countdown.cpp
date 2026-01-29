
#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"

using namespace std;
using json = nlohmann::json;

void MMSEngineProcessor::manageCountdown(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus,
	// string ingestionDate, sotto non viene usato per cui lo commento
	shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong media number for Countdown" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// aggiungiomo 'encodersDetails' in ingestion parameters. In questo oggetto json mettiamo
		// l'encodersPool che viene realmente utilizzato dall'MMS (MMSEngine::EncoderProxy).
		// In questo modo è possibile cambiare tramite API l'encoder per fare uno switch di un ingestionJob da un encoder ad un'altro
		{
			string taskEncodersPoolLabel = JSONUtils::as<string>(parametersRoot, "encodersPool", "");

			json encodersDetailsRoot;

			encodersDetailsRoot["encodersPoolLabel"] = taskEncodersPoolLabel;

			if (JSONUtils::isPresent(parametersRoot, "internalMMS"))
				parametersRoot["internalMMS"]["encodersDetails"] = encodersDetailsRoot;
			else
			{
				json internalMMSRoot;
				internalMMSRoot["encodersDetails"] = encodersDetailsRoot;
				parametersRoot["internalMMS"] = internalMMSRoot;
			}

			_mmsEngineDBFacade->updateIngestionJobMetadataContent(ingestionJobKey, JSONUtils::toString(parametersRoot));
		}

		bool defaultBroadcast = false;
		bool timePeriod = true;
		int64_t utcProxyPeriodStart = -1;
		int64_t utcProxyPeriodEnd = -1;
		{
			string field = "defaultBroadcast";
			defaultBroadcast = JSONUtils::as<bool>(parametersRoot, field, false);

			field = "schedule";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// Validator validator(_logger, _mmsEngineDBFacade, _configuration);

			json proxyPeriodRoot = parametersRoot[field];

			field = "start";
			if (!JSONUtils::isPresent(proxyPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string proxyPeriodStart = JSONUtils::as<string>(proxyPeriodRoot, field, "");
			utcProxyPeriodStart = Datetime::parseUtcStringToUtcInSecs(proxyPeriodStart);

			field = "end";
			if (!JSONUtils::isPresent(proxyPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string proxyPeriodEnd = JSONUtils::as<string>(proxyPeriodRoot, field, "");
			utcProxyPeriodEnd = Datetime::parseUtcStringToUtcInSecs(proxyPeriodEnd);
		}

		string mmsSourceVideoAssetPathName;
		string mmsSourceVideoAssetDeliveryURL;
		MMSEngineDBFacade::ContentType referenceContentType;
		int64_t sourceMediaItemKey;
		int64_t sourcePhysicalPathKey;
		{
			int64_t key;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];
			tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;

				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPath = _mmsStorage->getPhysicalPathDetails(
					key, encodingProfileKey, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);
				tie(sourcePhysicalPathKey, mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPath;

				sourceMediaItemKey = key;

				// sourcePhysicalPathKey = -1;
			}
			else
			{
				tuple<string, int, string, string, int64_t, string> physicalPath = _mmsStorage->getPhysicalPathDetails(
					key,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);
				tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPath;

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

				MMSEngineDBFacade::ContentType localContentType;
				string localTitle;
				string userData;
				string ingestionDate;
				int64_t localIngestionJobKey;
				tie(sourceMediaItemKey, localContentType, localTitle, userData, ingestionDate, localIngestionJobKey, ignore, ignore, ignore) =
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
			}

			// calculate delivery URL in case of an external encoder
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

					abs(utcNow - utcProxyPeriodEnd), // ttlInSeconds,
					999999,							 // maxRetries,
					false,							 // reuseAuthIfPresent
					false,							 // playerIPToBeAuthorized
					"",								 // playerCountry
					"",								 // playerRegion
					false,							 // save,
					"MMS_SignedURL",				 // deliveryType,

					false, // warningIfMissingMediaItemKey,
					true,  // filteredByStatistic
					""	   // userId (it is not needed it filteredByStatistic is
					   // true
				);

				tie(mmsSourceVideoAssetDeliveryURL, ignore) = deliveryAuthorizationDetails;
			}
		}

		int64_t videoDurationInMilliSeconds = _mmsEngineDBFacade->getMediaDurationInMilliseconds(
			sourceMediaItemKey, sourcePhysicalPathKey,
			// 2022-12-18: MIK potrebbe essere stato appena aggiunto
			true
		);

		json outputsRoot;
		{
			string field = "outputs";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (JSONUtils::isPresent(parametersRoot, field))
				outputsRoot = parametersRoot[field];
			else // if (JSONUtils::isPresent(parametersRoot, "Outputs",
				 // false))
				outputsRoot = parametersRoot["Outputs"];
		}

		json localOutputsRoot = getReviewedOutputsRoot(
			outputsRoot, workspace, ingestionJobKey, referenceContentType == MMSEngineDBFacade::ContentType::Image ? true : false
		);

		// 2021-12-22: in case of a Broadcaster, we may have a playlist
		// (inputsRoot) already ready
		json inputsRoot;
		if (JSONUtils::isPresent(parametersRoot, "internalMMS") &&
			JSONUtils::isPresent(parametersRoot["internalMMS"], "broadcaster") &&
			JSONUtils::isPresent(parametersRoot["internalMMS"]["broadcaster"], "broadcasterInputsRoot"))
		{
			inputsRoot = parametersRoot["internalMMS"]["broadcaster"]["broadcasterInputsRoot"];
		}
		else
		{
			/* 2023-01-01: normalmente, nel caso di un semplica Task di
			   Countdown, il drawTextDetails viene inserito all'interno
			   dell'OutputRoot, Nel caso pero' di un Live Channel, per capire il
			   campo broadcastDrawTextDetails, vedi il commento all'interno del
			   metodo java CatraMMSBroadcaster::buildCountdownJsonForBroadcast
			*/
			json filtersRoot = nullptr;

			string field = "broadcastFilters";
			if (JSONUtils::isPresent(parametersRoot, field))
				filtersRoot = parametersRoot[field];

			// same json structure is used in
			// API_Ingestion::changeLiveProxyPlaylist
			json countdownInputRoot = _mmsEngineDBFacade->getCountdownInputRoot(
				mmsSourceVideoAssetPathName, mmsSourceVideoAssetDeliveryURL, sourcePhysicalPathKey, videoDurationInMilliSeconds, filtersRoot
			);

			json inputRoot;
			{
				string field = "countdownInput";
				inputRoot[field] = countdownInputRoot;

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

			json localInputsRoot = json::array();
			localInputsRoot.push_back(inputRoot);

			inputsRoot = localInputsRoot;
		}

		// the only reason we may have a failure should be in case the image is
		// missing/removed
		long waitingSecondsBetweenAttemptsInCaseOfErrors = 30;
		long maxAttemptsNumberInCaseOfErrors = 3;

		_mmsEngineDBFacade->addEncoding_CountdownJob(
			workspace, ingestionJobKey, inputsRoot, utcProxyPeriodStart, localOutputsRoot, maxAttemptsNumberInCaseOfErrors,
			waitingSecondsBetweenAttemptsInCaseOfErrors
		);
	}
	catch (DBRecordNotFound &e)
	{
		LOG_ERROR(
			string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (runtime_error &e)
	{
		LOG_ERROR(
			string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		LOG_ERROR(
			string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
