
#include "MMSEngineProcessor.h"
#include "JSONUtils.h"
#include "catralibraries/DateTime.h"
/*
#include <stdio.h>

#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "FFMpeg.h"
#include "GEOInfoTimes.h"
#include "MMSCURL.h"
#include "PersistenceLock.h"
#include "ThreadsStatisticTimes.h"
#include "catralibraries/Convert.h"
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


void MMSEngineProcessor::manageCountdown(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, string ingestionDate, shared_ptr<Workspace> workspace,
	json parametersRoot, vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong media number for Countdown" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		bool defaultBroadcast = false;
		bool timePeriod = true;
		int64_t utcProxyPeriodStart = -1;
		int64_t utcProxyPeriodEnd = -1;
		{
			string field = "defaultBroadcast";
			defaultBroadcast = JSONUtils::asBool(parametersRoot, field, false);

			field = "schedule";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			// Validator validator(_logger, _mmsEngineDBFacade, _configuration);

			json proxyPeriodRoot = parametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);

			field = "end";
			if (!JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
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
					false,							 // save,
					"MMS_SignedToken",				 // deliveryType,

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

		json localOutputsRoot = getReviewedOutputsRoot(
			outputsRoot, workspace, ingestionJobKey, referenceContentType == MMSEngineDBFacade::ContentType::Image ? true : false
		);

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
			/* 2023-01-01: normalmente, nel caso di un semplica Task di
			   Countdown, il drawTextDetails viene inserito all'interno
			   dell'OutputRoot, Nel caso pero' di un Live Channel, per capire il
			   campo broadcastDrawTextDetails, vedi il commento all'interno del
			   metodo java CatraMMSBroadcaster::buildCountdownJsonForBroadcast
			*/
			json drawTextDetailsRoot = nullptr;

			string field = "broadcastDrawTextDetails";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				drawTextDetailsRoot = parametersRoot[field];

			// same json structure is used in
			// API_Ingestion::changeLiveProxyPlaylist
			json countdownInputRoot = _mmsEngineDBFacade->getCountdownInputRoot(
				mmsSourceVideoAssetPathName, mmsSourceVideoAssetDeliveryURL, sourcePhysicalPathKey, videoDurationInMilliSeconds, drawTextDetailsRoot
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

		// the only reason we may have a failure should be in case the image is
		// missing/removed
		long waitingSecondsBetweenAttemptsInCaseOfErrors = 30;
		long maxAttemptsNumberInCaseOfErrors = 3;

		_mmsEngineDBFacade->addEncoding_CountdownJob(
			workspace, ingestionJobKey, inputsRoot, utcProxyPeriodStart, localOutputsRoot, maxAttemptsNumberInCaseOfErrors,
			waitingSecondsBetweenAttemptsInCaseOfErrors, _mmsWorkflowIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageCountdown failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

