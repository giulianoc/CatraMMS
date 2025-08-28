
#ifndef FFMPEGEncoderBase_h
#define FFMPEGEncoderBase_h

#include "ProcessUtility.h"
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "FFMpegWrapper.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <queue>
#include <string>

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename((char *)__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

class FFMPEGEncoderBase
{

  public:
	struct Encoding
	{
		bool _available;
		ProcessUtility::ProcessId _childProcessId;
		int64_t _encodingJobKey;
		shared_ptr<FFMpegWrapper> _ffmpeg;
		bool _ffmpegTerminatedSuccessful;
		bool _killToRestartByEngine;

		mutex _errorMessagesMutex;
		queue<string> _errorMessages;
		string _lastErrorMessage;

		void pushErrorMessage(string errorMessage)
		{
			lock_guard<mutex> locker(_errorMessagesMutex);
			_errorMessages.push(errorMessage);

			_lastErrorMessage = errorMessage;
		}

		string popErrorMessage()
		{
			string errorMessage;

			lock_guard<mutex> locker(_errorMessagesMutex);

			if (!_errorMessages.empty())
			{
				errorMessage = _errorMessages.front();
				_errorMessages.pop();
			}
			return errorMessage;
		}
	};

	struct LiveProxyAndGrid : public Encoding
	{
		string _method;					 // liveProxy, liveGrid or awaitingTheBeginning
		bool _killedBecauseOfNotWorking; // by monitorThread

		// string					_liveGridOutputType;	// only for LiveGrid
		json _outputsRoot;

		bool _monitoringRealTimeInfoEnabled; // frame/size/time
		time_t _outputFfmpegFileModificationTime;
		long _realTimeFrame;
		long _realTimeSize;
		long _realTimeFrameRate;
		double _realTimeBitRate;
		double _realTimeTimeInMilliSeconds;
		chrono::system_clock::time_point _realTimeLastChange;

		long _numberOfRestartBecauseOfFailure;

		int64_t _ingestionJobKey;
		json _encodingParametersRoot;
		json _ingestedParametersRoot;

		json _inputsRoot;
		mutex _inputsRootMutex;

		chrono::system_clock::time_point _proxyStart;

		shared_ptr<LiveProxyAndGrid> cloneForMonitor()
		{
			shared_ptr<LiveProxyAndGrid> liveProxyAndGrid = make_shared<LiveProxyAndGrid>();

			liveProxyAndGrid->_available = _available;
			liveProxyAndGrid->_childProcessId = _childProcessId;
			liveProxyAndGrid->_killToRestartByEngine = _killToRestartByEngine;
			liveProxyAndGrid->_monitoringRealTimeInfoEnabled = _monitoringRealTimeInfoEnabled;
			liveProxyAndGrid->_outputFfmpegFileModificationTime = _outputFfmpegFileModificationTime;
			liveProxyAndGrid->_realTimeFrame = _realTimeFrame;
			liveProxyAndGrid->_realTimeSize = _realTimeSize;
			liveProxyAndGrid->_realTimeFrameRate = _realTimeFrameRate;
			liveProxyAndGrid->_realTimeBitRate = _realTimeBitRate;
			liveProxyAndGrid->_realTimeTimeInMilliSeconds = _realTimeTimeInMilliSeconds;
			liveProxyAndGrid->_realTimeLastChange = _realTimeLastChange;
			liveProxyAndGrid->_numberOfRestartBecauseOfFailure = _numberOfRestartBecauseOfFailure;
			liveProxyAndGrid->_encodingJobKey = _encodingJobKey;
			liveProxyAndGrid->_method = _method;
			liveProxyAndGrid->_ffmpeg = _ffmpeg;
			liveProxyAndGrid->_killedBecauseOfNotWorking = _killedBecauseOfNotWorking;
			// non serve clonare _errorMessages perchè il monitor eventualmente scrive su _errorMessages del source
			// liveProxyAndGrid->_errorMessages = _errorMessages;

			liveProxyAndGrid->_ingestionJobKey = _ingestionJobKey;

			liveProxyAndGrid->_outputsRoot = _outputsRoot;
			liveProxyAndGrid->_encodingParametersRoot = _encodingParametersRoot;
			liveProxyAndGrid->_ingestedParametersRoot = _ingestedParametersRoot;
			liveProxyAndGrid->_inputsRoot = _inputsRoot;

			liveProxyAndGrid->_proxyStart = _proxyStart;

			return liveProxyAndGrid;
		}
	};

	struct LiveRecording : public Encoding
	{
		bool _killedBecauseOfNotWorking; // by monitorThread

		bool _monitoringEnabled;

		bool _monitoringRealTimeInfoEnabled; // frame/size/time
		time_t _outputFfmpegFileModificationTime;
		long _realTimeFrame;
		long _realTimeSize;
		long _realTimeFrameRate;
		double _realTimeBitRate;
		double _realTimeTimeInMilliSeconds;
		chrono::system_clock::time_point _realTimeLastChange;

		long _numberOfRestartBecauseOfFailure;

		int64_t _ingestionJobKey;
		bool _externalEncoder;
		json _encodingParametersRoot;
		json _ingestedParametersRoot;
		string _streamSourceType;
		string _chunksTranscoderStagingContentsPath;
		string _chunksNFSStagingContentsPath;
		string _segmentListFileName;
		string _recordedFileNamePrefix;
		string _lastRecordedAssetFileName;
		double _lastRecordedAssetDurationInSeconds;
		int64_t _lastRecordedSegmentUtcStartTimeInMillisecs;
		string _channelLabel;
		string _segmenterType;
		chrono::system_clock::time_point _recordingStart;

		bool _virtualVOD;
		string _monitorVirtualVODManifestDirectoryPath; // used to build virtualVOD
		string _monitorVirtualVODManifestFileName;		// used to build virtualVOD
		string _virtualVODStagingContentsPath;
		int64_t _liveRecorderVirtualVODImageMediaItemKey;

		shared_ptr<LiveRecording> cloneForMonitorAndVirtualVOD()
		{
			shared_ptr<LiveRecording> liveRecording = make_shared<LiveRecording>();

			liveRecording->_available = _available;
			liveRecording->_childProcessId = _childProcessId;
			liveRecording->_killToRestartByEngine = _killToRestartByEngine;
			liveRecording->_monitoringEnabled = _monitoringEnabled;
			liveRecording->_monitoringRealTimeInfoEnabled = _monitoringRealTimeInfoEnabled;
			liveRecording->_outputFfmpegFileModificationTime = _outputFfmpegFileModificationTime;
			liveRecording->_realTimeFrame = _realTimeFrame;
			liveRecording->_realTimeSize = _realTimeSize;
			liveRecording->_realTimeFrameRate = _realTimeFrameRate;
			liveRecording->_realTimeBitRate = _realTimeBitRate;
			liveRecording->_realTimeTimeInMilliSeconds = _realTimeTimeInMilliSeconds;
			liveRecording->_realTimeLastChange = _realTimeLastChange;
			liveRecording->_numberOfRestartBecauseOfFailure = _numberOfRestartBecauseOfFailure;
			liveRecording->_encodingJobKey = _encodingJobKey;
			liveRecording->_externalEncoder = _externalEncoder;
			liveRecording->_ffmpeg = _ffmpeg;
			liveRecording->_killedBecauseOfNotWorking = _killedBecauseOfNotWorking;
			// non serve clonare _errorMessages perchè il monitor eventualmente scrive su _errorMessages del source
			// liveRecording->_errorMessage = _errorMessage;
			liveRecording->_ingestionJobKey = _ingestionJobKey;
			liveRecording->_encodingParametersRoot = _encodingParametersRoot;
			liveRecording->_ingestedParametersRoot = _ingestedParametersRoot;
			liveRecording->_streamSourceType = _streamSourceType;
			liveRecording->_chunksTranscoderStagingContentsPath = _chunksTranscoderStagingContentsPath;
			liveRecording->_chunksNFSStagingContentsPath = _chunksNFSStagingContentsPath;
			liveRecording->_segmentListFileName = _segmentListFileName;
			liveRecording->_recordedFileNamePrefix = _recordedFileNamePrefix;
			liveRecording->_lastRecordedAssetFileName = _lastRecordedAssetFileName;
			liveRecording->_lastRecordedAssetDurationInSeconds = _lastRecordedAssetDurationInSeconds;
			liveRecording->_channelLabel = _channelLabel;
			liveRecording->_segmenterType = _segmenterType;
			liveRecording->_recordingStart = _recordingStart;
			liveRecording->_virtualVOD = _virtualVOD;
			liveRecording->_monitorVirtualVODManifestDirectoryPath = _monitorVirtualVODManifestDirectoryPath;
			liveRecording->_monitorVirtualVODManifestFileName = _monitorVirtualVODManifestFileName;
			liveRecording->_virtualVODStagingContentsPath = _virtualVODStagingContentsPath;
			liveRecording->_liveRecorderVirtualVODImageMediaItemKey = _liveRecorderVirtualVODImageMediaItemKey;

			return liveRecording;
		}
	};

	struct EncodingCompleted
	{
		int64_t _encodingJobKey;
		bool _completedWithError;
		string _errorMessage;
		bool _killedByUser;
		bool _killToRestartByEngine;
		bool _urlForbidden;
		bool _urlNotFound;
		chrono::system_clock::time_point _timestamp;
	};

  public:
	FFMPEGEncoderBase(json configurationRoot);
	~FFMPEGEncoderBase();

  protected:
	int64_t _mmsAPITimeoutInSeconds;
	int64_t _mmsBinaryTimeoutInSeconds;

	long getAddContentIngestionJobKey(int64_t ingestionJobKey, string ingestionResponse);
};

#endif
