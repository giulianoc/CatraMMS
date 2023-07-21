
#include "LiveRecorder.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/DateTime.h"


LiveRecorder::LiveRecorder(
	shared_ptr<LiveRecording> liveRecording,
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	Json::Value configuration,
	mutex* encodingCompletedMutex,                                                                        
	map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,                                    
	shared_ptr<spdlog::logger> logger,
	mutex* tvChannelsPortsMutex,                                                                          
	long* tvChannelPort_CurrentOffset):
	FFMPEGEncoderTask(liveRecording, ingestionJobKey, encodingJobKey, configuration, encodingCompletedMutex,
		encodingCompletedMap, logger)
{
	_liveRecording					= liveRecording;
	_tvChannelsPortsMutex			= tvChannelsPortsMutex;                                                                          
	_tvChannelPort_CurrentOffset	= tvChannelPort_CurrentOffset;

	_liveRecorderChunksIngestionCheckInSeconds =  JSONUtils::asInt(configuration["ffmpeg"], "liveRecorderChunksIngestionCheckInSeconds", 5);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->liveRecorderChunksIngestionCheckInSeconds: " + to_string(_liveRecorderChunksIngestionCheckInSeconds)
	);
};

LiveRecorder::~LiveRecorder()
{
	_liveRecording->_encodingParametersRoot = Json::nullValue;
	_liveRecording->_ingestionJobKey		= 0;
	_liveRecording->_channelLabel		= "";
	_liveRecording->_killedBecauseOfNotWorking = false;
}

