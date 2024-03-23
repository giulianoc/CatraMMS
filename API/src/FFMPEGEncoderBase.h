
#ifndef FFMPEGEncoderBase_h
#define FFMPEGEncoderBase_h

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include "FFMpeg.h"
#include <string>
#include <chrono>

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif


class FFMPEGEncoderBase {

	public:
		struct Encoding
		{
			bool					_available;
			pid_t					_childPid;
			int64_t					_encodingJobKey;
			shared_ptr<FFMpeg>		_ffmpeg;
			bool					_ffmpegTerminatedSuccessful;
			string					_errorMessage;
		};

		struct LiveProxyAndGrid: public Encoding
		{
			string					_method;	// liveProxy, liveGrid or awaitingTheBeginning
			bool					_killedBecauseOfNotWorking;	// by monitorThread

			// string					_liveGridOutputType;	// only for LiveGrid
			Json::Value				_outputsRoot;

			int64_t					_ingestionJobKey;
			Json::Value				_encodingParametersRoot;
			Json::Value				_ingestedParametersRoot;

			Json::Value				_inputsRoot;
			mutex					_inputsRootMutex;

			chrono::system_clock::time_point	_proxyStart;

			shared_ptr<LiveProxyAndGrid> cloneForMonitor()
			{
				shared_ptr<LiveProxyAndGrid> liveProxyAndGrid =
					make_shared<LiveProxyAndGrid>();

				liveProxyAndGrid->_available = _available;
				liveProxyAndGrid->_childPid = _childPid;
				liveProxyAndGrid->_encodingJobKey = _encodingJobKey;
				liveProxyAndGrid->_method = _method;
				liveProxyAndGrid->_ffmpeg = _ffmpeg;
				liveProxyAndGrid->_killedBecauseOfNotWorking = _killedBecauseOfNotWorking;
				liveProxyAndGrid->_errorMessage = _errorMessage;
				// liveProxyAndGrid->_liveGridOutputType = _liveGridOutputType;

				liveProxyAndGrid->_ingestionJobKey = _ingestionJobKey;

				liveProxyAndGrid->_outputsRoot = _outputsRoot;
				liveProxyAndGrid->_encodingParametersRoot = _encodingParametersRoot;
				liveProxyAndGrid->_ingestedParametersRoot = _ingestedParametersRoot;
				liveProxyAndGrid->_inputsRoot = _inputsRoot;

				liveProxyAndGrid->_proxyStart = _proxyStart;

				return liveProxyAndGrid;
			}
		};

		struct LiveRecording: public Encoding
		{
			bool					_killedBecauseOfNotWorking;	// by monitorThread

			bool					_monitoringEnabled;
			bool					_monitoringFrameIncreasingEnabled;

			int64_t					_ingestionJobKey;
			bool					_externalEncoder;
			Json::Value				_encodingParametersRoot;
			Json::Value				_ingestedParametersRoot;
			string					_streamSourceType;
			string					_chunksTranscoderStagingContentsPath;
			string					_chunksNFSStagingContentsPath;
			string					_segmentListFileName;
			string					_recordedFileNamePrefix;
			string					_lastRecordedAssetFileName;
			double					_lastRecordedAssetDurationInSeconds;
			int64_t					_lastRecordedSegmentUtcStartTimeInMillisecs;
			string					_channelLabel;
			string					_segmenterType;
			chrono::system_clock::time_point	_recordingStart;

			bool					_virtualVOD;
			string					_monitorVirtualVODManifestDirectoryPath;	// used to build virtualVOD
			string					_monitorVirtualVODManifestFileName;			// used to build virtualVOD
			string					_virtualVODStagingContentsPath;
			int64_t					_liveRecorderVirtualVODImageMediaItemKey;

			shared_ptr<LiveRecording> cloneForMonitorAndVirtualVOD()
			{
				shared_ptr<LiveRecording> liveRecording =
					make_shared<LiveRecording>();

				liveRecording->_available = _available;
				liveRecording->_childPid = _childPid;
				liveRecording->_monitoringEnabled = _monitoringEnabled;
				liveRecording->_monitoringFrameIncreasingEnabled = _monitoringFrameIncreasingEnabled;
				liveRecording->_encodingJobKey = _encodingJobKey;
				liveRecording->_externalEncoder = _externalEncoder;
				liveRecording->_ffmpeg = _ffmpeg;
				liveRecording->_killedBecauseOfNotWorking = _killedBecauseOfNotWorking;
				liveRecording->_errorMessage = _errorMessage;
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
				int64_t					_encodingJobKey;
				bool					_completedWithError;
				string					_errorMessage;
				bool					_killedByUser;
				bool					_urlForbidden;
				bool					_urlNotFound;
				chrono::system_clock::time_point	_timestamp;
		};


	public:
		FFMPEGEncoderBase(
			Json::Value configuration,
			shared_ptr<spdlog::logger> logger);
		~FFMPEGEncoderBase();

	private:

	protected:
		shared_ptr<spdlog::logger>		_logger;

		int64_t				_mmsAPITimeoutInSeconds;
		int64_t				_mmsBinaryTimeoutInSeconds;


		long getAddContentIngestionJobKey(
			int64_t ingestionJobKey,
			string ingestionResponse);

};

#endif
