
#include "LiveProxy.h"

#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"

LiveProxy::LiveProxy(
	const shared_ptr<LiveProxyAndGrid> &liveProxyData, const json &configurationRoot,
	mutex *encodingCompletedMutex, map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap, mutex *tvChannelsPortsMutex,
	long *tvChannelPort_CurrentOffset
)
	: FFMPEGEncoderTask(liveProxyData, configurationRoot, encodingCompletedMutex, encodingCompletedMap)
{
	_encoding = liveProxyData;
	_tvChannelsPortsMutex = tvChannelsPortsMutex;
	_tvChannelPort_CurrentOffset = tvChannelPort_CurrentOffset;
};

void LiveProxy::encodeContent(const string_view& requestBody)
{
	string api = "liveProxy";

	shared_ptr<LiveProxyAndGrid> liveProxyData = dynamic_pointer_cast<LiveProxyAndGrid>(_encoding);

	SPDLOG_INFO(
		"Received {}"
		", _ingestionJobKey: {}"
		", _encodingJobKey: {}"
		", requestBody: {}",
		api, liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey, requestBody
	);

	try
	{
		liveProxyData->_killedBecauseOfNotWorking = false;
		json metadataRoot = JSONUtils::toJson(requestBody);

		liveProxyData->_encodingParametersRoot = metadataRoot["encodingParametersRoot"];
		liveProxyData->_ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];
		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];

		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);

		long maxStreamingDurationInMinutes = JSONUtils::asInt64(liveProxyData->_ingestedParametersRoot, "maxStreamingDurationInMinutes", -1);

		liveProxyData->_monitoringRealTimeInfoEnabled =
			JSONUtils::asBool(liveProxyData->_ingestedParametersRoot, "monitoringFrameIncreasingEnabled", true);
		liveProxyData->_outputFfmpegFileSize = 0;
		liveProxyData->_realTimeFrame = -1;
		liveProxyData->_realTimeSize = -1;
		liveProxyData->_realTimeFrameRate = -1;
		liveProxyData->_realTimeBitRate = -1;
		liveProxyData->_realTimeTimeInMilliSeconds = -1.0;
		liveProxyData->_realTimeLastChange = chrono::system_clock::now();

		// 0 perchè liveProxy2 incrementa su un restart
		liveProxyData->_numberOfRestartBecauseOfFailure = 0;

		liveProxyData->_outputsRoot = encodingParametersRoot["outputsRoot"];
		{
			for (int outputIndex = 0; outputIndex < liveProxyData->_outputsRoot.size(); outputIndex++)
			{
				json outputRoot = liveProxyData->_outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				// if (outputType == "HLS" || outputType == "DASH")
				if (outputType == "HLS_Channel")
				{
					string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");

					if (fs::exists(manifestDirectoryPath))
					{
						try
						{
							SPDLOG_INFO(
								"removeDirectory"
								", manifestDirectoryPath: {}",
								manifestDirectoryPath
							);
							fs::remove_all(manifestDirectoryPath);
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"remove directory failed"
								", ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", manifestDirectoryPath: {}"
								", e.what(): {}",
								liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey, manifestDirectoryPath, e.what()
							);

							// throw;
						}
					}
				}
			}
		}

		liveProxyData->_inputsRoot = encodingParametersRoot["inputsRoot"];

		for (int inputIndex = 0; inputIndex < liveProxyData->_inputsRoot.size(); inputIndex++)
		{
			json inputRoot = liveProxyData->_inputsRoot[inputIndex];

			if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
				continue;
			json streamInputRoot = inputRoot["streamInput"];

			string streamSourceType = JSONUtils::asString(streamInputRoot, "streamSourceType", "");
			if (streamSourceType == "TV")
			{
				string tvType = JSONUtils::asString(streamInputRoot, "tvType", "");
				int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
				int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
				int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
				int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
				string tvModulation = JSONUtils::asString(streamInputRoot, "tvModulation", "");
				int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
				int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot, "tvAudioItalianPid", -1);

				// In case ffmpeg crashes and is automatically restarted, it should use the same
				// IP-PORT it was using before because we already have a dbvlast sending the stream
				// to the specified IP-PORT.
				// For this reason, before to generate a new IP-PORT, let's look for the serviceId
				// inside the dvblast conf. file to see if it was already running before

				string tvMulticastIP;
				string tvMulticastPort;

				// in case there is already a serviceId running, we will use the same multicastIP-Port
				pair<string, string> tvMulticast = getTVMulticastFromDvblastConfigurationFile(
					liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey, tvType, tvServiceId, tvFrequency, tvSymbolRate, tvBandwidthInHz / 1000000,
					tvModulation
				);
				tie(tvMulticastIP, tvMulticastPort) = tvMulticast;

				if (tvMulticastIP.empty())
				{
					*_tvChannelPort_CurrentOffset = getFreeTvChannelPortOffset(_tvChannelsPortsMutex, *_tvChannelPort_CurrentOffset);

					tvMulticastIP = "239.255.1.1";
					tvMulticastPort = to_string(*_tvChannelPort_CurrentOffset + _tvChannelPort_Start);

					/*
					lock_guard<mutex> locker(*_tvChannelsPortsMutex);

					tvMulticastIP = "239.255.1.1";
					tvMulticastPort = to_string(*_tvChannelPort_CurrentOffset
						+ _tvChannelPort_Start);

					*_tvChannelPort_CurrentOffset = (*_tvChannelPort_CurrentOffset + 1)
						% _tvChannelPort_MaxNumberOfOffsets;
					*/
				}

				// overrun_nonfatal=1 prevents ffmpeg from exiting,
				//		it can recover in most circumstances.
				// fifo_size=50000000 uses a 50MB udp input buffer (default 5MB)
				string newURL = std::format("udp://@{}:{}", tvMulticastIP, tvMulticastPort);
					// 2022-12-08: the below parameters are added inside the liveProxy2 method
					// + "?overrun_nonfatal=1&fifo_size=50000000"

				streamInputRoot["url"] = newURL;
				streamInputRoot["tvMulticastIP"] = tvMulticastIP;
				streamInputRoot["tvMulticastPort"] = tvMulticastPort;
				inputRoot["streamInput"] = streamInputRoot;
				liveProxyData->_inputsRoot[inputIndex] = inputRoot;

				createOrUpdateTVDvbLastConfigurationFile(
					liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey, tvMulticastIP, tvMulticastPort, tvType, tvServiceId, tvFrequency, tvSymbolRate,
					tvBandwidthInHz / 1000000, tvModulation, tvVideoPid, tvAudioItalianPid, true
				);
			}
		}

		{
			// setting of liveProxyData->_proxyStart
			// Based on liveProxyData->_proxyStart, the monitor thread starts the checkings
			// In case of IP_PUSH, the checks should be done after the ffmpeg server
			// receives the stream and we do not know what it happens.
			// For this reason, in this scenario, we have to set _proxyStart in the worst scenario
			if (!liveProxyData->_inputsRoot.empty()) // it has to be > 0
			{
				json inputRoot = liveProxyData->_inputsRoot[0];

				int64_t utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, "utcScheduleStart", -1);
				// if (utcProxyPeriodStart == -1)
				// 	utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, "utcProxyPeriodStart", -1);

				if (JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
				{
					json streamInputRoot = inputRoot["streamInput"];

					string streamSourceType = JSONUtils::asString(streamInputRoot, "streamSourceType", "");

					if (streamSourceType == "IP_PUSH")
					{
						int pushListenTimeout = JSONUtils::asInt(streamInputRoot, "pushListenTimeout", -1);

						if (utcProxyPeriodStart != -1)
						{
							if (chrono::system_clock::from_time_t(utcProxyPeriodStart) < chrono::system_clock::now())
								liveProxyData->_proxyStart = chrono::system_clock::now() + chrono::seconds(pushListenTimeout);
							else
								liveProxyData->_proxyStart =
									chrono::system_clock::from_time_t(utcProxyPeriodStart) + chrono::seconds(pushListenTimeout);
						}
						else
							liveProxyData->_proxyStart = chrono::system_clock::now() + chrono::seconds(pushListenTimeout);
					}
					else
					{
						if (utcProxyPeriodStart != -1)
						{
							if (chrono::system_clock::from_time_t(utcProxyPeriodStart) < chrono::system_clock::now())
								liveProxyData->_proxyStart = chrono::system_clock::now();
							else
								liveProxyData->_proxyStart = chrono::system_clock::from_time_t(utcProxyPeriodStart);
						}
						else
							liveProxyData->_proxyStart = chrono::system_clock::now();
					}
				}
				else
				{
					if (utcProxyPeriodStart != -1)
					{
						if (chrono::system_clock::from_time_t(utcProxyPeriodStart) < chrono::system_clock::now())
							liveProxyData->_proxyStart = chrono::system_clock::now();
						else
							liveProxyData->_proxyStart = chrono::system_clock::from_time_t(utcProxyPeriodStart);
					}
					else
						liveProxyData->_proxyStart = chrono::system_clock::now();
				}
			}

			/*
			liveProxyData->_ffmpeg->liveProxy2(
				liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey, externalEncoder, maxStreamingDurationInMinutes,
				&(liveProxyData->_inputsRootMutex), &(liveProxyData->_inputsRoot), liveProxyData->_outputsRoot, liveProxyData->_childProcessId,
				&(liveProxyData->_proxyStart), &(liveProxyData->_numberOfRestartBecauseOfFailure)
			);
			*/
			liveProxyData->_ffmpeg->liveProxy(
				liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey, externalEncoder, maxStreamingDurationInMinutes,
				&(liveProxyData->_inputsRootMutex), &(liveProxyData->_inputsRoot), liveProxyData->_outputsRoot, liveProxyData->_childProcessId,
				&(liveProxyData->_proxyStart),
				[&](const string_view& line) {ffmpegLineCallback(line); }, liveProxyData->_callbackData,
				&(liveProxyData->_numberOfRestartBecauseOfFailure)
			);
		}

		for (int inputIndex = 0; inputIndex < liveProxyData->_inputsRoot.size(); inputIndex++)
		{
			json inputRoot = liveProxyData->_inputsRoot[inputIndex];

			if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
				continue;
			json streamInputRoot = inputRoot["streamInput"];

			string streamSourceType = JSONUtils::asString(streamInputRoot, "streamSourceType", "");
			if (streamSourceType == "TV")
			{
				string tvMulticastIP = JSONUtils::asString(streamInputRoot, "tvMulticastIP", "");
				string tvMulticastPort = JSONUtils::asString(streamInputRoot, "tvMulticastPort", "");

				string tvType = JSONUtils::asString(streamInputRoot, "tvType", "");
				int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
				int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
				int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
				int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
				string tvModulation = JSONUtils::asString(streamInputRoot, "tvModulation", "");
				int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
				int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot, "tvAudioItalianPid", -1);

				if (tvServiceId != -1) // this is just to be sure variables are initialized
				{
					// remove configuration from dvblast configuration file
					createOrUpdateTVDvbLastConfigurationFile(
						liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey, tvMulticastIP, tvMulticastPort, tvType, tvServiceId, tvFrequency,
						tvSymbolRate, tvBandwidthInHz / 1000000, tvModulation, tvVideoPid, tvAudioItalianPid, false
					);
				}
			}
		}

		SPDLOG_INFO(
			"_ffmpeg->liveProxy finished"
			", ingestionJobKey: {}"
			", _encodingJobKey: {}",
			liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey
			// + ", liveProxyData->_channelLabel: " + liveProxyData->_channelLabel
		);
	}
	catch (exception &e)
	{
		if (liveProxyData->_inputsRoot != nullptr)
		{
			for (int inputIndex = 0; inputIndex < liveProxyData->_inputsRoot.size(); inputIndex++)
			{
				json inputRoot = liveProxyData->_inputsRoot[inputIndex];

				if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
					continue;
				json streamInputRoot = inputRoot["streamInput"];

				string streamSourceType = JSONUtils::asString(streamInputRoot, "streamSourceType", "");
				if (streamSourceType == "TV")
				{
					string tvMulticastIP = JSONUtils::asString(streamInputRoot, "tvMulticastIP", "");
					string tvMulticastPort = JSONUtils::asString(streamInputRoot, "tvMulticastPort", "");

					string tvType = JSONUtils::asString(streamInputRoot, "tvType", "");
					int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
					int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
					int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
					int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
					string tvModulation = JSONUtils::asString(streamInputRoot, "tvModulation", "");
					int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot, "tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey, tvMulticastIP, tvMulticastPort, tvType, tvServiceId, tvFrequency,
							tvSymbolRate, tvBandwidthInHz / 1000000, tvModulation, tvVideoPid, tvAudioItalianPid, false
						);
					}
				}
			}
		}

		string eWhat = e.what();
		const string errorMessage = std::format(
			"{} API failed (EncodingKilledByUser)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), liveProxyData->_ingestionJobKey, liveProxyData->_encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		if (dynamic_cast<FFMpegEncodingKilledByUser*>(&e))
		{
			// used by FFMPEGEncoderTask
			if (liveProxyData->_killedBecauseOfNotWorking)
			{
				// it was killed just because it was not working and not because of user
				// In this case the process has to be restarted soon
				_completedWithError = true;
			}
			else
				_killedByUser = true;
		}
		else if (dynamic_cast<FFMpegURLForbidden*>(&e))
		{
			liveProxyData->pushErrorMessage(errorMessage);
			_completedWithError = true;
			_urlForbidden = true;
		}
		else if (dynamic_cast<FFMpegURLNotFound*>(&e))
		{
			liveProxyData->pushErrorMessage(errorMessage);
			_completedWithError = true;
			_urlNotFound = true;
		}
		else
		{
			liveProxyData->pushErrorMessage(errorMessage);
			_completedWithError = true;
		}

		throw;
	}
}
