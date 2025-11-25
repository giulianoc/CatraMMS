
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

class FFMPEGEncoderTask : public FFMPEGEncoderBase
{

  public:
	FFMPEGEncoderTask(
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const json& configurationRoot,
		mutex *encodingCompletedMutex, map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *encodingCompletedMap
	);
	~FFMPEGEncoderTask();

  private:
	mutex *_encodingCompletedMutex;
	map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *_encodingCompletedMap;

	string _tvChannelConfigurationDirectory;

	void addEncodingCompleted();
	void removeEncodingCompletedIfPresent() const;

	int64_t ingestContentByPushingBinary(
		int64_t ingestionJobKey, string workflowMetadata, string fileFormat, string binaryPathFileName, int64_t binaryFileSizeInBytes,
		int64_t userKey, const string &apiKey, string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL
	) const;

	int progressDownloadCallback(
		int64_t ingestionJobKey, chrono::system_clock::time_point &lastTimeProgressUpdate, double &lastPercentageUpdated, double dltotal,
		double dlnow, double ultotal, double ulnow
	);

  protected:
	shared_ptr<FFMPEGEncoderBase::Encoding> _encoding;

	bool _completedWithError;
	bool _killedByUser;
	// bool _urlForbidden;
	// bool _urlNotFound;

	long _tvChannelPort_Start;
	long _tvChannelPort_MaxNumberOfOffsets;

	// void ffmpegLineCallback(const string_view& ffmpegLine) const;

	static string buildAddContentIngestionWorkflow(
		int64_t ingestionJobKey, string label, string fileFormat, string ingester,

		// in case of a new content
		string sourceURL, // if empty it means binary is ingested later (PUSH)
		string title, const json& userDataRoot,
		const json& ingestedParametersRoot, // it could be also nullValue

		int64_t encodingProfileKey,

		int64_t variantOfMediaItemKey = -1 // in case of a variant, otherwise -1
	);

	void uploadLocalMediaToMMS(
		int64_t ingestionJobKey, int64_t encodingJobKey, json ingestedParametersRoot, const json &encodingProfileDetailsRoot,
		const json &encodingParametersRoot, string sourceFileExtension, const string &encodedStagingAssetPathName, const string &workflowLabel,
		const string &ingester, int64_t encodingProfileKey,
		int64_t variantOfMediaItemKey = -1 // in case Media is a variant of a MediaItem already present
	);

	static string downloadMediaFromMMS(
		int64_t ingestionJobKey, int64_t encodingJobKey, const shared_ptr<FFMpegWrapper> &ffmpeg, const string &sourceFileExtension,
		const string &sourcePhysicalDeliveryURL, const string &destAssetPathName
	);

	long getFreeTvChannelPortOffset(mutex *tvChannelsPortsMutex, long tvChannelPort_CurrentOffset) const;

	void createOrUpdateTVDvbLastConfigurationFile(
		int64_t ingestionJobKey, int64_t encodingJobKey, string multicastIP, string multicastPort, string tvType, int64_t tvServiceId,
		int64_t tvFrequency, int64_t tvSymbolRate, int64_t tvBandwidthInMhz, string tvModulation, int tvVideoPid, int tvAudioItalianPid,
		bool toBeAdded
	);

	[[nodiscard]] pair<string, string> getTVMulticastFromDvblastConfigurationFile(
		int64_t ingestionJobKey, int64_t encodingJobKey, string tvType, int64_t tvServiceId, int64_t tvFrequency, int64_t tvSymbolRate,
		int64_t tvBandwidthInMhz, string tvModulation
	) const;
};

