/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   EnodingsManager.cpp
 * Author: giuliano
 *
 * Created on February 4, 2018, 7:18 PM
 */

#include "EncoderProxy.h"
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "MMSDeliveryAuthorization.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/DateTime.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <cstdint>
#include <regex>

bool EncoderProxy::liveProxy(string proxyType)
{

	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	{
		json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];

		if (inputsRoot == nullptr || inputsRoot.size() == 0)
		{
			string errorMessage = fmt::format(
				"No inputsRoot are present"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", inputsRoot.size: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, inputsRoot.size()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			json firstInputRoot = inputsRoot[0];

			timePeriod = JSONUtils::asBool(firstInputRoot, "timePeriod", false);

			if (timePeriod)
				utcProxyPeriodStart = JSONUtils::asInt64(firstInputRoot, "utcScheduleStart", -1);

			json lastInputRoot = inputsRoot[inputsRoot.size() - 1];

			timePeriod = JSONUtils::asBool(lastInputRoot, "timePeriod", false);

			if (timePeriod)
				utcProxyPeriodEnd = JSONUtils::asInt64(lastInputRoot, "utcScheduleEnd", -1);
		}
	}

	if (timePeriod)
	{
		time_t utcNow;

		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		if (utcNow < utcProxyPeriodStart)
		{
			// MMS allocates a thread just 5 minutes before the beginning of the recording
			if (utcProxyPeriodStart - utcNow >= _timeBeforeToPrepareResourcesInMinutes * 60)
			{
				SPDLOG_INFO(
					"Too early to allocate a thread for proxing"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", utcProyPeriodStart - utcNow: {}"
					", _timeBeforeToPrepareResourcesInSeconds: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcProxyPeriodStart - utcNow,
					_timeBeforeToPrepareResourcesInMinutes * 60
				);

				// it is simulated a MaxConcurrentJobsReached to avoid to increase the error counter
				throw MaxConcurrentJobsReached();
			}
		}

		if (utcProxyPeriodEnd <= utcNow)
		{
			string errorMessage = fmt::format(
				"Too late to activate the proxy"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", utcProxyPeriodEnd: {}"
				", utcNow: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcProxyPeriodEnd, utcNow
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	{
		json outputsRoot = (_encodingItem->_encodingParametersRoot)["outputsRoot"];

		bool killedByUser = false;
		try
		{
			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "CDN_AWS")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string awsChannelConfigurationLabel = JSONUtils::asString(outputRoot, "awsChannelConfigurationLabel", "");
					bool awsSignedURL = JSONUtils::asBool(outputRoot, "awsSignedURL", false);
					int awsExpirationInMinutes = JSONUtils::asInt(outputRoot, "awsExpirationInMinutes", 1440);

					// reserveAWSChannel ritorna exception se non ci sono piu canali liberi o quello dedicato è già occupato
					// In caso di ripartenza di mmsEngine, nel caso di richiesta già attiva, ritornerebbe le stesse info
					// associate a ingestionJobKey (senza exception)
					auto [awsChannelId, rtmpURL, playURL, channelAlreadyReserved] = _mmsEngineDBFacade->reserveAWSChannel(
						_encodingItem->_workspace->_workspaceKey, awsChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
					);

					if (awsSignedURL)
					{
						try
						{
							playURL = getAWSSignedURL(playURL, awsExpirationInMinutes);
						}
						catch (exception &ex)
						{
							SPDLOG_ERROR(
								"getAWSSignedURL failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", playURL: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, playURL
							);

							// throw e;
						}
					}

					// update outputsRoot with the new details
					{
						outputRoot["awsChannelConfigurationLabel"] = awsChannelConfigurationLabel;
						outputRoot["rtmpUrl"] = rtmpURL;
						outputRoot["playUrl"] = playURL;
						outputsRoot[outputIndex] = outputRoot;
						(_encodingItem->_encodingParametersRoot)["outputsRoot"] = outputsRoot;

						try
						{
							SPDLOG_INFO(
								"updateOutputRtmpAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", awsChannelConfigurationLabel: {}"
								", awsChannelId: {}"
								", rtmpURL: {}"
								", playURL: {}"
								", channelAlreadyReserved: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, awsChannelConfigurationLabel, awsChannelId, rtmpURL, playURL, channelAlreadyReserved
							);

							_mmsEngineDBFacade->updateOutputRtmpAndPlaURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, rtmpURL, playURL
							);
						}
						catch (runtime_error &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): ",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
							);

							// throw e;
						}
					}

					// channelAlreadyReserved true means the channel was already reserved, so it is supposed
					// is already started Maybe just start again is not an issue!!! Let's see
					if (!channelAlreadyReserved)
						awsStartChannel(_encodingItem->_ingestionJobKey, awsChannelId);
				}
				else if (outputType == "CDN_CDN77")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string cdn77ChannelConfigurationLabel = JSONUtils::asString(outputRoot, "cdn77ChannelConfigurationLabel", "");
					int cdn77ExpirationInMinutes = JSONUtils::asInt(outputRoot, "cdn77ExpirationInMinutes", 1440);

					// reserveCDN77Channel ritorna exception se non ci sono piu canali liberi o quello dedicato è già occupato
					// In caso di ripartenza di mmsEngine, nel caso di richiesta già attiva, ritornerebbe le stesse info
					// associate a ingestionJobKey (senza exception)
					auto [reservedLabel, rtmpURL, resourceURL, filePath, secureToken, channelAlreadyReserved] =
						_mmsEngineDBFacade->reserveCDN77Channel(
							_encodingItem->_workspace->_workspaceKey, cdn77ChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
						);

					if (filePath.size() > 0 && filePath.front() != '/')
						filePath = "/" + filePath;

					string playURL;
					if (secureToken != "")
					{
						try
						{
							playURL =
								MMSDeliveryAuthorization::getSignedCDN77URL(resourceURL, filePath, secureToken, cdn77ExpirationInMinutes, _logger);
						}
						catch (exception &ex)
						{
							SPDLOG_ERROR(
								"getSignedCDN77URL failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
							);

							// throw e;
						}
					}
					else
						playURL = "https://" + resourceURL + filePath;

					// update outputsRoot with the new details
					{
						outputRoot["rtmpUrl"] = rtmpURL;
						outputRoot["playUrl"] = playURL;
						outputsRoot[outputIndex] = outputRoot;
						(_encodingItem->_encodingParametersRoot)["outputsRoot"] = outputsRoot;

						try
						{
							SPDLOG_INFO(
								"updateOutputRtmpAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", cdn77ChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", rtmpURL: {}"
								", resourceURL: {}"
								", filePath: {}"
								", secureToken: {}"
								", channelAlreadyReserved: {}"
								", playURL: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, cdn77ChannelConfigurationLabel, reservedLabel, rtmpURL, resourceURL, filePath,
								secureToken, channelAlreadyReserved, playURL
							);

							_mmsEngineDBFacade->updateOutputRtmpAndPlaURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, rtmpURL, playURL
							);
						}
						catch (runtime_error &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
					}
				}
				else if (outputType == "RTMP_Channel")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string rtmpChannelConfigurationLabel = JSONUtils::asString(outputRoot, "rtmpChannelConfigurationLabel", "");

					// reserveRTMPChannel ritorna exception se non ci sono piu canali liberi o quello dedicato è già occupato
					// In caso di ripartenza di mmsEngine, nel caso di richiesta già attiva, ritornerebbe le stesse info
					// associate a ingestionJobKey (senza exception)
					auto [reservedLabel, rtmpURL, streamName, userName, password, playURL, channelAlreadyReserved] =
						_mmsEngineDBFacade->reserveRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, rtmpChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
						);

					if (streamName != "")
					{
						if (rtmpURL.back() == '/')
							rtmpURL += streamName;
						else
							rtmpURL += ("/" + streamName);
					}
					if (userName != "" && password != "")
					{
						// rtmp://.....
						rtmpURL.insert(7, (userName + ":" + password + "@"));
					}

					// update outputsRoot with the new details
					{
						outputRoot["rtmpUrl"] = rtmpURL;
						outputRoot["playUrl"] = playURL;
						outputsRoot[outputIndex] = outputRoot;
						(_encodingItem->_encodingParametersRoot)["outputsRoot"] = outputsRoot;

						try
						{
							SPDLOG_INFO(
								"updateOutputRtmpAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", rtmpChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", rtmpURL: {}"
								", channelAlreadyReserved: {}"
								", playURL: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, rtmpChannelConfigurationLabel, reservedLabel, rtmpURL, channelAlreadyReserved, playURL
							);

							_mmsEngineDBFacade->updateOutputRtmpAndPlaURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, rtmpURL, playURL
							);
						}
						catch (runtime_error &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
					}
				}
				else if (outputType == "HLS_Channel")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string hlsChannelConfigurationLabel = JSONUtils::asString(outputRoot, "hlsChannelConfigurationLabel", "");

					// reserveHLSChannel ritorna exception se non ci sono piu canali liberi o quello dedicato è già occupato
					// In caso di ripartenza di mmsEngine, nel caso di richiesta già attiva, ritornerebbe le stesse info
					// associate a ingestionJobKey (senza exception)
					auto [reservedLabel, deliveryCode, segmentDuration, playlistEntriesNumber, channelAlreadyReserved] =
						_mmsEngineDBFacade->reserveHLSChannel(
							_encodingItem->_workspace->_workspaceKey, hlsChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
						);

					// update outputsRoot with the new details
					{
						outputRoot["deliveryCode"] = deliveryCode;

						if (segmentDuration > 0) // if not present, default is decided by the encoder
							outputRoot["segmentDurationInSeconds"] = segmentDuration;

						if (playlistEntriesNumber > 0) // if not present, default is decided by the encoder
							outputRoot["playlistEntriesNumber"] = playlistEntriesNumber;

						string manifestDirectoryPath = _mmsStorage->getLiveDeliveryAssetPath(to_string(deliveryCode), _encodingItem->_workspace);
						string manifestFileName = to_string(deliveryCode) + ".m3u8";

						outputRoot["manifestDirectoryPath"] = manifestDirectoryPath;

						outputRoot["manifestFileName"] = manifestFileName;

						string otherOutputOptions = JSONUtils::asString(outputRoot, "otherOutputOptions", "");

						outputsRoot[outputIndex] = outputRoot;

						(_encodingItem->_encodingParametersRoot)["outputsRoot"] = outputsRoot;

						try
						{
							SPDLOG_INFO(
								"updateOutputHLSDetails"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", hlsChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", deliveryCode: {}"
								", segmentDuration: {}"
								", playlistEntriesNumber: {}"
								", manifestDirectoryPath: {}"
								", manifestFileName: {}"
								", channelAlreadyReserved: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, hlsChannelConfigurationLabel, reservedLabel, deliveryCode, segmentDuration,
								playlistEntriesNumber, manifestDirectoryPath, manifestFileName, channelAlreadyReserved
							);

							_mmsEngineDBFacade->updateOutputHLSDetails(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, deliveryCode, segmentDuration,
								playlistEntriesNumber, manifestDirectoryPath, manifestFileName, otherOutputOptions
							);
						}
						catch (runtime_error &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							// throw e;
						}
					}
				}
			}

			killedByUser = liveProxy_through_ffmpeg(proxyType);

			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "CDN_AWS")
				{
					try
					{
						// error in case do not find ingestionJobKey
						string awsChannelId = _mmsEngineDBFacade->releaseAWSChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);

						awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"releaseAWSChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
				else if (outputType == "CDN_CDN77")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseCDN77Channel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"releaseCDN77Channel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
				else if (outputType == "RTMP_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"releaseRTMPChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
				else if (outputType == "HLS_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseHLSChannel(_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey);
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"releaseHLSChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
			}

			if (killedByUser)
			{
				string errorMessage = fmt::format(
					"Encoding killed by the User"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);
				SPDLOG_WARN(errorMessage);

				throw EncodingKilledByUser();
			}
		}
		catch (...)
		{
			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "CDN_AWS")
				{
					try
					{
						// error in case do not find ingestionJobKey
						string awsChannelId = _mmsEngineDBFacade->releaseAWSChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);

						awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"releaseAWSChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
				else if (outputType == "CDN_CDN77")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseCDN77Channel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"releaseCDN77Channel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
				else if (outputType == "RTMP_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"releaseRTMPChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
				else if (outputType == "HLS_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseHLSChannel(_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey);
					}
					catch (...)
					{
						SPDLOG_ERROR(
							"releaseHLSChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
			}

			// throw the same received exception
			throw;
		}

		return killedByUser;
	}
}

bool EncoderProxy::liveProxy_through_ffmpeg(string proxyType)
{

	// se proxyType == "liveProxy" vuold dire che abbiamo uno Stream
	string streamUrl;
	string streamSourceType;

	// indica i secondi da attendere prima di tornare sull'encoder in caso di errore
	long waitingSecondsBetweenAttemptsInCaseOfErrors;
	// viene confrontato con currentAttemptsNumberInCaseOfErrors e viene usato in caso di !timePeriod
	long maxAttemptsNumberInCaseOfErrors;
	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];
	json firstInputRoot = inputsRoot[0];
	json proxyInputRoot;

	{
		string field;
		if (proxyType == "vodProxy")
			field = "vodInput";
		else if (proxyType == "liveProxy")
			field = "streamInput";
		else if (proxyType == "countdownProxy")
			field = "countdownInput";
		proxyInputRoot = firstInputRoot[field];

		{
			if (proxyType == "liveProxy")
			{
				streamUrl = JSONUtils::asString(proxyInputRoot, "url", "");

				streamSourceType = JSONUtils::asString(proxyInputRoot, "streamSourceType", "");
			}

			waitingSecondsBetweenAttemptsInCaseOfErrors =
				JSONUtils::asInt(_encodingItem->_encodingParametersRoot, "waitingSecondsBetweenAttemptsInCaseOfErrors", 600);

			maxAttemptsNumberInCaseOfErrors = JSONUtils::asInt(_encodingItem->_ingestedParametersRoot, "maxAttemptsNumberInCaseOfErrors", -1);

			{
				timePeriod = JSONUtils::asBool(firstInputRoot, "timePeriod", false);

				if (timePeriod)
					utcProxyPeriodStart = JSONUtils::asInt64(firstInputRoot, "utcScheduleStart", -1);

				json lastInputRoot = inputsRoot[inputsRoot.size() - 1];

				timePeriod = JSONUtils::asBool(lastInputRoot, "timePeriod", false);

				if (timePeriod)
				{
					utcProxyPeriodEnd = JSONUtils::asInt64(lastInputRoot, "utcScheduleEnd", -1);
					if (utcProxyPeriodEnd == -1)
						utcProxyPeriodEnd = JSONUtils::asInt64(lastInputRoot, "utcProxyPeriodEnd", -1);
				}
			}
		}
	}

	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;

	SPDLOG_INFO(
		"check maxAttemptsNumberInCaseOfErrors"
		", ingestionJobKey: {}"
		", encodingJobKey: {}"
		", maxAttemptsNumberInCaseOfErrors: {}",
		_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, maxAttemptsNumberInCaseOfErrors
	);

	// viene incrementato se completedWithError, EncoderNotFound, Encoding URL failed
	// viene resettato a 0 se !completedWithError
	long currentAttemptsNumberInCaseOfErrors = 0;

	if (maxAttemptsNumberInCaseOfErrors == -1)
	{
		// 2022-07-20: this is to allow the next loop to exit after 2 errors
		maxAttemptsNumberInCaseOfErrors = 2;

		try
		{
			SPDLOG_INFO(
				"updateEncodingJobFailuresNumber"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", currentAttemptsNumberInCaseOfErrors: {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
			);

			killedByUser = _mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
		}
		catch (...)
		{
			SPDLOG_ERROR(
				"updateEncodingJobFailuresNumber FAILED"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", currentAttemptsNumberInCaseOfErrors: {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
			);
		}
	}

	// 2020-03-11: we saw the following scenarios:
	//	1. ffmpeg was running
	//	2. after several hours it failed (1:34 am)
	//	3. our below loop tried again and this new attempt returned 404 URL NOT FOUND
	//	4. we exit from this loop
	//	5. crontab started again it after 15 minutes
	//	In this scenarios, we have to retry again without waiting the crontab check
	// 2020-03-12: Removing the urlNotFound management generated duplication of ffmpeg process
	//	For this reason we rollbacked as it was before
	// 2021-05-29: LiveProxy has to exit if:
	//	- was killed OR
	//	- if timePeriod true
	//		- no way to exit (we have to respect the timePeriod)
	//	- if timePeriod false
	//		- exit if too many error or urlForbidden or urlNotFound
	time_t utcNowCheckToExit = 0;
	while (! // while we are NOT in the exit condition
		   (
			   // exit condition
			   killedByUser ||
			   (!timePeriod && (urlForbidden || urlNotFound || currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors)) ||
			   (timePeriod && utcNowCheckToExit >= utcProxyPeriodEnd)
		   ))
	{
		if (timePeriod)
		{
			// if (utcNowCheckToExit >= utcProxyPeriodEnd)
			// 	break;
			// else
			SPDLOG_INFO(
				"check to exit"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", _encodingItem->_encoderKey: {}"
				", still miss (secs): {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_encoderKey, utcProxyPeriodEnd - utcNowCheckToExit
			);
		}

		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegLiveProxyURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			_currentUsedFFMpegExternalEncoder = false;

			if (_encodingItem->_encoderKey == -1)
			{
				// 2021-12-14: we have to read again encodingParametersRoot because, in case the playlist (inputsRoot) is changed, the
				// updated inputsRoot is into DB
				{
					try
					{
						MMSEngineDBFacade::IngestionStatus ingestionJobStatus;

						// 2022-12-18: fromMaster true because the inputsRoot maybe was just updated (modifying the playlist)
						_encodingItem->_encodingParametersRoot = _mmsEngineDBFacade->encodingJob_Parameters(_encodingItem->_encodingJobKey, true);

						// 2024-12-01: ricarichiamo ingestedParameters perchè potrebbe essere stato modificato con un nuovo 'encodersDetails' (nello
						// scenario in cui si vuole eseguire lo switch di un ingestionjob su un nuovo encoder)
						tie(ingestionJobStatus, _encodingItem->_ingestedParametersRoot) = _mmsEngineDBFacade->ingestionJob_StatusMetadataContent(
							_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, true
						);

						// viene controllato lo status perchè nello scenario in cui l'IngestionJob sia terminato e all'interno
						// di questo if viene generata una eccezione (ad es. EncoderNotFound nella call
						// _encodersLoadBalancer->getEncoderURL), si prosegue nel catch che ci riporta a inizio while
						// Entriamo quindi in un loop (inizio-while, eccezione EncoderNotFound, inizio-while) quando l'IngestionJob è terminato
						string sIngestionJobStatus = MMSEngineDBFacade::toString(ingestionJobStatus);
						if (sIngestionJobStatus.starts_with("End_"))
						{
							SPDLOG_INFO(
								"IngestionJob is terminated"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", ingestionJobStatus: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, sIngestionJobStatus
							);
							killedByUser = true;
							continue;
						}
					}
					catch (DBRecordNotFound &e)
					{
						SPDLOG_ERROR(
							"encodingJob_Parameters failed"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", e.what(): {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
						);

						throw e;
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							"encodingJob_Parameters failed"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", e.what(): {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
						);

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"encodingJob_Parameters failed"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", e.what(): {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
						);

						throw e;
					}
				}

				// IN ingestionJob->metadataParameters abbiamo già il campo encodersPool.
				// Aggiungiamo encoderKey nel caso di IP_PUSH in modo da avere un posto unico (ingestionJob->metadataParameters)
				// per questa informazione
				if (streamSourceType == "IP_PUSH")
				{
					// scenario:
					// 	- viene configurato uno Stream per un IP_PUSH su un encoder specifico
					// 	- questo encoder ha un fault e va giu
					// 	- finche questo encode non viene ripristinato (dipende da Hetzner) abbiamo un outage
					// 	- Per evitare l'outage, posso io cambiare encoder nella configurazione dello Stream
					// 	- La getStreamInputPushDetails sotto mi serve in questo loop per recuperare avere l'encoder aggiornato configurato nello
					// Stream 		altrimenti rimarremmo con l'encoder e l'url calcolata all'inizio e non potremmo evitare l'outage
					// 2024-06-25: In uno scenario di Broadcaster e Broadcast, il cambiamento descritto sopra
					// 		risolve il problema del broadcaster ma non quello del broadcast. Infatti il broadcast ha il campo udpUrl
					// 		nell'outputRoot che punta al transcoder iniziale. Questo campo udpUrl è stato inizializzato
					// 		in CatraMMSBroadcaster.java (method: addBroadcaster).

					int64_t updatedPushEncoderKey = -1;
					string updatedUrl;
					{
						// json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];

						// json streamInputRoot = inputsRoot[0]["streamInput"];

						// se è IP_PUSH vuol dire anche che siamo nel caso di proxyType == "liveProxy" che quindi ha il campo streamInput
						string streamConfigurationLabel = JSONUtils::asString(proxyInputRoot, "configurationLabel", "");

						json internalMMSRoot = JSONUtils::asJson(_encodingItem->_ingestedParametersRoot, "internalMMS", nullptr);
						json encodersDetailsRoot = JSONUtils::asJson(internalMMSRoot, "encodersDetails", nullptr);
						/*
						if (encodersDetailsRoot == nullptr)
						{
							// quando elimino questo if, verifica se anche la funzione getStreamInputPushDetails possa essere eliminata
							// per essere sostituita da getStreamPushServerUrl
							tie(updatedPushEncoderKey, updatedUrl) = _mmsEngineDBFacade->getStreamInputPushDetails(
								_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, streamConfigurationLabel
							);
						}
						else
						*/
						{
							// questo quello corretto, l'if sopra dovrebbe essere eliminato

							updatedPushEncoderKey = JSONUtils::asInt64(encodersDetailsRoot, "pushEncoderKey", static_cast<int64_t>(-1));
							if (updatedPushEncoderKey == -1)
							{
								string errorMessage = fmt::format(
									"Wrong pushEncoderKey"
									", _ingestionJobKey: {}"
									", _encodingJobKey: {}"
									", encodersDetailsRoot: {}",
									_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, JSONUtils::toString(encodersDetailsRoot)
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}

							bool pushPublicEncoderName = JSONUtils::asBool(encodersDetailsRoot, "pushPublicEncoderName", false);

							updatedUrl = _mmsEngineDBFacade->getStreamPushServerUrl(
								_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, streamConfigurationLabel,
								updatedPushEncoderKey, pushPublicEncoderName, true
							);
						}
					}

					proxyInputRoot["pushEncoderKey"] = updatedPushEncoderKey;
					proxyInputRoot["url"] = updatedUrl;
					inputsRoot[0]["streamInput"] = proxyInputRoot;
					(_encodingItem->_encodingParametersRoot)["inputsRoot"] = inputsRoot;

					_currentUsedFFMpegEncoderKey = updatedPushEncoderKey;
					// 2023-12-18: pushEncoderName è importante che sia usato
					// nella url rtmp
					//	dove il transcoder ascolta per il flusso di streaming
					//	ma non deve essere usato per decidere l'url con cui
					// l'engine deve comunicare 	con il transcoder. Questa url
					// dipende solamente dal fatto che il transcoder 	sia interno
					// o esterno
					tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) =
						_mmsEngineDBFacade->getEncoderURL(updatedPushEncoderKey); // , pushEncoderName);

					SPDLOG_INFO(
						"LiveProxy. Retrieved updated Stream info"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", updatedPushEncoderKey: {}"
						", updatedUrl: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, updatedPushEncoderKey, updatedUrl
					);
				}
				else
				{
					string encodersPool;
					if (proxyType == "vodProxy" || proxyType == "countdownProxy")
					{
						// both vodProxy and countdownProxy work with VODs and
						// the encodersPool is defined by the ingestedParameters field
						encodersPool = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, "encodersPool", "");
					}
					else
					{
						json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];

						json streamInputRoot = inputsRoot[0]["streamInput"];

						json internalMMSRoot = JSONUtils::asJson(_encodingItem->_ingestedParametersRoot, "internalMMS", nullptr);
						json encodersDetailsRoot = JSONUtils::asJson(internalMMSRoot, "encodersDetails", nullptr);
						if (encodersDetailsRoot == nullptr)
							encodersPool = JSONUtils::asString(streamInputRoot, "encodersPoolLabel", "");
						else // questo quello corretto, l'if sopra dovrebbe essere eliminato
							encodersPool = JSONUtils::asString(encodersDetailsRoot, "encodersPoolLabel", string());
					}

					int64_t encoderKeyToBeSkipped = -1;
					bool externalEncoderAllowed = true;
					tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) =
						_encodersLoadBalancer->getEncoderURL(
							_encodingItem->_ingestionJobKey, encodersPool, _encodingItem->_workspace, encoderKeyToBeSkipped, externalEncoderAllowed
						);
				}

				SPDLOG_INFO(
					"Configuration item"
					", _proxyIdentifier: {}"
					", _currentUsedFFMpegEncoderHost: {}"
					", _currentUsedFFMpegEncoderKey: {}",
					_proxyIdentifier, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
				);
				ffmpegEncoderURL = fmt::format(
					"{}{}/{}/{}", _currentUsedFFMpegEncoderHost, ffmpegURI, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);

				string body;
				{
					json liveProxyMetadata;

					// 2023-03-21: rimuovere il parametro ingestionJobKey se il
					// trascoder deployed è > 1.0.5315
					liveProxyMetadata["ingestionJobKey"] = _encodingItem->_ingestionJobKey;
					liveProxyMetadata["externalEncoder"] = _currentUsedFFMpegExternalEncoder;
					liveProxyMetadata["liveURL"] = streamUrl;
					liveProxyMetadata["ingestedParametersRoot"] = _encodingItem->_ingestedParametersRoot;
					liveProxyMetadata["encodingParametersRoot"] = _encodingItem->_encodingParametersRoot;

					body = JSONUtils::toString(liveProxyMetadata);
				}

				SPDLOG_INFO(
					"LiveProxy. Selection of the transcoder. The transcoder is selected by load balancer"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", transcoder: {}"
					", _currentUsedFFMpegEncoderKey: {}"
					", body: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost,
					_currentUsedFFMpegEncoderKey, body
				);

				vector<string> otherHeaders;
				json liveProxyContentResponse;
				try
				{
					liveProxyContentResponse = MMSCURL::httpPostStringAndGetJson(
						_logger, _encodingItem->_ingestionJobKey, ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, _ffmpegEncoderUser,
						_ffmpegEncoderPassword, body,
						"application/json", // contentType
						otherHeaders
					);
				}
				catch (runtime_error &e)
				{
					string error = e.what();
					if (error.find(EncodingIsAlreadyRunning().what()) != string::npos)
					{
						// 2023-03-26:
						// Questo scenario indica che per il DB "l'encoding è da eseguire" mentre abbiamo un Encoder che lo sta già
						// eseguendo Si tratta di una inconsistenza che non dovrebbe mai accadere. Oggi pero' ho visto questo
						// scenario e l'ho risolto facendo ripartire sia l'encoder che gli engines Gestire questo scenario
						// rende il sistema piu' robusto e recupera facilmente una situazione che altrimenti richiederebbe una
						// gestione manuale Inoltre senza guardare nel log, non si riuscirebbe a capire che siamo in questo scenario.

						// La gestione di questo scenario consiste nell'ignorare questa eccezione facendo andare avanti la procedura,
						// come se non avesse generato alcun errore
						_logger->error(
							__FILEREF__ +
							"inconsistency: DB says the encoding has to be "
							"executed but the Encoder is already executing it. "
							"We will manage it" +
							", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " +
							to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
							", body: " + regex_replace(body, regex("\n"), " ") + ", e.what: " + e.what()
						);
					}
					else
						throw e;
				}
			}
			else
			{
				SPDLOG_INFO(
					"LiveProxy. Selection of the transcoder. The transcoder is already saved (DB), the encoding is already running"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", encoderKey: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_encoderKey
				);

				tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;

				// we have to reset _encodingItem->_encoderKey because in case we will come back in the above 'while' loop, we have to
				// select another encoder
				_encodingItem->_encoderKey = -1;

				ffmpegEncoderURL = fmt::format("{}{}/{}", _currentUsedFFMpegEncoderHost, ffmpegURI, _encodingItem->_encodingJobKey);
			}

			chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

			{
				lock_guard<mutex> locker(*_mtEncodingJobs);

				*_status = EncodingJobStatus::Running;
			}

			SPDLOG_INFO(
				"Update EncodingJob"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", transcoder: {}"
				", _currentUsedFFMpegEncoderKey: {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, "");

			if (timePeriod)
				;
			else
			{
				try
				{
					double encodingProgress = -1.0; // it is a live

					SPDLOG_INFO(
						"updateEncodingJobProgress"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", encodingProgress: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingProgress
					);
					_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, encodingProgress);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"updateEncodingJobProgress failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"updateEncodingJobProgress failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
			}

			// loop waiting the end of the encoding
			bool encodingFinished = false;
			bool completedWithError = false;
			string encodingErrorMessage;
			chrono::system_clock::time_point startCheckingEncodingStatus = chrono::system_clock::now();

			int encoderNotReachableFailures = 0;
			int lastEncodingPid = 0;
			long lastRealTimeFrameRate = 0;
			double lastRealTimeBitRate = 0;
			int encodingPid;
			long realTimeFrameRate;
			double realTimeBitRate;
			long lastNumberOfRestartBecauseOfFailure = 0;
			long numberOfRestartBecauseOfFailure;

			SPDLOG_INFO(
				"starting loop"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", encodingFinished: {}"
				", encoderNotReachableFailures: {}"
				", _maxEncoderNotReachableFailures: {}"
				", currentAttemptsNumberInCaseOfErrors: {}"
				", maxAttemptsNumberInCaseOfErrors: {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingFinished, encoderNotReachableFailures,
				_maxEncoderNotReachableFailures, currentAttemptsNumberInCaseOfErrors, maxAttemptsNumberInCaseOfErrors
			);

			/*
			 Questo loop server per gestire e controllare l'encoding running nell'Encoder.
			 La condizione di uscita quindi dovrebbe basarsi solamente su encodingFinished ma ci sono dei casi come
			 ad esempio 'EncoderNotReachable' in cui è bene aspettare un po prima di abbandonare l'encoding per
			 essere sicuri che l'encoding sia effettivamente terminato nell'Encoder.
			 Non vogliamo quindi abbandonare l'encoding ed eventualmente attivare un nuovo encoding su un'altro
			 Encoder se non siamo sicurissimi che l'encoding attuale sia terminato.
			*/
			while (!(encodingFinished || encoderNotReachableFailures >= _maxEncoderNotReachableFailures))
			{
				SPDLOG_INFO(
					"sleep_for"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", _intervalInSecondsToCheckEncodingFinished: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _intervalInSecondsToCheckEncodingFinished
				);
				this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

				try
				{
					tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage, urlForbidden, urlNotFound, ignore, encodingPid,
						realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure) = getEncodingStatus();
					SPDLOG_INFO(
						"getEncodingStatus"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", currentAttemptsNumberInCaseOfErrors: {}"
						", maxAttemptsNumberInCaseOfErrors: {}"
						", encodingFinished: {}"
						", killedByUser: {}"
						", completedWithError: {}"
						", encodingErrorMessage: {}"
						", urlForbidden: {}"
						", urlNotFound: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors,
						maxAttemptsNumberInCaseOfErrors, encodingFinished, killedByUser, completedWithError, encodingErrorMessage, urlForbidden,
						urlNotFound
					);
				}
				catch (EncoderNotReachable &e)
				{
					encoderNotReachableFailures++;

					// 2020-11-23. Scenario:
					//	1. I shutdown the encoder because I had to upgrade OS version
					//	2. this thread remained in this loop (while(!encodingFinished)) 		and the channel did not work
					// until the Encoder was working again 	In this scenario, so when the encoder is not reachable at all, the engine 	has
					// to select a new encoder. 	For this reason we added this EncoderNotReachable catch 	and the
					// encoderNotReachableFailures variable

					SPDLOG_ERROR(
						"Transcoder is not reachable at all, if continuing we will select another encoder"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", encoderNotReachableFailures: {}"
						", _maxEncoderNotReachableFailures: {}"
						", _currentUsedFFMpegEncoderHost: {}"
						", _currentUsedFFMpegEncoderKey: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encoderNotReachableFailures, _maxEncoderNotReachableFailures,
						_currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
					);

					continue;
				}
				catch (...)
				{
					encoderNotReachableFailures++;

					SPDLOG_ERROR(
						"getEncodingStatus failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", encoderNotReachableFailures: {}"
						", _maxEncoderNotReachableFailures: {}"
						", _currentUsedFFMpegEncoderHost: {}"
						", _currentUsedFFMpegEncoderKey: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encoderNotReachableFailures, _maxEncoderNotReachableFailures,
						_currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
					);

					continue;
				}

				try
				{
					// resetto encoderNotReachableFailures a 0. Continue 'exception' incrementano questa variabile
					// ma la prima volta in cui non abbiamo l'exception viene resettata a 0
					encoderNotReachableFailures = 0;

					// update encodingProgress/encodingPid/real time info
					{
						if (timePeriod)
						{
							time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

							double encodingProgress;

							if (utcNow < utcProxyPeriodStart)
								encodingProgress = 0.0;
							else if (utcProxyPeriodStart < utcNow && utcNow < utcProxyPeriodEnd)
							{
								double elapsed = utcNow - utcProxyPeriodStart;
								double proxyPeriod = utcProxyPeriodEnd - utcProxyPeriodStart;
								encodingProgress = (elapsed * 100) / proxyPeriod;
							}
							else
								encodingProgress = 100.0;

							SPDLOG_INFO(
								"updateEncodingJobProgress"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", encodingProgress: {}"
								", utcProxyPeriodStart: {}"
								", utcNow: {}"
								", utcProxyPeriodEnd: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingProgress, utcProxyPeriodStart, utcNow,
								utcProxyPeriodEnd
							);
							_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, encodingProgress);
						}

						if (lastEncodingPid != encodingPid || lastRealTimeFrameRate != realTimeFrameRate || lastRealTimeBitRate != realTimeBitRate ||
							lastNumberOfRestartBecauseOfFailure != numberOfRestartBecauseOfFailure)
						{
							SPDLOG_INFO(
								"updateEncodingRealTimeInfo"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", encodingPid: {}"
								", realTimeFrameRate: {}"
								", realTimeBitRate: {}"
								", numberOfRestartBecauseOfFailure: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate,
								numberOfRestartBecauseOfFailure
							);
							_mmsEngineDBFacade->updateEncodingRealTimeInfo(
								_encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure
							);

							lastEncodingPid = encodingPid;
							lastRealTimeFrameRate = realTimeFrameRate;
							lastRealTimeBitRate = realTimeBitRate;
							lastNumberOfRestartBecauseOfFailure = numberOfRestartBecauseOfFailure;
						}
						else
						{
							SPDLOG_INFO(
								"encoderPid/bitrate/framerate check, not changed"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", encodingPid: {}"
								", realTimeFrameRate: {}"
								", realTimeBitRate: {}"
								", numberOfRestartBecauseOfFailure: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate,
								numberOfRestartBecauseOfFailure
							);
						}
					}

					if (encodingErrorMessage != "")
					{
						SPDLOG_ERROR(
							"Encoding failed (look the Transcoder logs)"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", _currentUsedFFMpegEncoderHost: {}"
							", completedWithError: {}"
							", encodingErrorMessage: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost, completedWithError,
							encodingErrorMessage
						);

						string firstLineOfEncodingErrorMessage;
						{
							string firstLine;
							stringstream ss(encodingErrorMessage);
							if (getline(ss, firstLine))
								firstLineOfEncodingErrorMessage = firstLine;
							else
								firstLineOfEncodingErrorMessage = encodingErrorMessage;
						}

						_mmsEngineDBFacade->appendIngestionJobErrorMessage(_encodingItem->_ingestionJobKey, firstLineOfEncodingErrorMessage);
					}

					// update currentAttemptsNumberInCaseOfErrors++
					{
						if (completedWithError)
							currentAttemptsNumberInCaseOfErrors++;
						else
							// ffmpeg is running successful, we will make sure currentAttemptsNumberInCaseOfErrors is reset
							currentAttemptsNumberInCaseOfErrors = 0;

						// update EncodingJob failures number to notify the GUI encodingJob is successful

						SPDLOG_INFO(
							"updateEncodingJobFailuresNumber"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", currentAttemptsNumberInCaseOfErrors: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
						);

						int64_t mediaItemKey = -1;
						int64_t encodedPhysicalPathKey = -1;
						_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
					}

					if (!killedByUser)
					{
						// secondo l'encoder l'encoding non è stato killato. Eseguo per essere sicuro anche
						// una verifica recuperando lo stato dell'IngestionJob
						string ingestionStatus = MMSEngineDBFacade::toString(
							_mmsEngineDBFacade->ingestionJob_Status(_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, true)
						);
						if (ingestionStatus.starts_with("End_"))
						{
							SPDLOG_INFO(
								"getEncodingStatus killedByUser is false but the ingestionJob is terminated"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", killedByUser: {}"
								", ingestionStatus: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, killedByUser, ingestionStatus
							);
							encodingFinished = true;
							killedByUser = true;
						}
					}
					if (!encodingFinished && (killedByUser || urlForbidden || urlNotFound))
						SPDLOG_ERROR(
							"Encoding was killedByUser or urlForbidden or urlNotFound but encodingFinished is false!!!"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", _currentUsedFFMpegEncoderHost: {}"
							", killedByUser: {}"
							", urlForbidden: {}"
							", urlNotFound: {}"
							", completedWithError: {}"
							", encodingErrorMessage: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost, killedByUser,
							urlForbidden, urlNotFound, completedWithError, encodingErrorMessage
						);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"management of the getEncodingJob result failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"management of the getEncodingJob result failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
			}

			chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
			utcNowCheckToExit = chrono::system_clock::to_time_t(endEncoding);

			if (!timePeriod || (timePeriod && utcNowCheckToExit < utcProxyPeriodEnd))
			{
				SPDLOG_ERROR(
					"LiveProxy media file completed unexpected"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", still remaining seconds (utcProxyPeriodEnd - utcNow): {}"
					", ffmpegEncoderURL: {}"
					", encodingFinished: {}"
					", killedByUser: {}"
					", @MMS statistics@ - encodingDuration (secs): @{}@"
					", _intervalInSecondsToCheckEncodingFinished: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcProxyPeriodEnd - utcNowCheckToExit,
					ffmpegEncoderURL, encodingFinished, killedByUser, chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count(),
					_intervalInSecondsToCheckEncodingFinished
				);

				try
				{
					string errorMessage = DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())) +
										  " LiveProxy media file completed unexpected";

					string firstLineOfErrorMessage;
					{
						string firstLine;
						stringstream ss(errorMessage);
						if (getline(ss, firstLine))
							firstLineOfErrorMessage = firstLine;
						else
							firstLineOfErrorMessage = errorMessage;
					}

					_mmsEngineDBFacade->appendIngestionJobErrorMessage(_encodingItem->_ingestionJobKey, firstLineOfErrorMessage);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"appendIngestionJobErrorMessage failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"appendIngestionJobErrorMessage failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
			}
			else
			{
				SPDLOG_INFO(
					"LiveProxy media file completed"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", ffmpegEncoderURL: {}"
					", encodingFinished: {}"
					", killedByUser: {}"
					", @MMS statistics@ - encodingDuration (secs): @{}@"
					", _intervalInSecondsToCheckEncodingFinished: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL, encodingFinished,
					killedByUser, chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count(),
					_intervalInSecondsToCheckEncodingFinished
				);
			}
		}
		catch (YouTubeURLNotRetrieved &e)
		{
			SPDLOG_ERROR(
				"YouTubeURLNotRetrieved"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", response.str: {}"
				", e.what(): {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, responseInitialized ? response.str() : "", e.what()
			);

			// in this case we will through the exception independently if the live streaming time (utcRecordingPeriodEnd)
			// is finished or not. This task will come back by the MMS system
			throw e;
		}
		catch (EncoderNotFound e)
		{
			SPDLOG_ERROR(
				"Encoder not found"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", ffmpegEncoderURL: {}"
				", response.str: {}"
				", e.what(): {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL,
				responseInitialized ? response.str() : "", e.what()
			);

			// update EncodingJob failures number to notify the GUI EncodingJob
			// is failing
			try
			{
				// 2021-02-12: scenario, encodersPool does not exist, a runtime_error is generated contiuosly. The task will never
				// exist from this loop because currentAttemptsNumberInCaseOfErrors always remain to 0 and
				// the main loop look currentAttemptsNumberInCaseOfErrors. So added currentAttemptsNumberInCaseOfErrors++
				currentAttemptsNumberInCaseOfErrors++;

				SPDLOG_INFO(
					"updateEncodingJobFailuresNumber"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", currentAttemptsNumberInCaseOfErrors: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
			}
			catch (...)
			{
				SPDLOG_ERROR(
					"updateEncodingJobFailuresNumber failed"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
			}

			// throw e;
		}
		catch (MaxConcurrentJobsReached &e)
		{
			SPDLOG_WARN(
				"MaxConcurrentJobsReached"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", response.str: {}"
				", e.what(): {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, responseInitialized ? response.str() : "", e.what()
			);

			// in this case we will through the exception independently
			// if the live streaming time (utcRecordingPeriodEnd)
			// is finished or not. This task will come back by the MMS system
			throw e;
		}
		catch (runtime_error e)
		{
			string error = e.what();
			if (error.find(NoEncodingAvailable().what()) != string::npos)
			{
				SPDLOG_WARN(
					"No Encodings available / MaxConcurrentJobsReached"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", e.what(): {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
				);

				throw MaxConcurrentJobsReached();
			}
			else
			{
				SPDLOG_ERROR(
					"Encoding URL failed/runtime_error"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", ffmpegEncoderURL: {}"
					", response.str: {}"
					", e.what(): {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL,
					responseInitialized ? response.str() : "", e.what()
				);

				// update EncodingJob failures number to notify the GUI EncodingJob is failing
				try
				{
					// 2021-02-12: scenario, encodersPool does not exist, a runtime_error is generated contiuosly. The task will
					// never exist from this loop because currentAttemptsNumberInCaseOfErrors always remain to 0
					// and the main loop look currentAttemptsNumberInCaseOfErrors. So added currentAttemptsNumberInCaseOfErrors++
					currentAttemptsNumberInCaseOfErrors++;

					SPDLOG_INFO(
						"updateEncodingJobFailuresNumber"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", currentAttemptsNumberInCaseOfErrors: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
					);

					int64_t mediaItemKey = -1;
					int64_t encodedPhysicalPathKey = -1;
					_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
				}
				catch (...)
				{
					SPDLOG_ERROR(
						"updateEncodingJobFailuresNumber failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
					);
				}

				// sleep a bit and try again
				int sleepTime = 30;
				this_thread::sleep_for(chrono::seconds(sleepTime));

				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNowCheckToExit = chrono::system_clock::to_time_t(now);
				}

				// throw e;
			}
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"Encoding URL failed (exception)"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", ffmpegEncoderURL: {}"
				", response.str: {}"
				", e.what(): {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL,
				responseInitialized ? response.str() : "", e.what()
			);

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				currentAttemptsNumberInCaseOfErrors++;
				// encodingStatusFailures++;

				SPDLOG_INFO(
					"updateEncodingJobFailuresNumber"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", currentAttemptsNumberInCaseOfErrors: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
			}
			catch (...)
			{
				SPDLOG_ERROR(
					"updateEncodingJobFailuresNumber failed"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
			}

			// throw e;
		}
	}

	if (urlForbidden)
	{
		SPDLOG_ERROR(
			"LiveProxy: URL forbidden"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
		);

		throw FFMpegURLForbidden();
	}
	else if (urlNotFound)
	{
		SPDLOG_ERROR(
			"LiveProxy: URL Not Found"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
		);

		throw FFMpegURLNotFound();
	}
	else if (currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors)
	{
		SPDLOG_ERROR(
			"Reached the max number of attempts to the URL"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", currentAttemptsNumberInCaseOfErrors: {}"
			", maxAttemptsNumberInCaseOfErrors: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors,
			maxAttemptsNumberInCaseOfErrors
		);

		throw EncoderError();
	}

	return killedByUser;
}