void LiveRecorder::encodeContent(
	string requestBody)
{
	string api = "liveRecorder";

	_logger->info(__FILEREF__ + "Received " + api
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
		+ ", requestBody: " + requestBody
	);

	string tvMulticastIP;
	string tvMulticastPort;
	string tvType;
	int64_t tvServiceId = -1;
	int64_t tvFrequency = -1;
	int64_t tvSymbolRate = -1;
	int64_t tvBandwidthInHz = -1;
	string tvModulation;
	int tvVideoPid = -1;
	int tvAudioItalianPid = -1;

    try
    {
        _liveRecording->_killedBecauseOfNotWorking = false;

        Json::Value metadataRoot = JSONUtils::toJson(-1, _encodingJobKey, requestBody);

        _liveRecording->_ingestionJobKey = _ingestionJobKey;	// JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);
        _liveRecording->_externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);
		Json::Value encodingParametersRoot = metadataRoot["encodingParametersRoot"];
		Json::Value ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];

		// _chunksTranscoderStagingContentsPath is a transcoder LOCAL path,
		//		this is important because in case of high bitrate,
		//		nfs would not be enough fast and could create random file system error
        _liveRecording->_chunksTranscoderStagingContentsPath =
			JSONUtils::asString(encodingParametersRoot, "chunksTranscoderStagingContentsPath", "");
		string userAgent = JSONUtils::asString(ingestedParametersRoot, "userAgent", "");


		// this is the global shared path where the chunks would be moved for the ingestion
		// see the comments in EncoderVideoAudioProxy.cpp
        _liveRecording->_chunksNFSStagingContentsPath = JSONUtils::asString(encodingParametersRoot, "chunksNFSStagingContentsPath", "");
		// 2022-08-09: the chunksNFSStagingContentsPath directory was created by EncoderVideoAudioProxy.cpp
		// 		into the shared working area.
		// 		In case of an external encoder, the external working area does not have this directory
		// 		and the encoder will fail. For this reason, the directory is created if it does not exist
		// 2022-08-10: in case of an external encoder, the chunk has to be ingested
		//	as push, so the chunksNFSStagingContentsPath dir is not used at all
		//	For this reason the directory check is useless and it is commented
		/*
		if (!fs::exists(_liveRecording->_chunksNFSStagingContentsPath))
		{
			bool noErrorIfExists = true;
			bool recursive = true;
			_logger->info(__FILEREF__ + "Creating directory"
				+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			);
			fs::createDirectory(_liveRecording->_chunksNFSStagingContentsPath,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH,
				noErrorIfExists, recursive);
		}
		*/

        _liveRecording->_segmentListFileName = JSONUtils::asString(encodingParametersRoot, "segmentListFileName", "");
        _liveRecording->_recordedFileNamePrefix = JSONUtils::asString(encodingParametersRoot, "recordedFileNamePrefix", "");
		// see the comments in EncoderVideoAudioProxy.cpp
		if (_liveRecording->_externalEncoder)
			_liveRecording->_virtualVODStagingContentsPath =
				JSONUtils::asString(encodingParametersRoot, "virtualVODTranscoderStagingContentsPath", "");
		else
			_liveRecording->_virtualVODStagingContentsPath =
				JSONUtils::asString(encodingParametersRoot, "virtualVODStagingContentsPath", "");
       _liveRecording->_liveRecorderVirtualVODImageMediaItemKey =
			JSONUtils::asInt64(encodingParametersRoot, "liveRecorderVirtualVODImageMediaItemKey", -1);

		// _encodingParametersRoot has to be the last field to be set because liveRecorderChunksIngestion()
		//		checks this field is set before to see if there are chunks to be ingested
		_liveRecording->_encodingParametersRoot = encodingParametersRoot;
		_liveRecording->_ingestedParametersRoot =
			metadataRoot["ingestedParametersRoot"];

		time_t utcRecordingPeriodStart;
		time_t utcRecordingPeriodEnd;
		bool autoRenew;
		int segmentDurationInSeconds;
		string outputFileFormat;
		{
			string field = "schedule";
			Json::Value recordingPeriodRoot = (_liveRecording->_ingestedParametersRoot)[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(_liveRecording->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string recordingPeriodStart = JSONUtils::asString(recordingPeriodRoot, field, "");
			utcRecordingPeriodStart = DateTime::sDateSecondsToUtc(recordingPeriodStart);

			field = "end";
			if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(_liveRecording->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string recordingPeriodEnd = JSONUtils::asString(recordingPeriodRoot, field, "");
			utcRecordingPeriodEnd = DateTime::sDateSecondsToUtc(recordingPeriodEnd);

			field = "autoRenew";
			autoRenew = JSONUtils::asBool(recordingPeriodRoot, field, false);

			field = "segmentDuration";
			if (!JSONUtils::isMetadataPresent(_liveRecording->_ingestedParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(_liveRecording->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			segmentDurationInSeconds = JSONUtils::asInt(_liveRecording->_ingestedParametersRoot, field, -1);

			field = "outputFileFormat";
			outputFileFormat = JSONUtils::asString(_liveRecording->_ingestedParametersRoot, field, "ts");
		}

		_liveRecording->_monitoringEnabled = JSONUtils::asBool(
			_liveRecording->_ingestedParametersRoot, "monitoringEnabled", true);
		_liveRecording->_monitoringFrameIncreasingEnabled = JSONUtils::asBool(
			_liveRecording->_ingestedParametersRoot, "monitoringFrameIncreasingEnabled", true);

		_liveRecording->_channelLabel = JSONUtils::asString(_liveRecording->_ingestedParametersRoot,
			"configurationLabel", "");

		_liveRecording->_lastRecordedAssetFileName			= "";
		_liveRecording->_lastRecordedAssetDurationInSeconds	= 0.0;
		_liveRecording->_lastRecordedSegmentUtcStartTimeInMillisecs	= -1;

        _liveRecording->_streamSourceType = JSONUtils::asString(encodingParametersRoot,
			"streamSourceType", "IP_PULL");
		int ipMMSAsServer_listenTimeoutInSeconds = encodingParametersRoot
			.get("ActAsServerListenTimeout", 300).asInt();
		int pushListenTimeout = JSONUtils::asInt(encodingParametersRoot, "pushListenTimeout", -1);

		int captureLive_videoDeviceNumber = -1;
		string captureLive_videoInputFormat;
		int captureLive_frameRate = -1;
		int captureLive_width = -1;
		int captureLive_height = -1;
		int captureLive_audioDeviceNumber = -1;
		int captureLive_channelsNumber = -1;
		if (_liveRecording->_streamSourceType == "CaptureLive")
		{
			captureLive_videoDeviceNumber = JSONUtils::asInt(encodingParametersRoot["capture"],
				"videoDeviceNumber", -1);
			captureLive_videoInputFormat = JSONUtils::asString(encodingParametersRoot["capture"],
				"videoInputFormat", "");
			captureLive_frameRate = JSONUtils::asInt(encodingParametersRoot["capture"], "frameRate", -1);
			captureLive_width = JSONUtils::asInt(encodingParametersRoot["capture"], "width", -1);
			captureLive_height = JSONUtils::asInt(encodingParametersRoot["capture"], "height", -1);
			captureLive_audioDeviceNumber = JSONUtils::asInt(encodingParametersRoot["capture"],
				"audioDeviceNumber", -1);
			captureLive_channelsNumber = JSONUtils::asInt(encodingParametersRoot["capture"],
				"channelsNumber", -1);
		}

        string liveURL;

		if (_liveRecording->_streamSourceType == "TV")
		{
			tvType = JSONUtils::asString(encodingParametersRoot, "tvType", "");
			tvServiceId = JSONUtils::asInt64(encodingParametersRoot,
				"tvServiceId", -1);
			tvFrequency = JSONUtils::asInt64(encodingParametersRoot,
				"tvFrequency", -1);
			tvSymbolRate = JSONUtils::asInt64(encodingParametersRoot,
				"tvSymbolRate", -1);
			tvBandwidthInHz = JSONUtils::asInt64(encodingParametersRoot,
				"tvBandwidthInHz", -1);
			tvModulation = JSONUtils::asString(encodingParametersRoot, "tvModulation", "");
			tvVideoPid = JSONUtils::asInt(encodingParametersRoot, "tvVideoPid", -1);
			tvAudioItalianPid = JSONUtils::asInt(encodingParametersRoot,
				"tvAudioItalianPid", -1);

			// In case ffmpeg crashes and is automatically restarted, it should use the same
			// IP-PORT it was using before because we already have a dbvlast sending the stream
			// to the specified IP-PORT.
			// For this reason, before to generate a new IP-PORT, let's look for the serviceId
			// inside the dvblast conf. file to see if it was already running before

			pair<string, string> tvMulticast =
				getTVMulticastFromDvblastConfigurationFile(
				_liveRecording->_ingestionJobKey, _encodingJobKey,
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

				*_tvChannelPort_CurrentOffset =
					(*_tvChannelPort_CurrentOffset + 1)
					% _tvChannelPort_MaxNumberOfOffsets;
				*/
			}

			// overrun_nonfatal=1 prevents ffmpeg from exiting,
			//		it can recover in most circumstances.
			// fifo_size=50000000 uses a 50MB udp input buffer (default 5MB)
			liveURL = string("udp://@") + tvMulticastIP
				+ ":" + tvMulticastPort
				+ "?overrun_nonfatal=1&fifo_size=50000000"
			;

			createOrUpdateTVDvbLastConfigurationFile(
				_liveRecording->_ingestionJobKey, _encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				true);
		}
		else 
		{
			// in case of actAsServer
			//	true: it is set into the MMSEngineProcessor::manageLiveRecorder method
			//	false: it comes from the LiveRecorder json ingested
			liveURL = JSONUtils::asString(encodingParametersRoot, "liveURL", "");
		}

		Json::Value outputsRoot = encodingParametersRoot["outputsRoot"];

		{
			bool monitorHLS = JSONUtils::asBool(encodingParametersRoot, "monitorHLS", false);
			_liveRecording->_virtualVOD = JSONUtils::asBool(encodingParametersRoot,
				"liveRecorderVirtualVOD", false);

			if (monitorHLS || _liveRecording->_virtualVOD)
			{
				// monitorVirtualVODOutputRootIndex has to be initialized in case of monitor/virtualVOD
				int monitorVirtualVODOutputRootIndex = JSONUtils::asInt(
					encodingParametersRoot, "monitorVirtualVODOutputRootIndex", -1);

				if (monitorVirtualVODOutputRootIndex >= 0)
				{
					_liveRecording->_monitorVirtualVODManifestDirectoryPath =
						JSONUtils::asString(outputsRoot[monitorVirtualVODOutputRootIndex],
						"manifestDirectoryPath", "");
					_liveRecording->_monitorVirtualVODManifestFileName =
						JSONUtils::asString(outputsRoot[monitorVirtualVODOutputRootIndex],
						"manifestFileName", "");
				}
			}
		}

		if (fs::exists(_liveRecording->_chunksTranscoderStagingContentsPath
			+ _liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", ingestionJobKey: " + to_string(_liveRecording->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingJobKey)
				+ ", segmentListPathName: " + _liveRecording->_chunksTranscoderStagingContentsPath
					+ _liveRecording->_segmentListFileName
			);
			fs::remove_all(_liveRecording->_chunksTranscoderStagingContentsPath
				+ _liveRecording->_segmentListFileName);
		}

		// since the first chunk is discarded, we will start recording before the period of the chunk
		// In case of autorenew, when it is renewed, we will lose the first chunk
		// utcRecordingPeriodStart -= segmentDurationInSeconds;
		// 2019-12-19: the above problem is managed inside _ffmpeg->liveRecorder
		//		(see the secondsToStartEarly variable inside _ffmpeg->liveRecorder)
		//		For this reason the above decrement was commented

		// based on liveProxy->_proxyStart, the monitor thread starts the checkings
		// In case of IP_PUSH, the checks should be done after the ffmpeg server
		// receives the stream and we do not know what it happens.
		// For this reason, in this scenario, we have to set _proxyStart in the worst scenario
		if (_liveRecording->_streamSourceType == "IP_PUSH")
		{
			if (chrono::system_clock::from_time_t(
					utcRecordingPeriodStart) < chrono::system_clock::now())
				_liveRecording->_recordingStart = chrono::system_clock::now() +
					chrono::seconds(ipMMSAsServer_listenTimeoutInSeconds);
			else
				_liveRecording->_recordingStart = chrono::system_clock::from_time_t(
					utcRecordingPeriodStart) +
					chrono::seconds(ipMMSAsServer_listenTimeoutInSeconds);
		}
		else
		{
			if (chrono::system_clock::from_time_t(utcRecordingPeriodStart)
					< chrono::system_clock::now())
				_liveRecording->_recordingStart = chrono::system_clock::now();
			else
				_liveRecording->_recordingStart = chrono::system_clock::from_time_t(
					utcRecordingPeriodStart);
		}

		// _liveRecording->_liveRecorderOutputRoots.clear();
		{
			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");
				string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");

				// if (outputType == "HLS" || outputType == "DASH")
				if (outputType == "HLS_Channel")
				{
					if (fs::exists(manifestDirectoryPath))
					{
						try
						{
							_logger->info(__FILEREF__ + "removeDirectory"
								+ ", ingestionJobKey: " + to_string(_liveRecording->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
							);
							fs::remove_all(manifestDirectoryPath);
						}
						catch(runtime_error e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(_liveRecording->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
						catch(exception e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: "
									+ to_string(_liveRecording->_ingestionJobKey)
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

		Json::Value framesToBeDetectedRoot = encodingParametersRoot["framesToBeDetectedRoot"];

		string otherInputOptions = JSONUtils::asString(_liveRecording->_ingestedParametersRoot,
			"otherInputOptions", "");

		_liveRecording->_segmenterType = "hlsSegmenter";
		// _liveRecording->_segmenterType = "streamSegmenter";

		_logger->info(__FILEREF__ + "liveRecorder. _ffmpeg->liveRecorder"
			+ ", ingestionJobKey: " + to_string(_liveRecording->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", streamSourceType: " + _liveRecording->_streamSourceType
			+ ", liveURL: " + liveURL
		);
		_liveRecording->_ffmpeg->liveRecorder(
			_liveRecording->_ingestionJobKey,
			_encodingJobKey,
			_liveRecording->_externalEncoder,
			_liveRecording->_chunksTranscoderStagingContentsPath
				+ _liveRecording->_segmentListFileName,
			_liveRecording->_recordedFileNamePrefix,

			otherInputOptions,

			_liveRecording->_streamSourceType,
			StringUtils::trimTabToo(liveURL),
			pushListenTimeout,
			captureLive_videoDeviceNumber,
			captureLive_videoInputFormat,
			captureLive_frameRate,
			captureLive_width,
			captureLive_height,
			captureLive_audioDeviceNumber,
			captureLive_channelsNumber,

			userAgent,
			utcRecordingPeriodStart,
			utcRecordingPeriodEnd,

			segmentDurationInSeconds,
			outputFileFormat,
			_liveRecording->_segmenterType,

			outputsRoot,

			framesToBeDetectedRoot,

			&(_liveRecording->_childPid),
			&(_liveRecording->_recordingStart)
		);

		if (_liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				_liveRecording->_ingestionJobKey, _encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

		if (!autoRenew)
		{
			// to wait the ingestion of the last chunk
			this_thread::sleep_for(chrono::seconds(
				2 * _liveRecorderChunksIngestionCheckInSeconds));
		}

		_liveRecording->_encodingParametersRoot = Json::nullValue;
        _liveRecording->_killedBecauseOfNotWorking = false;
        
        _logger->info(__FILEREF__ + "liveRecorded finished"
            + ", _liveRecording->_ingestionJobKey: "
				+ to_string(_liveRecording->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
        );

        _liveRecording->_ingestionJobKey		= 0;
		_liveRecording->_channelLabel		= "";
		// _liveRecording->_liveRecorderOutputRoots.clear();

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (fs::exists(_liveRecording->_chunksTranscoderStagingContentsPath
			+ _liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ _liveRecording->_chunksTranscoderStagingContentsPath
					+ _liveRecording->_segmentListFileName
			);
			fs::remove_all(_liveRecording->_chunksTranscoderStagingContentsPath
				+ _liveRecording->_segmentListFileName);
		}
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		if (_liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				_liveRecording->_ingestionJobKey, _encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (fs::exists(_liveRecording->_chunksTranscoderStagingContentsPath
					+ _liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ _liveRecording->_chunksTranscoderStagingContentsPath
					+ _liveRecording->_segmentListFileName
			);
			fs::remove_all(_liveRecording->_chunksTranscoderStagingContentsPath
				+ _liveRecording->_segmentListFileName);
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
		if (_liveRecording->_killedBecauseOfNotWorking)
		{
			// it was killed just because it was not working and not because of user
			// In this case the process has to be restarted soon
			_killedByUser				= false;
			_completedWithError			= true;
			_liveRecording->_killedBecauseOfNotWorking = false;
		}
		else
		{
			_killedByUser                = true;
		}

		throw e;
    }
    catch(FFMpegURLForbidden e)
    {
		if (_liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				_liveRecording->_ingestionJobKey, _encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (fs::exists(_liveRecording->_chunksTranscoderStagingContentsPath
			+ _liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ _liveRecording->_chunksTranscoderStagingContentsPath
					+ _liveRecording->_segmentListFileName
			);
			fs::remove_all(_liveRecording->_chunksTranscoderStagingContentsPath
				+ _liveRecording->_segmentListFileName);
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
		_liveRecording->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
    catch(FFMpegURLNotFound e)
    {
		if (_liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				_liveRecording->_ingestionJobKey, _encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (fs::exists(_liveRecording->_chunksTranscoderStagingContentsPath
			+ _liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ _liveRecording->_chunksTranscoderStagingContentsPath
					+ _liveRecording->_segmentListFileName
			);
			fs::remove_all(_liveRecording->_chunksTranscoderStagingContentsPath
				+ _liveRecording->_segmentListFileName);
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
		_liveRecording->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
    catch(runtime_error e)
    {
		if (_liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				_liveRecording->_ingestionJobKey, _encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (fs::exists(_liveRecording->_chunksTranscoderStagingContentsPath
			+ _liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ _liveRecording->_chunksTranscoderStagingContentsPath
					+ _liveRecording->_segmentListFileName
			);
			fs::remove_all(_liveRecording->_chunksTranscoderStagingContentsPath
				+ _liveRecording->_segmentListFileName);
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
		_liveRecording->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
    catch(exception e)
    {
		if (_liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				_liveRecording->_ingestionJobKey, _encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (fs::exists(_liveRecording->_chunksTranscoderStagingContentsPath
			+ _liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ _liveRecording->_chunksTranscoderStagingContentsPath
					+ _liveRecording->_segmentListFileName
			);
			fs::remove_all(_liveRecording->_chunksTranscoderStagingContentsPath
				+ _liveRecording->_segmentListFileName);
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
		_liveRecording->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
}

