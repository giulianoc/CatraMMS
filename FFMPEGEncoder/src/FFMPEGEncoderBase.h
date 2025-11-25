
#pragma once

#include "ProcessUtility.h"
#include <cstdint>
#include <fstream>
#include <shared_mutex>
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
		virtual ~Encoding() = default;

		void initEncoding(const int64_t encodingJobKey, const string_view& method)
		{
			_available = false;
			_method = method;
			_childProcessId.reset(); // not running
			_killToRestartByEngine = false;
			_encodingJobKey = encodingJobKey;
			_ffmpegTerminatedSuccessful = false;
		}

		virtual void reset()
		{
			_available = true;
			_ingestionJobKey = 0;
			_encodingJobKey = 0;
			_method = "";
			_childProcessId.reset(); // not running
			_callbackData->reset();
		}

		shared_ptr<FFMpegEngine::CallbackData> _callbackData;

		string _method;
		bool _available{};
		ProcessUtility::ProcessId _childProcessId;
		int64_t _ingestionJobKey{};
		int64_t _encodingJobKey{};
		shared_ptr<FFMpegWrapper> _ffmpeg;
		bool _ffmpegTerminatedSuccessful{};
		bool _killToRestartByEngine{};
	};

	struct LiveProxyAndGrid final : public Encoding
	{
		bool _killedBecauseOfNotWorking{}; // by monitorThread

		// string					_liveGridOutputType;	// only for LiveGrid
		json _outputsRoot;

		bool _monitoringRealTimeInfoEnabled{}; // frame/size/time
		uintmax_t _lastOutputFfmpegFileSize{};
		int32_t _lastRealTimeFrame{};
		size_t _lastRealTimeSize{};
		long _lastRealTimeFrameRate{};
		double _lastRealTimeBitRate{};
		chrono::milliseconds _lastRealTimeTimeInMilliSeconds{};
		chrono::system_clock::time_point _realTimeLastChange;

		long _numberOfRestartBecauseOfFailure{};

		json _encodingParametersRoot;
		json _ingestedParametersRoot;

		json _inputsRoot;
		mutex _inputsRootMutex;

		chrono::system_clock::time_point _proxyStart;

		[[nodiscard]] shared_ptr<LiveProxyAndGrid> cloneForMonitor() const
		{
			auto liveProxyAndGrid = make_shared<LiveProxyAndGrid>();

			liveProxyAndGrid->_available = _available;
			liveProxyAndGrid->_childProcessId = _childProcessId;
			liveProxyAndGrid->_killToRestartByEngine = _killToRestartByEngine;
			liveProxyAndGrid->_monitoringRealTimeInfoEnabled = _monitoringRealTimeInfoEnabled;
			liveProxyAndGrid->_lastOutputFfmpegFileSize = _lastOutputFfmpegFileSize;
			liveProxyAndGrid->_lastRealTimeFrame = _lastRealTimeFrame;
			liveProxyAndGrid->_lastRealTimeSize = _lastRealTimeSize;
			liveProxyAndGrid->_lastRealTimeFrameRate = _lastRealTimeFrameRate;
			liveProxyAndGrid->_lastRealTimeBitRate = _lastRealTimeBitRate;
			liveProxyAndGrid->_lastRealTimeTimeInMilliSeconds = _lastRealTimeTimeInMilliSeconds;
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

			liveProxyAndGrid->_callbackData = _callbackData->clone();

			liveProxyAndGrid->_proxyStart = _proxyStart;

			return liveProxyAndGrid;
		}

		void reset() override
		{
			_encodingParametersRoot = nullptr;
			_killedBecauseOfNotWorking = false;

			Encoding::reset();
		}
	};

	struct LiveRecording : public Encoding
	{
		bool _killedBecauseOfNotWorking{}; // by monitorThread

		bool _monitoringEnabled{};

		bool _monitoringRealTimeInfoEnabled{}; // frame/size/time
		uintmax_t _lastOutputFfmpegFileSize{};
		int32_t _lastRealTimeFrame{};
		size_t _lastRealTimeSize{};
		long _lastRealTimeFrameRate{};
		double _lastRealTimeBitRate{};
		chrono::milliseconds _lastRealTimeTimeInMilliSeconds{};
		chrono::system_clock::time_point _realTimeLastChange;

		long _numberOfRestartBecauseOfFailure{};

		bool _externalEncoder{};
		json _encodingParametersRoot;
		json _ingestedParametersRoot;
		string _streamSourceType;
		string _chunksTranscoderStagingContentsPath;
		string _chunksNFSStagingContentsPath;
		string _segmentListFileName;
		string _recordedFileNamePrefix;
		string _lastRecordedAssetFileName;
		double _lastRecordedAssetDurationInSeconds{};
		int64_t _lastRecordedSegmentUtcStartTimeInMillisecs{};
		string _channelLabel;
		string _segmenterType;
		chrono::system_clock::time_point _recordingStart;

		bool _virtualVOD{};
		string _monitorVirtualVODManifestDirectoryPath; // used to build virtualVOD
		string _monitorVirtualVODManifestFileName;		// used to build virtualVOD
		string _virtualVODStagingContentsPath;
		int64_t _liveRecorderVirtualVODImageMediaItemKey{};

		void reset() override
		{
			_encodingParametersRoot = nullptr;
			_channelLabel = "";
			_killedBecauseOfNotWorking = false;

			Encoding::reset();
		}

		[[nodiscard]] shared_ptr<LiveRecording> cloneForMonitorAndVirtualVOD() const
		{
			auto liveRecording = make_shared<LiveRecording>();

			liveRecording->_available = _available;
			liveRecording->_childProcessId = _childProcessId;
			liveRecording->_killToRestartByEngine = _killToRestartByEngine;
			liveRecording->_monitoringEnabled = _monitoringEnabled;
			liveRecording->_monitoringRealTimeInfoEnabled = _monitoringRealTimeInfoEnabled;
			liveRecording->_lastOutputFfmpegFileSize = _lastOutputFfmpegFileSize;
			liveRecording->_lastRealTimeFrame = _lastRealTimeFrame;
			liveRecording->_lastRealTimeSize = _lastRealTimeSize;
			liveRecording->_lastRealTimeFrameRate = _lastRealTimeFrameRate;
			liveRecording->_lastRealTimeBitRate = _lastRealTimeBitRate;
			liveRecording->_lastRealTimeTimeInMilliSeconds = _lastRealTimeTimeInMilliSeconds;
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

			liveRecording->_callbackData = _callbackData->clone();

			return liveRecording;
		}
	};

	struct EncodingCompleted
	{
		int64_t _encodingJobKey;
		bool _completedWithError;
		// string _errorMessage;
		bool _killedByUser;
		bool _killToRestartByEngine;
		// bool _urlForbidden;
		// bool _urlNotFound;
		chrono::system_clock::time_point _timestamp;

		shared_ptr<FFMpegEngine::CallbackData> _callbackData;
	};

  public:
	explicit FFMPEGEncoderBase(json configurationRoot);
	~FFMPEGEncoderBase();

  protected:
	int64_t _mmsAPITimeoutInSeconds;
	int64_t _mmsBinaryTimeoutInSeconds;

	static long getAddContentIngestionJobKey(int64_t ingestionJobKey, string ingestionResponse);
};
