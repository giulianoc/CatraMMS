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
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <cstdint>
#include <regex>

bool EncoderProxy::liveProxy(MMSEngineDBFacade::EncodingType encodingType)
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
		time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

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

			killedByUser = liveProxy_through_ffmpeg(encodingType);

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

				// the catch releaseXXXChannel
				throw EncodingKilledByUser();
			}

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

bool EncoderProxy::liveProxy_through_ffmpeg(MMSEngineDBFacade::EncodingType encodingType)
{

	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	long maxAttemptsNumberInCaseOfErrors;
	string ipPushStreamConfigurationLabel;
	{
		json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];
		json firstInputRoot = inputsRoot[0];
		json proxyInputRoot;

		if (encodingType == MMSEngineDBFacade::EncodingType::VODProxy)
			proxyInputRoot = firstInputRoot["vodInput"];
		else if (encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
			proxyInputRoot = firstInputRoot["streamInput"];
		else if (encodingType == MMSEngineDBFacade::EncodingType::Countdown)
			proxyInputRoot = firstInputRoot["countdownInput"];

		if (encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
		{
			// se proxyType == "liveProxy" vuold dire che abbiamo uno Stream
			string streamSourceType = JSONUtils::asString(proxyInputRoot, "streamSourceType", "");
			if (streamSourceType == "IP_PUSH")
				ipPushStreamConfigurationLabel = JSONUtils::asString(proxyInputRoot, "configurationLabel", "");
		}

		maxAttemptsNumberInCaseOfErrors = JSONUtils::asInt(_encodingItem->_ingestedParametersRoot, "maxAttemptsNumberInCaseOfErrors", -1);
		// 2022-07-20: this is to allow the next loop to exit after 2 errors
		if (maxAttemptsNumberInCaseOfErrors == -1)
			maxAttemptsNumberInCaseOfErrors = 2;

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

	return waitingLiveProxyOrLiveRecorder(
		encodingType, _ffmpegLiveProxyURI, timePeriod, utcProxyPeriodStart, utcProxyPeriodEnd, maxAttemptsNumberInCaseOfErrors,
		ipPushStreamConfigurationLabel
	);
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
