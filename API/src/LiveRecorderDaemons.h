
#ifndef LiveRecorderDaemons_h
#define LiveRecorderDaemons_h

#include "FFMPEGEncoderBase.h"

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


class LiveRecorderDaemons: public FFMPEGEncoderBase {

	public:
		LiveRecorderDaemons(
			Json::Value configuration,
			mutex* liveRecordingMutex,                                                                            
			vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>>* liveRecordingsCapability,
			shared_ptr<spdlog::logger> logger);
		~LiveRecorderDaemons();

		void startChunksIngestionThread();

		void stopChunksIngestionThread();

		void startVirtualVODIngestionThread();

		void stopVirtualVODIngestionThread();

	private:
		mutex*		_liveRecordingMutex;
		vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>>* _liveRecordingsCapability;

		int			_liveRecorderChunksIngestionCheckInSeconds;
		string		_liveRecorderVirtualVODRetention;
		int			_liveRecorderVirtualVODIngestionInSeconds;
		bool		_liveRecorderChunksIngestionThreadShutdown;
		bool		_liveRecorderVirtualVODIngestionThreadShutdown;



		tuple<string, double, int64_t> processStreamSegmenterOutput(
			int64_t ingestionJobKey, int64_t encodingJobKey,
			string streamSourceType,
			bool externalEncoder,
			int segmentDurationInSeconds, string outputFileFormat,
			Json::Value encodingParametersRoot,
			Json::Value ingestedParametersRoot,
			string chunksTranscoderStagingContentsPath,
			string chunksNFSStagingContentsPath,
			string segmentListFileName,
			string recordedFileNamePrefix,
			string lastRecordedAssetFileName,
			double lastRecordedAssetDurationInSeconds,
			int64_t lastRecordedSegmentUtcStartTimeInMillisecs);

		tuple<string, double, int64_t> processHLSSegmenterOutput(
			int64_t ingestionJobKey, int64_t encodingJobKey,
			string streamSourceType,
			bool externalEncoder,
			int segmentDurationInSeconds, string outputFileFormat,
			Json::Value encodingParametersRoot,
			Json::Value ingestedParametersRoot,
			string chunksTranscoderStagingContentsPath,
			string chunksNFSStagingContentsPath,
			string segmentListFileName,
			string recordedFileNamePrefix,
			string lastRecordedAssetFileName,
			double lastRecordedAssetDurationInSeconds,
			int64_t lastRecordedSegmentUtcStartTimeInMillisecs);

		void ingestRecordedMediaInCaseOfInternalTranscoder(
			int64_t ingestionJobKey,
			string chunksTranscoderStagingContentsPath, string currentRecordedAssetFileName,
			string chunksNFSStagingContentsPath,
			string addContentTitle,
			string uniqueName,
			// bool highAvailability,
			Json::Value userDataRoot,
			string fileFormat,
			Json::Value ingestedParametersRoot,
			Json::Value encodingParametersRoot,
			bool copy);

		void ingestRecordedMediaInCaseOfExternalTranscoder(
			int64_t ingestionJobKey,
			string chunksTranscoderStagingContentsPath, string currentRecordedAssetFileName,
			string addContentTitle,
			string uniqueName,
			Json::Value userDataRoot,
			string fileFormat,
			Json::Value ingestedParametersRoot,
			Json::Value encodingParametersRoot);

		string buildChunkIngestionWorkflow(
			int64_t ingestionJobKey,
			bool externalEncoder,
			string currentRecordedAssetFileName,
			string chunksNFSStagingContentsPath,
			string addContentTitle,
			string uniqueName,
			Json::Value userDataRoot,
			string fileFormat,
			Json::Value ingestedParametersRoot,
			Json::Value encodingParametersRoot);

		bool isLastLiveRecorderFile(
			int64_t ingestionJobKey, int64_t encodingJobKey,
			time_t utcCurrentRecordedFileCreationTime, string chunksTranscoderStagingContentsPath,
			string recordedFileNamePrefix, int segmentDurationInSeconds, bool isFirstChunk);

		time_t getMediaLiveRecorderStartTime(
			int64_t ingestionJobKey, int64_t encodingJobKey,
			string mediaLiveRecorderFileName, int segmentDurationInSeconds,
			bool isFirstChunk);

		time_t getMediaLiveRecorderEndTime(
			int64_t ingestionJobKey, int64_t encodingJobKey,
			string mediaLiveRecorderFileName);

		long buildAndIngestVirtualVOD(
			int64_t liveRecorderIngestionJobKey,
			int64_t liveRecorderEncodingJobKey,
			bool externalEncoder,

			string sourceSegmentsDirectoryPathName,
			string sourceManifestFileName,
			// /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/.../content
			string stagingLiveRecorderVirtualVODPathName,

			int64_t recordingCode,
			string liveRecorderIngestionJobLabel,
			string liveRecorderVirtualVODUniqueName,
			string liveRecorderVirtualVODRetention,
			int64_t liveRecorderVirtualVODImageMediaItemKey,
			int64_t liveRecorderUserKey,
			string liveRecorderApiKey,
			string mmsWorkflowIngestionURL,
			string mmsBinaryIngestionURL);

		string buildVirtualVODIngestionWorkflow(
			int64_t liveRecorderIngestionJobKey,
			int64_t liveRecorderEncodingJobKey,
			bool externalEncoder,

			int64_t utcStartTimeInMilliSecs,
			int64_t utcEndTimeInMilliSecs,
			int64_t recordingCode,
			string liveRecorderIngestionJobLabel,
			string tarGzStagingLiveRecorderVirtualVODPathName,
			string liveRecorderVirtualVODUniqueName,
			string liveRecorderVirtualVODRetention,
			int64_t liveRecorderVirtualVODImageMediaItemKey);
};

#endif
