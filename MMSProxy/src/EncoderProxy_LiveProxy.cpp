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

#include "CurlWrapper.h"
#include "EncoderProxy.h"
#include "JSONUtils.h"
#include "MMSDeliveryAuthorization.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <FFMpegWrapper.h>
#include <cstdint>
#include <exception>
#include <regex>

using namespace std;
using json = nlohmann::json;

bool EncoderProxy::liveProxy(MMSEngineDBFacade::EncodingType encodingType)
{
	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	{
		json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];

		if (inputsRoot == nullptr || inputsRoot.size() == 0)
		{
			string errorMessage = std::format(
				"No inputsRoot are present"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", inputsRoot.size: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, inputsRoot.size()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			json firstInputRoot = inputsRoot[0];

			timePeriod = JSONUtils::as<bool>(firstInputRoot, "timePeriod", false);

			if (timePeriod)
				utcProxyPeriodStart = JSONUtils::as<int64_t>(firstInputRoot, "utcScheduleStart", -1);

			json lastInputRoot = inputsRoot[inputsRoot.size() - 1];

			timePeriod = JSONUtils::as<bool>(lastInputRoot, "timePeriod", false);

			if (timePeriod)
				utcProxyPeriodEnd = JSONUtils::as<int64_t>(lastInputRoot, "utcScheduleEnd", -1);
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
				LOG_INFO(
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
			string errorMessage = std::format(
				"Too late to activate the proxy"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", utcProxyPeriodEnd: {}"
				", utcNow: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcProxyPeriodEnd, utcNow
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	{
		json outputsRoot = (_encodingItem->_encodingParametersRoot)["outputsRoot"];

		bool killed = false;
		try
		{
			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::as<string>(outputRoot, "outputType", "");

				if (outputType == "RTMP_Channel")
				{
					// RtmpUrl fields have to be initialized

					string rtmpChannelConfigurationLabel = JSONUtils::as<string>(outputRoot, "rtmpChannelConfigurationLabel", "");

					// reserveRTMPChannel ritorna exception se non ci sono piu canali liberi o quello dedicato è già occupato
					// In caso di ripartenza di mmsEngine, nel caso di richiesta già attiva, ritornerebbe le stesse info
					// associate a ingestionJobKey (senza exception)
					auto [reservedLabel, rtmpURL, streamName, userName, password,
						channelAlreadyReserved, playURLDetailsRoot] = _mmsEngineDBFacade->reserveRTMPChannel(
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
						// nel caso di rtmp nativo (basato su libavformat/protocols/rtmp.c) non serve l'escape.
						// Ad esempio avevo una password con il '.' e, se fosse codificato, non funziona
						// rtmpURL.insert(7, (CurlWrapper::escape(userName) + ":" + CurlWrapper::escape(password) + "@"));
						rtmpURL.insert(7, (userName + ":" + password + "@"));
					}

					// update outputsRoot with the new details
					{
						outputRoot["rtmpUrl"] = rtmpURL;
						outputsRoot[outputIndex] = outputRoot;
						(_encodingItem->_encodingParametersRoot)["outputsRoot"] = outputsRoot;

						try
						{
							LOG_INFO(
								"updateOutputRtmpAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", rtmpChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", rtmpURL: {}"
								", channelAlreadyReserved: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, rtmpChannelConfigurationLabel, reservedLabel, rtmpURL, channelAlreadyReserved
							);

							_mmsEngineDBFacade->updateOutputURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, false, rtmpURL
							);
						}
						catch (exception &e)
						{
							LOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							throw;
						}
					}

					// channelAlreadyReserved true means the channel was already reserved, so it is supposed
					// is already started Maybe just start again is not an issue!!! Let's see
					if (!channelAlreadyReserved)
					{
						string cdnName = JSONUtils::as<string>(playURLDetailsRoot, "cdnName", "");
						if (cdnName == "aws")
						{
							json awsRoot = JSONUtils::as<json>(playURLDetailsRoot, "aws", json(nullptr));
							string awsChannelId = JSONUtils::as<string>(awsRoot, "channelId", "");
							if (!awsChannelId.empty())
								awsStartChannel(_encodingItem->_ingestionJobKey, awsChannelId);
						}
					}
				}
				else if (outputType == "SRT_Channel")
				{
					// SrtUrl and PlayUrl fields have to be initialized

					string srtChannelConfigurationLabel = JSONUtils::as<string>(outputRoot, "srtChannelConfigurationLabel", "");

					// reserveSRTChannel ritorna exception se non ci sono piu canali liberi o quello dedicato è già occupato
					// In caso di ripartenza di mmsEngine, nel caso di richiesta già attiva, ritornerebbe le stesse info
					// associate a ingestionJobKey (senza exception)
					auto [reservedLabel, srtURL, mode, streamId, passphrase, playURL, channelAlreadyReserved] = _mmsEngineDBFacade->reserveSRTChannel(
						_encodingItem->_workspace->_workspaceKey, srtChannelConfigurationLabel, outputIndex, _encodingItem->_ingestionJobKey
					);

					if (mode != "")
					{
						if (srtURL.find('?') == string::npos)
							srtURL += "?";
						else if (srtURL.back() != '?' && srtURL.back() != '&')
							srtURL += "&";

						srtURL += std::format("mode={}", mode);
					}
					if (streamId != "")
					{
						if (srtURL.find('?') == string::npos)
							srtURL += "?";
						else if (srtURL.back() != '?' && srtURL.back() != '&')
							srtURL += "&";

						srtURL += std::format("streamid={}", streamId);
					}
					if (passphrase != "")
					{
						if (srtURL.find('?') == string::npos)
							srtURL += "?";
						else if (srtURL.back() != '?' && srtURL.back() != '&')
							srtURL += "&";

						srtURL += std::format("passphrase={}", passphrase);
					}

					// update outputsRoot with the new details
					{
						outputRoot["srtUrl"] = srtURL;
						// outputRoot["playUrl"] = playURL;
						outputsRoot[outputIndex] = outputRoot;
						(_encodingItem->_encodingParametersRoot)["outputsRoot"] = outputsRoot;

						try
						{
							LOG_INFO(
								"updateOutputSrtAndPlaURL"
								", _proxyIdentifier: {}"
								", workspaceKey: {}"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", srtChannelConfigurationLabel: {}"
								", reservedLabel: {}"
								", srtURL: {}"
								", channelAlreadyReserved: {}"
								", playURL: {}",
								_proxyIdentifier, _encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey, srtChannelConfigurationLabel, reservedLabel, srtURL, channelAlreadyReserved, playURL
							);

							_mmsEngineDBFacade->updateOutputURL(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, outputIndex, true, srtURL
							);
						}
						catch (exception &e)
						{
							LOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							throw;
						}
					}
				}
				else if (outputType == "HLS_Channel")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string hlsChannelConfigurationLabel = JSONUtils::as<string>(outputRoot, "hlsChannelConfigurationLabel", "");

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

						string otherOutputOptions = JSONUtils::as<string>(outputRoot, "otherOutputOptions", "");

						outputsRoot[outputIndex] = outputRoot;

						(_encodingItem->_encodingParametersRoot)["outputsRoot"] = outputsRoot;

						try
						{
							LOG_INFO(
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
						catch (exception &e)
						{
							LOG_ERROR(
								"updateEncodingJobParameters failed"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", e.what(): {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
							);

							throw;
						}
					}
				}
			}

			killed = liveProxy_through_ffmpeg(encodingType);

			if (killed)
			{
				string errorMessage = std::format(
					"Encoding killed by the User"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);
				LOG_WARN(errorMessage);

				// the catch releaseXXXChannel
				throw EncodingKilledByUser();
			}

			for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				json outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::as<string>(outputRoot, "outputType", "");

				if (outputType == "RTMP_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						json playURLDetailsRoot = _mmsEngineDBFacade->releaseRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
						string cdnName = JSONUtils::as<string>(playURLDetailsRoot, "cdnName", "");
						if (cdnName == "aws")
						{
							json awsRoot = JSONUtils::as<json>(playURLDetailsRoot, "aws", json(nullptr));
							string awsChannelId = JSONUtils::as<string>(awsRoot, "channelId", "");
							if (!awsChannelId.empty())
								awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
						}
					}
					catch (...)
					{
						LOG_ERROR(
							"releaseRTMPChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
				else if (outputType == "SRT_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseSRTChannel(_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey);
					}
					catch (...)
					{
						LOG_ERROR(
							"releaseSRTChannel failed"
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
						LOG_ERROR(
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

				string outputType = JSONUtils::as<string>(outputRoot, "outputType", "");

				if (outputType == "RTMP_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						json playURLDetailsRoot = _mmsEngineDBFacade->releaseRTMPChannel(
							_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey
						);
						string cdnName = JSONUtils::as<string>(playURLDetailsRoot, "cdnName", "");
						if (cdnName == "aws")
						{
							json awsRoot = JSONUtils::as<json>(playURLDetailsRoot, "aws", json(nullptr));
							string awsChannelId = JSONUtils::as<string>(awsRoot, "channelId", "");
							if (!awsChannelId.empty())
								awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
						}
					}
					catch (...)
					{
						LOG_ERROR(
							"releaseRTMPChannel failed"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
						);
					}
				}
				else if (outputType == "SRT_Channel")
				{
					try
					{
						// error in case do not find ingestionJobKey
						_mmsEngineDBFacade->releaseSRTChannel(_encodingItem->_workspace->_workspaceKey, outputIndex, _encodingItem->_ingestionJobKey);
					}
					catch (...)
					{
						LOG_ERROR(
							"releaseSRTChannel failed"
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
						LOG_ERROR(
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

		return killed;
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
			string streamSourceType = JSONUtils::as<string>(proxyInputRoot, "streamSourceType", "");
			if (streamSourceType == "IP_PUSH")
				ipPushStreamConfigurationLabel = JSONUtils::as<string>(proxyInputRoot, "configurationLabel", "");
		}

		maxAttemptsNumberInCaseOfErrors = JSONUtils::as<int32_t>(_encodingItem->_ingestedParametersRoot, "maxAttemptsNumberInCaseOfErrors", -1);
		// 2022-07-20: this is to allow the next loop to exit after 2 errors
		if (maxAttemptsNumberInCaseOfErrors == -1)
			maxAttemptsNumberInCaseOfErrors = 2;

		{
			timePeriod = JSONUtils::as<bool>(firstInputRoot, "timePeriod", false);

			if (timePeriod)
				utcProxyPeriodStart = JSONUtils::as<int64_t>(firstInputRoot, "utcScheduleStart", -1);

			json lastInputRoot = inputsRoot[inputsRoot.size() - 1];

			timePeriod = JSONUtils::as<bool>(lastInputRoot, "timePeriod", false);

			if (timePeriod)
			{
				utcProxyPeriodEnd = JSONUtils::as<int64_t>(lastInputRoot, "utcScheduleEnd", -1);
				if (utcProxyPeriodEnd == -1)
					utcProxyPeriodEnd = JSONUtils::as<int64_t>(lastInputRoot, "utcProxyPeriodEnd", -1);
			}
		}
	}

	return waitingLiveProxyOrLiveRecorder(
		encodingType, _ffmpegLiveProxyURI, timePeriod, utcProxyPeriodStart, utcProxyPeriodEnd, maxAttemptsNumberInCaseOfErrors,
		ipPushStreamConfigurationLabel
	);
}

void EncoderProxy::processLiveProxy(bool killed)
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

			LOG_INFO(
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
		LOG_ERROR(
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
		LOG_ERROR(
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
