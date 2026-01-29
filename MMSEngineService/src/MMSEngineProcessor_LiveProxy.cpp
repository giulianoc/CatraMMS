
#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "spdlog/fmt/fmt.h"

using namespace std;
using json = nlohmann::json;

void MMSEngineProcessor::manageLiveProxy(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	try
	{
		/*
		 * commented because it will be High by default
		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isPresent(parametersRoot, field))
		{
			encodingPriority =
				static_cast<MMSEngineDBFacade::EncodingPriority>(
				workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority =
				MMSEngineDBFacade::toEncodingPriority(
				parametersRoot.get(field, "").asString());
		}
		*/

		string configurationLabel;

		string streamSourceType;
		bool defaultBroadcast = false;
		int maxWidth = -1;
		string userAgent;
		string otherInputOptions;

		long waitingSecondsBetweenAttemptsInCaseOfErrors;
		json outputsRoot;
		bool timePeriod = false;
		int64_t utcProxyPeriodStart = -1;
		int64_t utcProxyPeriodEnd = -1;
		int64_t useVideoTrackFromMediaItemKey = -1;
		string taskEncodersPoolLabel;
		{
			int64_t pushEncoderKey;
			string streamEncodersPoolLabel;
			bool pushPublicEncoderName;
			{
				configurationLabel = JSONUtils::as<string>(parametersRoot, "configurationLabel", "", {}, true);

				useVideoTrackFromMediaItemKey = JSONUtils::as<int64_t>(parametersRoot, "useVideoTrackFromMediaItemKey", -1);

				{
					tie(streamSourceType, streamEncodersPoolLabel, pushEncoderKey, pushPublicEncoderName) =
						_mmsEngineDBFacade->stream_sourceTypeEncodersPoolPushEncoderKeyPushPublicEncoderName(
							workspace->_workspaceKey, configurationLabel
						);

					// default is IP_PULL
					if (streamSourceType.empty())
						streamSourceType = "IP_PULL";
				}
			}

			// EncodersPool override the one included in ChannelConf if present
			taskEncodersPoolLabel = JSONUtils::as<string>(parametersRoot, "encodersPool", "");

			// aggiungiomo 'encodersDetails' in ingestion parameters. In questo oggetto json mettiamo
			// l'encodersPool o l'encoderKey in caso di IP_PUSH che viene realmente utilizzato dall'MMS (MMSEngine::EncoderProxy).
			// In questo modo è possibile cambiare tramite API l'encoder per fare uno switch di un ingestionJob da un encoder ad un'altro
			{
				json encodersDetailsRoot;

				if (streamSourceType == "IP_PUSH")
				{
					encodersDetailsRoot["pushEncoderKey"] = pushEncoderKey;
					encodersDetailsRoot["pushPublicEncoderName"] = pushPublicEncoderName;
				}
				else
					encodersDetailsRoot["encodersPoolLabel"] = !taskEncodersPoolLabel.empty() ? taskEncodersPoolLabel : streamEncodersPoolLabel;

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

			defaultBroadcast = JSONUtils::as<bool>(parametersRoot, "defaultBroadcast", false);

			if (JSONUtils::isPresent(parametersRoot, "timePeriod"))
			{
				timePeriod = JSONUtils::as<bool>(parametersRoot, "timePeriod", false);
				if (timePeriod)
				{
					if (!JSONUtils::isPresent(parametersRoot, "schedule"))
					{
						string errorMessage = std::format(
							"Field is not present or it is null"
							", _processorIdentifier: {}"
							", field: {}",
							_processorIdentifier, "schedule"
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					// Validator validator(_logger, _mmsEngineDBFacade,
					// _configuration);

					json proxyPeriodRoot = parametersRoot["schedule"];

					string proxyPeriodStart = JSONUtils::as<string>(proxyPeriodRoot, "start", "", {}, true);
					utcProxyPeriodStart = Datetime::parseUtcStringToUtcInSecs(proxyPeriodStart);

					string proxyPeriodEnd = JSONUtils::as<string>(proxyPeriodRoot, "end", "", {}, true);
					utcProxyPeriodEnd = Datetime::parseUtcStringToUtcInSecs(proxyPeriodEnd);
				}
			}

			maxWidth = JSONUtils::as<int32_t>(parametersRoot, "maxWidth", -1);

			userAgent = JSONUtils::as<string>(parametersRoot, "userAgent", "");

			otherInputOptions = JSONUtils::as<string>(parametersRoot, "otherInputOptions", "");

			waitingSecondsBetweenAttemptsInCaseOfErrors = JSONUtils::as<int64_t>(parametersRoot, "waitingSecondsBetweenAttemptsInCaseOfErrors", 5);

			if (!JSONUtils::isPresent(parametersRoot, "outputs"))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _processorIdentifier: {}"
					", field: {}",
					_processorIdentifier, "outputs"
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (JSONUtils::isPresent(parametersRoot, "outputs"))
				outputsRoot = parametersRoot["outputs"];
		}
		string useVideoTrackFromPhysicalPathName;
		string useVideoTrackFromPhysicalDeliveryURL;
		if (useVideoTrackFromMediaItemKey != -1)
		{
			int64_t sourcePhysicalPathKey;

			LOG_INFO(
				"useVideoTrackFromMediaItemKey"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", useVideoTrackFromMediaItemKey: {}",
				_processorIdentifier, ingestionJobKey, useVideoTrackFromMediaItemKey
			);

			{
				int64_t encodingProfileKey = -1;
				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
					useVideoTrackFromMediaItemKey, encodingProfileKey, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);
				tie(sourcePhysicalPathKey, useVideoTrackFromPhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
			}

			// calculate delivery URL in case of an external encoder
			{
				tie(useVideoTrackFromPhysicalDeliveryURL, ignore) = _mmsDeliveryAuthorization->createDeliveryAuthorization(
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
					false,				// reuseAuthIfPresent
					false,				// playerIPToBeAuthorized
					"",					// playerCountry
					"",					// playerRegion
					false,				// save,
					"MMS_SignedURL",	// deliveryType,

					false, // warningIfMissingMediaItemKey,
					true,  // filteredByStatistic
					""	   // userId (it is not needed it filteredByStatistic is
					   // true
				);
			}
		}

		json localOutputsRoot = getReviewedOutputsRoot(outputsRoot, workspace, ingestionJobKey, false);

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
			/* 2023-01-01: normalmente, nel caso di un semplice Task di
			   LiveProxy, il drawTextDetails viene inserito all'interno
			   dell'OutputRoot, Nel caso pero' di un Live Channel, per capire il
			   campo broadcastDrawTextDetails, vedi il commento all'interno del
			   metodo java CatraMMSBroadcaster::buildLiveProxyJsonForBroadcast
			*/
			json filtersRoot = nullptr;

			string field = "broadcastFilters";
			if (JSONUtils::isPresent(parametersRoot, field))
				filtersRoot = parametersRoot[field];

			json streamInputRoot = _mmsEngineDBFacade->getStreamInputRoot(
				workspace, ingestionJobKey, configurationLabel, useVideoTrackFromPhysicalPathName, useVideoTrackFromPhysicalDeliveryURL, maxWidth,
				userAgent, otherInputOptions, taskEncodersPoolLabel, filtersRoot
			);

			json inputRoot;
			{
				inputRoot["streamInput"] = streamInputRoot;
				inputRoot["timePeriod"] = timePeriod;
				inputRoot["utcScheduleStart"] = utcProxyPeriodStart;
				inputRoot["sUtcScheduleStart"] = Datetime::utcToUtcString(utcProxyPeriodStart);
				inputRoot["utcScheduleEnd"] = utcProxyPeriodEnd;
				inputRoot["sUtcScheduleEnd"] = Datetime::utcToUtcString(utcProxyPeriodEnd);
				if (defaultBroadcast)
					inputRoot["defaultBroadcast"] = defaultBroadcast;
			}

			json localInputsRoot = json::array();
			localInputsRoot.push_back(inputRoot);

			inputsRoot = localInputsRoot;
		}

		_mmsEngineDBFacade->addEncoding_LiveProxyJob(
			workspace, ingestionJobKey,
			inputsRoot,			 // used by FFMPEGEncoder
			streamSourceType,	 // used by FFMPEGEncoder
			utcProxyPeriodStart, // used in MMSEngineDBFacade::getEncodingJobs
			// maxAttemptsNumberInCaseOfErrors,	// used in
			// EncoderVideoAudioProxy.cpp
			waitingSecondsBetweenAttemptsInCaseOfErrors, // used in
														 // EncoderVideoAudioProxy.cpp
			localOutputsRoot
		);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"manageLiveProxy failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what: {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw;
	}
}
