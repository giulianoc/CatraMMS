
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
		struct Progress {
			inline static const std::vector<string> errorPatterns = {
				"Invalid data found",
				"Error while decoding",
				"Connection refused",
				"Connection timed out",
				"Network is unreachable",
				"Protocol not found",
				"No such file",
				"Broken pipe",
				"Unknown encoder",
				"Invalid argument"
			};
			static constexpr int32_t maxErrorsStored = 50;

			ofstream ffmpegOutputLogFile;

			int32_t processedFrames{};
			double framePerSeconds{};
			chrono::milliseconds processedOutputTimestampMilliSecs{};
			double speed{}; // Utile per capire se il server sta performando bene
			int32_t dropFrames{};
			int32_t dupFrames{};
			double stream_0_0_q{};
			double stream_1_0_q{};
			size_t totalSizeKBps{};
			double bitRateKbps{};
			double avgBitRateKbps{};	// calculated by us

			bool finished{}; // progress=end

			queue<string> _errorMessages;

			Progress() = default;
			// Copy assignment operator that ignores ffmpegOutputLogFile
			Progress& operator=(const Progress& other)
			{
				if (this == &other)
					return *this;

				processedFrames = other.processedFrames;
				framePerSeconds = other.framePerSeconds;
				processedOutputTimestampMilliSecs = other.processedOutputTimestampMilliSecs;
				speed = other.speed;
				dropFrames = other.dropFrames;
				dupFrames = other.dupFrames;
				stream_0_0_q = other.stream_0_0_q;
				stream_1_0_q = other.stream_1_0_q;
				totalSizeKBps = other.totalSizeKBps;
				bitRateKbps = other.bitRateKbps;
				avgBitRateKbps = other.avgBitRateKbps;
				finished = other.finished;
				_errorMessages = other._errorMessages;

				// ffmpegOutputLogFile non viene copiato perchè non è copiabile (La sua copy-constructor è deleted)

				return *this;
			}

			void pushErrorMessage(const string& errorMessage)
			{
				if (_errorMessages.size() >= maxErrorsStored)
					_errorMessages.pop();
				_errorMessages.push(errorMessage);
			}

			void reset()
			{
				processedFrames = 0;
				framePerSeconds = 0.0;
				processedOutputTimestampMilliSecs = chrono::milliseconds(0);
				speed = 0.0;
				dropFrames = 0;
				dupFrames = 0;
				stream_0_0_q = 0.0;
				stream_1_0_q = 0.0;
				totalSizeKBps = 0;
				bitRateKbps = 0.0;
				avgBitRateKbps = 0.0;
				finished = false;

				while (!_errorMessages.empty())
					_errorMessages.pop();
			}

			json toJson()
			{
				json progressRoot;
				progressRoot["processedFrames"] = processedFrames;
				progressRoot["framePerSeconds"] = framePerSeconds;
				progressRoot["processedOutputTimestampMilliSecs"] = processedOutputTimestampMilliSecs.count();
				progressRoot["speed"] = speed;
				progressRoot["dropFrames"] = dropFrames;
				progressRoot["dupFrames"] = dupFrames;
				progressRoot["stream_0_0_q"] = stream_0_0_q;
				progressRoot["stream_1_0_q"] = stream_1_0_q;
				progressRoot["totalSizeKBps"] = totalSizeKBps;
				progressRoot["bitRateKbps"] = bitRateKbps;
				progressRoot["avgBitRateKbps"] = avgBitRateKbps;
				progressRoot["finished"] = finished;

				json errorMessagesRoot = json::array();
				auto tmp = _errorMessages;   // copia della queue
				while (!tmp.empty()) {
					errorMessagesRoot.push_back(tmp.front());
					tmp.pop();
				}
				progressRoot["errorMessages"] = errorMessagesRoot;
				return progressRoot;
			}
		};

		string _method;
		bool _available{};
		ProcessUtility::ProcessId _childProcessId;
		int64_t _encodingJobKey{};
		shared_ptr<FFMpegWrapper> _ffmpeg;
		bool _ffmpegTerminatedSuccessful{};
		bool _killToRestartByEngine{};

		mutex _errorMessagesMutex;
		queue<string> _errorMessages;
		string _lastErrorMessage;

		shared_mutex _progressMutex;
		Progress _progress;

		void initEncoding(const int64_t encodingJobKey, const string_view& method)
		{
			_available = false;
			_method = method;
			_childProcessId.reset(); // not running
			_killToRestartByEngine = false;
			_encodingJobKey = encodingJobKey;
			_ffmpegTerminatedSuccessful = false;
		}

		void resetEncoding()
		{
			unique_lock lock(_progressMutex);

			_available = true;
			_childProcessId.reset(); // not running
			_progress.reset();
		}

		void pushErrorMessage(const string& errorMessage)
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
		bool _killedBecauseOfNotWorking{}; // by monitorThread

		// string					_liveGridOutputType;	// only for LiveGrid
		json _outputsRoot;

		bool _monitoringRealTimeInfoEnabled{}; // frame/size/time
		uintmax_t _outputFfmpegFileSize{};
		long _realTimeFrame{};
		long _realTimeSize{};
		long _realTimeFrameRate{};
		double _realTimeBitRate{};
		double _realTimeTimeInMilliSeconds{};
		chrono::system_clock::time_point _realTimeLastChange;

		long _numberOfRestartBecauseOfFailure{};

		int64_t _ingestionJobKey{};
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
			liveProxyAndGrid->_outputFfmpegFileSize = _outputFfmpegFileSize;
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
		bool _killedBecauseOfNotWorking{}; // by monitorThread

		bool _monitoringEnabled{};

		bool _monitoringRealTimeInfoEnabled{}; // frame/size/time
		uintmax_t _outputFfmpegFileSize{};
		long _realTimeFrame{};
		long _realTimeSize{};
		long _realTimeFrameRate{};
		double _realTimeBitRate{};
		double _realTimeTimeInMilliSeconds{};
		chrono::system_clock::time_point _realTimeLastChange;

		long _numberOfRestartBecauseOfFailure{};

		int64_t _ingestionJobKey{};
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

		[[nodiscard]] shared_ptr<LiveRecording> cloneForMonitorAndVirtualVOD() const
		{
			auto liveRecording = make_shared<LiveRecording>();

			liveRecording->_available = _available;
			liveRecording->_childProcessId = _childProcessId;
			liveRecording->_killToRestartByEngine = _killToRestartByEngine;
			liveRecording->_monitoringEnabled = _monitoringEnabled;
			liveRecording->_monitoringRealTimeInfoEnabled = _monitoringRealTimeInfoEnabled;
			liveRecording->_outputFfmpegFileSize = _outputFfmpegFileSize;
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

		Encoding::Progress _progress;
	};

  public:
	explicit FFMPEGEncoderBase(json configurationRoot);
	~FFMPEGEncoderBase();

  protected:
	int64_t _mmsAPITimeoutInSeconds;
	int64_t _mmsBinaryTimeoutInSeconds;

	long getAddContentIngestionJobKey(int64_t ingestionJobKey, string ingestionResponse);
};
