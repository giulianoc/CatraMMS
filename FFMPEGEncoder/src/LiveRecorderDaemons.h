
#pragma once

#include "FFMPEGEncoderBase.h"

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "FFMpegWrapper.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <string>

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename((char *)__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

class LiveRecorderDaemons : public FFMPEGEncoderBase
{

  public:
	LiveRecorderDaemons(
		nlohmann::json configurationRoot, std::mutex *liveRecordingMutex, std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveRecording>> *liveRecordingsCapability
	);
	~LiveRecorderDaemons();

	void startChunksIngestionThread();

	void stopChunksIngestionThread();

	void startVirtualVODIngestionThread();

	void stopVirtualVODIngestionThread();

  private:
	std::mutex *_liveRecordingMutex;
	std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveRecording>> *_liveRecordingsCapability;

	int _liveRecorderChunksIngestionCheckInSeconds;
	std::string _liveRecorderVirtualVODRetention;
	int _liveRecorderVirtualVODIngestionInSeconds;
	bool _liveRecorderChunksIngestionThreadShutdown;
	bool _liveRecorderVirtualVODIngestionThreadShutdown;

	std::tuple<std::string, double, int64_t> processStreamSegmenterOutput(
		int64_t ingestionJobKey, int64_t encodingJobKey, std::string streamSourceType, bool externalEncoder, int segmentDurationInSeconds,
		std::string outputFileFormat, nlohmann::json encodingParametersRoot, nlohmann::json ingestedParametersRoot, std::string chunksTranscoderStagingContentsPath,
		std::string chunksNFSStagingContentsPath, std::string segmentListFileName, std::string recordedFileNamePrefix, std::string lastRecordedAssetFileName,
		double lastRecordedAssetDurationInSeconds, int64_t lastRecordedSegmentUtcStartTimeInMillisecs
	);

	std::tuple<std::string, double, int64_t> processHLSSegmenterOutput(
		int64_t ingestionJobKey, int64_t encodingJobKey, std::string streamSourceType, bool externalEncoder, int segmentDurationInSeconds,
		std::string outputFileFormat, nlohmann::json encodingParametersRoot, nlohmann::json ingestedParametersRoot, std::string chunksTranscoderStagingContentsPath,
		std::string chunksNFSStagingContentsPath, std::string segmentListFileName, std::string recordedFileNamePrefix, std::string lastRecordedAssetFileName,
		double lastRecordedAssetDurationInSeconds, int64_t lastRecordedSegmentUtcStartTimeInMillisecs
	);

	void ingestRecordedMediaInCaseOfInternalTranscoder(
		int64_t ingestionJobKey, std::string chunksTranscoderStagingContentsPath, std::string currentRecordedAssetFileName, std::string chunksNFSStagingContentsPath,
		std::string addContentTitle, std::string uniqueName,
		// bool highAvailability,
		nlohmann::json userDataRoot, std::string fileFormat, nlohmann::json ingestedParametersRoot, nlohmann::json encodingParametersRoot, bool copy
	);

	void ingestRecordedMediaInCaseOfExternalTranscoder(
		int64_t ingestionJobKey, std::string chunksTranscoderStagingContentsPath, std::string currentRecordedAssetFileName, std::string addContentTitle,
		std::string uniqueName, nlohmann::json userDataRoot, std::string fileFormat, nlohmann::json ingestedParametersRoot, nlohmann::json encodingParametersRoot
	);

	std::string buildChunkIngestionWorkflow(
		int64_t ingestionJobKey, bool externalEncoder, std::string currentRecordedAssetFileName, std::string chunksNFSStagingContentsPath,
		std::string addContentTitle, std::string uniqueName, nlohmann::json userDataRoot, std::string fileFormat, nlohmann::json ingestedParametersRoot,
		nlohmann::json encodingParametersRoot
	);

	bool isLastLiveRecorderFile(
		int64_t ingestionJobKey, int64_t encodingJobKey, time_t utcCurrentRecordedFileCreationTime, std::string chunksTranscoderStagingContentsPath,
		std::string recordedFileNamePrefix, int segmentDurationInSeconds, bool isFirstChunk
	);

	time_t getMediaLiveRecorderStartTime(
		int64_t ingestionJobKey, int64_t encodingJobKey, std::string mediaLiveRecorderFileName, int segmentDurationInSeconds, bool isFirstChunk
	);

	time_t getMediaLiveRecorderEndTime(int64_t ingestionJobKey, int64_t encodingJobKey, std::string mediaLiveRecorderFileName);

	long buildAndIngestVirtualVOD(
		int64_t liveRecorderIngestionJobKey, int64_t liveRecorderEncodingJobKey, bool externalEncoder,

		std::string sourceSegmentsDirectoryPathName, std::string sourceManifestFileName,
		// /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/.../content
		std::string stagingLiveRecorderVirtualVODPathName,

		int64_t recordingCode, std::string liveRecorderIngestionJobLabel, std::string liveRecorderVirtualVODUniqueName, std::string liveRecorderVirtualVODRetention,
		int64_t liveRecorderVirtualVODImageMediaItemKey, int64_t liveRecorderUserKey, std::string liveRecorderApiKey, std::string mmsWorkflowIngestionURL,
		std::string mmsBinaryIngestionURL
	);

	std::string buildVirtualVODIngestionWorkflow(
		int64_t liveRecorderIngestionJobKey, int64_t liveRecorderEncodingJobKey, bool externalEncoder,

		int64_t utcStartTimeInMilliSecs, int64_t utcEndTimeInMilliSecs, int64_t recordingCode, std::string liveRecorderIngestionJobLabel,
		std::string tarGzStagingLiveRecorderVirtualVODPathName, std::string liveRecorderVirtualVODUniqueName, std::string liveRecorderVirtualVODRetention,
		int64_t liveRecorderVirtualVODImageMediaItemKey
	);
};