void EncoderProxy::processLiveProxy(bool killedByUser)
{
	try
	{
		// In case of Liveproxy where TimePeriod is false, this method is never
		// called because,
		//	in both the scenarios below, an exception by
		// EncoderProxy::liveProxy will be raised:
		// - transcoding killed by the user
		// - The max number of calls to the URL were all done and all failed
		//
		// In case of LiveProxy where TimePeriod is true, this method is called

		// Status will be success if at least one Chunk was generated, otherwise
		// it will be failed
		{
			string errorMessage;
			string processorMMS;
			MMSEngineDBFacade::IngestionStatus newIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

			SPDLOG_INFO(
				"Update IngestionJob"
				", ingestionJobKey: {}"
				", IngestionStatus: {}"
				", errorMessage: {}"
				", processorMMS: {}",
				_encodingItem->_ingestionJobKey, MMSEngineDBFacade::toString(newIngestionStatus), errorMessage, processorMMS
			);
			_mmsEngineDBFacade->updateIngestionJob(_encodingItem->_ingestionJobKey, newIngestionStatus, errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"processLiveProxy failed"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _workspace->_directoryName: {}"
			", e.what: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"processLiveProxy failed"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _workspace->_directoryName: {}"
			", e.what: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_workspace->_directoryName, e.what()
		);

		throw e;
	}
}
