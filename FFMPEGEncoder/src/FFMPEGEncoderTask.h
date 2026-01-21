
#pragma once

#include "FFMPEGEncoderBase.h"

#include "FFMpegWrapper.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <string>

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ std::string("[") + std::string(__FILE__).substr(std::string(__FILE__).find_last_of("/") + 1) + ":" + to_std::string(__LINE__) + "] "
#else
#define __FILEREF__ std::string("[") + basename((char *)__FILE__) + ":" + to_std::string(__LINE__) + "] "
#endif
#endif

class FFMPEGEncoderTask : public FFMPEGEncoderBase
{

  public:
	FFMPEGEncoderTask(
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const nlohmann::json& configurationRoot,
		std::mutex *encodingCompletedMutex, std::map<int64_t, std::shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *encodingCompletedMap
	);
	~FFMPEGEncoderTask();

  private:
	std::mutex *_encodingCompletedMutex;
	std::map<int64_t, std::shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *_encodingCompletedMap;

	std::string _tvChannelConfigurationDirectory;

	void addEncodingCompleted() const;
	void removeEncodingCompletedIfPresent() const;

	int64_t ingestContentByPushingBinary(
		int64_t ingestionJobKey, std::string workflowMetadata, std::string fileFormat, std::string binaryPathFileName, int64_t binaryFileSizeInBytes,
		int64_t userKey, const std::string &apiKey, std::string mmsWorkflowIngestionURL, std::string mmsBinaryIngestionURL
	) const;

	int progressDownloadCallback(
		int64_t ingestionJobKey, std::chrono::system_clock::time_point &lastTimeProgressUpdate, double &lastPercentageUpdated, double dltotal,
		double dlnow, double ultotal, double ulnow
	);

  protected:
	std::shared_ptr<FFMPEGEncoderBase::Encoding> _encoding;

	bool _completedWithError;
	bool _killedByUser;
	// bool _urlForbidden;
	// bool _urlNotFound;

	long _tvChannelPort_Start;
	long _tvChannelPort_MaxNumberOfOffsets;

	// void ffmpegLineCallback(const std::string_view& ffmpegLine) const;

	static std::string buildAddContentIngestionWorkflow(
		int64_t ingestionJobKey, std::string label, std::string fileFormat, std::string ingester,

		// in case of a new content
		std::string sourceURL, // if empty it means binary is ingested later (PUSH)
		std::string title, const nlohmann::json& userDataRoot,
		const nlohmann::json& ingestedParametersRoot, // it could be also nullValue

		int64_t encodingProfileKey,

		int64_t variantOfMediaItemKey = -1 // in case of a variant, otherwise -1
	);

	void uploadLocalMediaToMMS(
		int64_t ingestionJobKey, int64_t encodingJobKey, nlohmann::json ingestedParametersRoot, const nlohmann::json &encodingProfileDetailsRoot,
		const nlohmann::json &encodingParametersRoot, std::string sourceFileExtension, const std::string &encodedStagingAssetPathName, const std::string &workflowLabel,
		const std::string &ingester, int64_t encodingProfileKey,
		int64_t variantOfMediaItemKey = -1 // in case Media is a variant of a MediaItem already present
	);

	static std::string downloadMediaFromMMS(
		int64_t ingestionJobKey, int64_t encodingJobKey, const std::shared_ptr<FFMpegWrapper> &ffmpeg, const std::string &sourceFileExtension,
		const std::string &sourcePhysicalDeliveryURL, const std::string &destAssetPathName
	);

	long getFreeTvChannelPortOffset(std::mutex *tvChannelsPortsMutex, long tvChannelPort_CurrentOffset) const;

	void createOrUpdateTVDvbLastConfigurationFile(
		int64_t ingestionJobKey, int64_t encodingJobKey, std::string multicastIP, std::string multicastPort, std::string tvType, int64_t tvServiceId,
		int64_t tvFrequency, int64_t tvSymbolRate, int64_t tvBandwidthInMhz, std::string tvModulation, int tvVideoPid, int tvAudioItalianPid,
		bool toBeAdded
	);

	[[nodiscard]] std::pair<std::string, std::string> getTVMulticastFromDvblastConfigurationFile(
		int64_t ingestionJobKey, int64_t encodingJobKey, std::string tvType, int64_t tvServiceId, int64_t tvFrequency, int64_t tvSymbolRate,
		int64_t tvBandwidthInMhz, std::string tvModulation
	) const;
};

