
#pragma once

#include "ProcessUtility.h"
#include <cstdint>
#include <fstream>
#include <shared_mutex>
#include "FFMpegWrapper.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <queue>
#include <string>

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ std::string("[") + std::string(__FILE__).substr(std::string(__FILE__).find_last_of("/") + 1) + ":" + to_std::string(__LINE__) + "] "
#else
#define __FILEREF__ std::string("[") + basename((char *)__FILE__) + ":" + to_std::string(__LINE__) + "] "
#endif
#endif

class FFMPEGEncoderBase
{
  public:
	struct Encoding
	{
		virtual ~Encoding() = default;

		void initEncoding(const int64_t encodingJobKey, const std::string_view& method)
		{
			_available = false;
			_method = method;
			_childProcessId.reset(); // not running
			_killTypeReceived = FFMpegWrapper::KillType::None;
			_encodingJobKey = encodingJobKey;
			_ffmpegTerminatedSuccessful = false;
			_encodingStart = std::nullopt;
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

		std::shared_ptr<FFMpegEngine::CallbackData> _callbackData;

		std::string _method;
		bool _available{};
		ProcessUtility::ProcessId _childProcessId;
		int64_t _ingestionJobKey{};
		int64_t _encodingJobKey{};
		std::shared_ptr<FFMpegWrapper> _ffmpeg;
		bool _ffmpegTerminatedSuccessful{};
		// quando inizia l'encoding, dopo la preparazione dei contenuti che in caso di externalContent può essere lunga per il download
		std::optional<std::chrono::system_clock::time_point> _encodingStart;
		FFMpegWrapper::KillType _killTypeReceived = FFMpegWrapper::KillType::None;
	};

	struct LiveProxyAndGrid final : public Encoding
	{
		bool _killedBecauseOfNotWorking{}; // by monitorThread

		// std::string					_liveGridOutputType;	// only for LiveGrid
		nlohmann::json _outputsRoot;

		bool _monitoringRealTimeInfoEnabled{};
		// frames, time, size, bitrate, framerate
		std::tuple<int32_t, std::chrono::milliseconds, size_t, double, double> _lastRealTimeInfo{};
		// int32_t _lastRealTimeFrame{};
		// chrono::milliseconds _lastRealTimeTimeInMilliSeconds{};
		// size_t _lastRealTimeSize{};
		// double _lastRealTimeBitRate{};
		// long _lastRealTimeFrameRate{};
		std::chrono::system_clock::time_point _realTimeLastChange;

		long _numberOfRestartBecauseOfFailure{};

		nlohmann::json _encodingParametersRoot;
		nlohmann::json _ingestedParametersRoot;

		nlohmann::json _inputsRoot;
		std::mutex _inputsRootMutex;

		// chrono::system_clock::time_point _proxyStart;

		[[nodiscard]] std::shared_ptr<LiveProxyAndGrid> cloneForMonitor() const
		{
			auto liveProxyAndGrid = std::make_shared<LiveProxyAndGrid>();

			liveProxyAndGrid->_available = _available;
			liveProxyAndGrid->_childProcessId = _childProcessId;
			liveProxyAndGrid->_killTypeReceived = _killTypeReceived;
			liveProxyAndGrid->_monitoringRealTimeInfoEnabled = _monitoringRealTimeInfoEnabled;
			liveProxyAndGrid->_lastRealTimeInfo = _lastRealTimeInfo;
			// liveProxyAndGrid->_lastOutputFfmpegFileSize = _lastOutputFfmpegFileSize;
			// liveProxyAndGrid->_lastRealTimeFrame = _lastRealTimeFrame;
			// liveProxyAndGrid->_lastRealTimeSize = _lastRealTimeSize;
			// liveProxyAndGrid->_lastRealTimeFrameRate = _lastRealTimeFrameRate;
			// liveProxyAndGrid->_lastRealTimeBitRate = _lastRealTimeBitRate;
			// liveProxyAndGrid->_lastRealTimeTimeInMilliSeconds = _lastRealTimeTimeInMilliSeconds;
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

			// liveProxyAndGrid->_proxyStart = _proxyStart;
			liveProxyAndGrid->_encodingStart = _encodingStart;

			return liveProxyAndGrid;
		}

