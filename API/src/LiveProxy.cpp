
#include "LiveProxy.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/DateTime.h"


LiveProxy::LiveProxy(
	shared_ptr<LiveProxyAndGrid> liveProxyData,
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	json configurationRoot,
	mutex* encodingCompletedMutex,
	map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,
	shared_ptr<spdlog::logger> logger,
	mutex* tvChannelsPortsMutex,
	long* tvChannelPort_CurrentOffset):
	FFMPEGEncoderTask(liveProxyData, ingestionJobKey, encodingJobKey, configurationRoot, encodingCompletedMutex,
		encodingCompletedMap, logger)
{
	_liveProxyData					= liveProxyData;
	_tvChannelsPortsMutex			= tvChannelsPortsMutex;                                                                          
	_tvChannelPort_CurrentOffset	= tvChannelPort_CurrentOffset;

};

LiveProxy::~LiveProxy()
{
	_liveProxyData->_encodingParametersRoot = nullptr;
	_liveProxyData->_method = "";
	_liveProxyData->_ingestionJobKey = 0;
	// _liveProxyData->_channelLabel = "";
	// _liveProxyData->_liveProxyOutputRoots.clear();
	_liveProxyData->_killedBecauseOfNotWorking = false;
}

void LiveProxy::encodeContent(
	string requestBody)
{
    string api = "liveProxy";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
        + ", requestBody: " + requestBody
    );

	// string tvMulticastIP;
	// string tvMulticastPort;
	// string tvType;
	// int64_t tvServiceId = -1;
	// int64_t tvFrequency = -1;
	// int64_t tvSymbolRate = -1;
	// int64_t tvBandwidthInMhz = -1;
	// string tvModulation;
	// int tvVideoPid = -1;
	// int tvAudioItalianPid = -1;
    try
    {
		_liveProxyData->_killedBecauseOfNotWorking = false;
        json metadataRoot = JSONUtils::toJson(requestBody);

		_liveProxyData->_ingestionJobKey = _ingestionJobKey;	// JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);

		_liveProxyData->_encodingParametersRoot = metadataRoot["encodingParametersRoot"];
		_liveProxyData->_ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];
		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];


		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);

		_liveProxyData->_monitoringRealTimeInfoEnabled = JSONUtils::asBool(
			_liveProxyData->_ingestedParametersRoot, "monitoringFrameIncreasingEnabled", true);

		_liveProxyData->_outputsRoot = encodingParametersRoot["outputsRoot"];
		{
			for(int outputIndex = 0; outputIndex < _liveProxyData->_outputsRoot.size(); outputIndex++)
			{
				json outputRoot = _liveProxyData->_outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				// if (outputType == "HLS" || outputType == "DASH")
				if (outputType == "HLS_Channel")
				{
					string manifestDirectoryPath
						= JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");

					if (fs::exists(manifestDirectoryPath))
					{
						try
						{
							_logger->info(__FILEREF__ + "removeDirectory"
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
							);
							fs::remove_all(manifestDirectoryPath);
						}
						catch(runtime_error& e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(_liveProxyData->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
						catch(exception& e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(_liveProxyData->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
					}
				}
			}
		}


		// non serve
		// _liveProxyData->_channelLabel = "";

		// _liveProxyData->_streamSourceType = metadataRoot["encodingParametersRoot"].
		// 	get("streamSourceType", "IP_PULL").asString();

		_liveProxyData->_inputsRoot = encodingParametersRoot["inputsRoot"];

		for (int inputIndex = 0; inputIndex < _liveProxyData->_inputsRoot.size(); inputIndex++)
		{
			json inputRoot = _liveProxyData->_inputsRoot[inputIndex];

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
				int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
					"tvAudioItalianPid", -1);

				// In case ffmpeg crashes and is automatically restarted, it should use the same
				// IP-PORT it was using before because we already have a dbvlast sending the stream
				// to the specified IP-PORT.
				// For this reason, before to generate a new IP-PORT, let's look for the serviceId
				// inside the dvblast conf. file to see if it was already running before

				string tvMulticastIP;
				string tvMulticastPort;

				// in case there is already a serviceId running, we will use the same multicastIP-Port
				pair<string, string> tvMulticast = getTVMulticastFromDvblastConfigurationFile(
					_liveProxyData->_ingestionJobKey, _encodingJobKey,
					tvType, tvServiceId, tvFrequency, tvSymbolRate,
					tvBandwidthInHz / 1000000,
					tvModulation);
				tie(tvMulticastIP, tvMulticastPort) = tvMulticast;

				if (tvMulticastIP == "")
				{
					*_tvChannelPort_CurrentOffset = getFreeTvChannelPortOffset(
						_tvChannelsPortsMutex, *_tvChannelPort_CurrentOffset);

					tvMulticastIP = "239.255.1.1";
					tvMulticastPort = to_string(*_tvChannelPort_CurrentOffset
						+ _tvChannelPort_Start);

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
				string newURL = string("udp://@") + tvMulticastIP
					+ ":" + tvMulticastPort
					// 2022-12-08: the below parameters are added inside the liveProxy2 method
					// + "?overrun_nonfatal=1&fifo_size=50000000"
				;

				streamInputRoot["url"] = newURL;
				streamInputRoot["tvMulticastIP"] = tvMulticastIP;
				streamInputRoot["tvMulticastPort"] = tvMulticastPort;
				inputRoot["streamInput"] = streamInputRoot;
				_liveProxyData->_inputsRoot[inputIndex] = inputRoot;

				createOrUpdateTVDvbLastConfigurationFile(
					_liveProxyData->_ingestionJobKey, _encodingJobKey,
					tvMulticastIP, tvMulticastPort,
					tvType, tvServiceId, tvFrequency, tvSymbolRate,
					tvBandwidthInHz / 1000000,
					tvModulation, tvVideoPid, tvAudioItalianPid,
					true);
			}
		}

		{
			// setting of _liveProxyData->_proxyStart
			// Based on _liveProxyData->_proxyStart, the monitor thread starts the checkings
			// In case of IP_PUSH, the checks should be done after the ffmpeg server
			// receives the stream and we do not know what it happens.
			// For this reason, in this scenario, we have to set _proxyStart in the worst scenario
			if (_liveProxyData->_inputsRoot.size() > 0)	// it has to be > 0
			{
				json inputRoot = _liveProxyData->_inputsRoot[0];

				int64_t utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, "utcScheduleStart", -1);
				// if (utcProxyPeriodStart == -1)
				// 	utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, "utcProxyPeriodStart", -1);

				if (JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
				{
					json streamInputRoot = inputRoot["streamInput"];

					string streamSourceType =
						JSONUtils::asString(streamInputRoot, "streamSourceType", "");

					if (streamSourceType == "IP_PUSH")
					{
						int pushListenTimeout = JSONUtils::asInt(
							streamInputRoot, "pushListenTimeout", -1);

						if (utcProxyPeriodStart != -1)
						{
							if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
								chrono::system_clock::now())
								_liveProxyData->_proxyStart = chrono::system_clock::now() +
									chrono::seconds(pushListenTimeout);
							else
								_liveProxyData->_proxyStart = chrono::system_clock::from_time_t(
									utcProxyPeriodStart) +
									chrono::seconds(pushListenTimeout);
						}
						else
							_liveProxyData->_proxyStart = chrono::system_clock::now() +
								chrono::seconds(pushListenTimeout);
					}
					else
					{
						if (utcProxyPeriodStart != -1)
						{
							if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
									chrono::system_clock::now())
								_liveProxyData->_proxyStart = chrono::system_clock::now();
							else
								_liveProxyData->_proxyStart = chrono::system_clock::from_time_t(
									utcProxyPeriodStart);
						}
						else
							_liveProxyData->_proxyStart = chrono::system_clock::now();
					}
				}
				else
				{
					if (utcProxyPeriodStart != -1)
					{
						if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
								chrono::system_clock::now())
							_liveProxyData->_proxyStart = chrono::system_clock::now();
						else
							_liveProxyData->_proxyStart = chrono::system_clock::from_time_t(
								utcProxyPeriodStart);
					}
					else
						_liveProxyData->_proxyStart = chrono::system_clock::now();
				}
			}

			_liveProxyData->_ffmpeg->liveProxy2(
				_liveProxyData->_ingestionJobKey,
				_encodingJobKey,
				externalEncoder,
				&(_liveProxyData->_inputsRootMutex),
				&(_liveProxyData->_inputsRoot),
				_liveProxyData->_outputsRoot,
				&(_liveProxyData->_childPid),
				&(_liveProxyData->_proxyStart)
			);
		}

		for (int inputIndex = 0; inputIndex < _liveProxyData->_inputsRoot.size(); inputIndex++)
		{
			json inputRoot = _liveProxyData->_inputsRoot[inputIndex];

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
				int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
					"tvAudioItalianPid", -1);

				if (tvServiceId != -1) // this is just to be sure variables are initialized
				{
					// remove configuration from dvblast configuration file
					createOrUpdateTVDvbLastConfigurationFile(
						_liveProxyData->_ingestionJobKey, _encodingJobKey,
						tvMulticastIP, tvMulticastPort,
						tvType, tvServiceId, tvFrequency, tvSymbolRate,
						tvBandwidthInHz / 1000000,
						tvModulation, tvVideoPid, tvAudioItalianPid,
						false);
				}
			}
		}

        _logger->info(__FILEREF__ + "_ffmpeg->liveProxy finished"
			+ ", ingestionJobKey: " + to_string(_liveProxyData->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
            // + ", _liveProxyData->_channelLabel: " + _liveProxyData->_channelLabel
        );
    }
	catch(FFMpegEncodingKilledByUser& e)
	{
		if (_liveProxyData->_inputsRoot != nullptr)
		{
			for (int inputIndex = 0; inputIndex < _liveProxyData->_inputsRoot.size(); inputIndex++)
			{
				json inputRoot = _liveProxyData->_inputsRoot[inputIndex];

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
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							_liveProxyData->_ingestionJobKey, _encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;

        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		if (_liveProxyData->_killedBecauseOfNotWorking)
		{
			// it was killed just because it was not working and not because of user
			// In this case the process has to be restarted soon
			_completedWithError			= true;
		}
		else
		{
			_killedByUser				= true;
		}

		throw e;
    }
    catch(FFMpegURLForbidden& e)
    {
		if (_liveProxyData->_inputsRoot != nullptr)
		{
			for (int inputIndex = 0; inputIndex < _liveProxyData->_inputsRoot.size(); inputIndex++)
			{
				json inputRoot = _liveProxyData->_inputsRoot[inputIndex];

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
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							_liveProxyData->_ingestionJobKey, _encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLForbidden)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);


		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage	= errorMessage;
		_completedWithError			= true;
		_urlForbidden				= true;

		throw e;
    }
    catch(FFMpegURLNotFound& e)
    {
		if (_liveProxyData->_inputsRoot != nullptr)
		{
			for (int inputIndex = 0; inputIndex < _liveProxyData->_inputsRoot.size(); inputIndex++)
			{
				json inputRoot = _liveProxyData->_inputsRoot[inputIndex];

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
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							_liveProxyData->_ingestionJobKey, _encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLNotFound)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);


		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage	= errorMessage;
		_completedWithError			= true;
		_urlNotFound				= true;

		throw e;
    }
    catch(runtime_error& e)
    {
		if (_liveProxyData->_inputsRoot != nullptr)
		{
			for (int inputIndex = 0; inputIndex < _liveProxyData->_inputsRoot.size(); inputIndex++)
			{
				json inputRoot = _liveProxyData->_inputsRoot[inputIndex];

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
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							_liveProxyData->_ingestionJobKey, _encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage	= errorMessage;
		_completedWithError			= true;

		throw e;
    }
    catch(exception& e)
    {
		if (_liveProxyData->_inputsRoot != nullptr)
		{
			for (int inputIndex = 0; inputIndex < _liveProxyData->_inputsRoot.size(); inputIndex++)
			{
				json inputRoot = _liveProxyData->_inputsRoot[inputIndex];

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
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							_liveProxyData->_ingestionJobKey, _encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage	= errorMessage;
		_completedWithError			= true;

		throw e;
    }
}

