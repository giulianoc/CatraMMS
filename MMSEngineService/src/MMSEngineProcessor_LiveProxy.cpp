
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "catralibraries/DateTime.h"
#include "spdlog/fmt/fmt.h"

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
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
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
			{
				string field = "configurationLabel";
				configurationLabel = JSONUtils::asString(parametersRoot, field, "", true);

				field = "useVideoTrackFromMediaItemKey";
				useVideoTrackFromMediaItemKey = JSONUtils::asInt64(parametersRoot, field, -1);

				{
					/*
					bool warningIfMissing = false;
					tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
						channelConfDetails = _mmsEngineDBFacade->getStreamDetails(workspace->_workspaceKey, configurationLabel, warningIfMissing);
					tie(ignore, streamSourceType, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
						ignore, ignore, ignore, ignore) = channelConfDetails;
					*/

					streamSourceType = _mmsEngineDBFacade->stream_sourceType(workspace->_workspaceKey, configurationLabel);

					// default is IP_PULL
					if (streamSourceType == "")
						streamSourceType = "IP_PULL";
				}
			}

			// EncodersPool override the one included in ChannelConf if present
			string field = "encodersPool";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				taskEncodersPoolLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "defaultBroadcast";
			defaultBroadcast = JSONUtils::asBool(parametersRoot, field, false);

			field = "timePeriod";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				timePeriod = JSONUtils::asBool(parametersRoot, field, false);
				if (timePeriod)
				{
					field = "schedule";
					if (!JSONUtils::isMetadataPresent(parametersRoot, field))
					{
						string errorMessage = fmt::format(
							"Field is not present or it is null"
							", _processorIdentifier: {}"
							", field: {}",
							_processorIdentifier, field
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					// Validator validator(_logger, _mmsEngineDBFacade,
					// _configuration);

					json proxyPeriodRoot = parametersRoot[field];

					field = "start";
					string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "", true);
					utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);

					field = "end";
					string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "", true);
					utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
				}
			}

			field = "maxWidth";
			maxWidth = JSONUtils::asInt(parametersRoot, field, -1);

			field = "userAgent";
			userAgent = JSONUtils::asString(parametersRoot, field, "");

			field = "otherInputOptions";
			otherInputOptions = JSONUtils::asString(parametersRoot, field, "");

			// field = "maxAttemptsNumberInCaseOfErrors";
			// if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			// 	maxAttemptsNumberInCaseOfErrors = 3;
			// else
			// 	maxAttemptsNumberInCaseOfErrors = JSONUtils::asInt(
			// 		parametersRoot, field, 0);

			field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
			waitingSecondsBetweenAttemptsInCaseOfErrors = JSONUtils::asInt64(parametersRoot, field, 5);

			field = "outputs";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = fmt::format(
					"Field is not present or it is null"
					", _processorIdentifier: {}"
					", field: {}",
					_processorIdentifier, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				outputsRoot = parametersRoot[field];
			else // if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs",
				 // false))
				outputsRoot = parametersRoot["Outputs"];
		}

		string useVideoTrackFromPhysicalPathName;
		string useVideoTrackFromPhysicalDeliveryURL;
		if (useVideoTrackFromMediaItemKey != -1)
		{
			int64_t sourcePhysicalPathKey;

			SPDLOG_INFO(
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
				int64_t utcNow;
				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNow = chrono::system_clock::to_time_t(now);
				}

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
		if (JSONUtils::isMetadataPresent(parametersRoot, "internalMMS") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"], "broadcaster") &&
			JSONUtils::isMetadataPresent(parametersRoot["internalMMS"]["broadcaster"], "broadcasterInputsRoot"))
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
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				filtersRoot = parametersRoot[field];

			json streamInputRoot = _mmsEngineDBFacade->getStreamInputRoot(
				workspace, ingestionJobKey, configurationLabel, useVideoTrackFromPhysicalPathName, useVideoTrackFromPhysicalDeliveryURL, maxWidth,
				userAgent, otherInputOptions, taskEncodersPoolLabel, filtersRoot
			);

			json inputRoot;
			{
				string field = "streamInput";
				inputRoot[field] = streamInputRoot;

				field = "timePeriod";
				inputRoot[field] = timePeriod;

				field = "utcScheduleStart";
				inputRoot[field] = utcProxyPeriodStart;

				field = "sUtcScheduleStart";
				inputRoot[field] = DateTime::utcToUtcString(utcProxyPeriodStart);

				field = "utcScheduleEnd";
				inputRoot[field] = utcProxyPeriodEnd;

				field = "sUtcScheduleEnd";
				inputRoot[field] = DateTime::utcToUtcString(utcProxyPeriodEnd);

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

		_mmsEngineDBFacade->addEncoding_LiveProxyJob(
			workspace, ingestionJobKey,
			inputsRoot,			 // used by FFMPEGEncoder
			streamSourceType,	 // used by FFMPEGEncoder
			utcProxyPeriodStart, // used in MMSEngineDBFacade::getEncodingJobs
			// maxAttemptsNumberInCaseOfErrors,	// used in
			// EncoderVideoAudioProxy.cpp
			waitingSecondsBetweenAttemptsInCaseOfErrors, // used in
														 // EncoderVideoAudioProxy.cpp
			localOutputsRoot,							 // used by FFMPEGEncoder
			_mmsWorkflowIngestionURL
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"manageLiveProxy failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what: {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw runtime_error(e.what());
	}
	catch (JsonFieldNotFound &e)
	{
		SPDLOG_ERROR(
			"manageLiveProxy failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what: {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw runtime_error(e.what());
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"manageLiveProxy failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what: {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"manageLiveProxy failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}",
			_processorIdentifier, ingestionJobKey
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