		void reset() override
		{
			_encodingParametersRoot = nullptr;
			_killedBecauseOfNotWorking = false;
			_lastRealTimeInfo = {};

			Encoding::reset();
		}
	};

	struct LiveRecording final : public Encoding
	{
		bool _killedBecauseOfNotWorking{}; // by monitorThread

		bool _monitoringEnabled{};

		bool _monitoringRealTimeInfoEnabled{};
		// frames, time, size, bitrate, framerate
		std::tuple<int32_t, std::chrono::milliseconds, size_t, double, double> _lastRealTimeInfo{};
		// int32_t _lastRealTimeFrame{};
		// chrono::milliseconds _lastRealTimeTimeInMilliSeconds{};
		// size_t _lastRealTimeSize{};
		// double _lastRealTimeBitRate{};
		// long _lastRealTimeFrameRate{};
		uintmax_t _lastOutputFfmpegFileSize{};
		std::chrono::system_clock::time_point _realTimeLastChange;

		long _numberOfRestartBecauseOfFailure{};

		bool _externalEncoder{};
		nlohmann::json _encodingParametersRoot;
		nlohmann::json _ingestedParametersRoot;
		std::string _streamSourceType;
		std::string _chunksTranscoderStagingContentsPath;
		std::string _chunksNFSStagingContentsPath;
		std::string _segmentListFileName;
		std::string _recordedFileNamePrefix;
		std::string _lastRecordedAssetFileName;
		double _lastRecordedAssetDurationInSeconds{};
		int64_t _lastRecordedSegmentUtcStartTimeInMillisecs{};
		std::string _channelLabel;
		std::string _segmenterType;
		// chrono::system_clock::time_point _recordingStart;

		bool _virtualVOD{};
		std::string _monitorVirtualVODManifestDirectoryPath; // used to build virtualVOD
		std::string _monitorVirtualVODManifestFileName;		// used to build virtualVOD
		std::string _virtualVODStagingContentsPath;
		int64_t _liveRecorderVirtualVODImageMediaItemKey{};

		void reset() override
		{
			_encodingParametersRoot = nullptr;
			_channelLabel = "";
			_killedBecauseOfNotWorking = false;
			_lastRealTimeInfo = {};

			Encoding::reset();
		}

		[[nodiscard]] std::shared_ptr<LiveRecording> cloneForMonitorAndVirtualVOD() const
		{
			auto liveRecording = std::make_shared<LiveRecording>();

			liveRecording->_available = _available;
			liveRecording->_childProcessId = _childProcessId;
			liveRecording->_killTypeReceived = _killTypeReceived;
			liveRecording->_monitoringEnabled = _monitoringEnabled;
			liveRecording->_monitoringRealTimeInfoEnabled = _monitoringRealTimeInfoEnabled;
			liveRecording->_lastRealTimeInfo = _lastRealTimeInfo;
			liveRecording->_lastOutputFfmpegFileSize = _lastOutputFfmpegFileSize;
			// liveRecording->_lastRealTimeFrame = _lastRealTimeFrame;
			// liveRecording->_lastRealTimeSize = _lastRealTimeSize;
			// liveRecording->_lastRealTimeFrameRate = _lastRealTimeFrameRate;
			// liveRecording->_lastRealTimeBitRate = _lastRealTimeBitRate;
			// liveRecording->_lastRealTimeTimeInMilliSeconds = _lastRealTimeTimeInMilliSeconds;
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
			// liveRecording->_recordingStart = _recordingStart;
			liveRecording->_encodingStart = _encodingStart;
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
		bool _killedByUser;
		FFMpegWrapper::KillType _killTypeReceived;
		std::chrono::system_clock::time_point _timestamp;

		std::shared_ptr<FFMpegEngine::CallbackData> _callbackData;
	};

  public:
	explicit FFMPEGEncoderBase(nlohmann::json configurationRoot);
	~FFMPEGEncoderBase();

  protected:
	std::string _mmsIngestionURL;
	std::string _mmsWorkflowIngestionURL;
	std::string _mmsBinaryIngestionURL;
	int64_t _mmsAPITimeoutInSeconds;
	int64_t _mmsBinaryTimeoutInSeconds;

	static long getAddContentIngestionJobKey(int64_t ingestionJobKey, std::string ingestionResponse);
};
