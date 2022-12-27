
#ifndef FFMPEGEncoderTask_h
#define FFMPEGEncoderTask_h

#include "FFMPEGEncoderBase.h"

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


class FFMPEGEncoderTask: public FFMPEGEncoderBase {

	public:
		FFMPEGEncoderTask(
			shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
			int64_t encodingJobKey,
			Json::Value configuration,
			mutex* encodingCompletedMutex,                                                                        
			map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>* encodingCompletedMap,                                    
			shared_ptr<spdlog::logger> logger);
		~FFMPEGEncoderTask();

	private:
		mutex*				_encodingCompletedMutex;
		map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>*	_encodingCompletedMap;

		string				_tvChannelConfigurationDirectory;


		void addEncodingCompleted();
		void removeEncodingCompletedIfPresent();

		int64_t ingestContentByPushingBinary(
			int64_t ingestionJobKey,
			string workflowMetadata,
			string fileFormat,
			string binaryPathFileName,
			int64_t binaryFileSizeInBytes,
			int64_t userKey,
			string apiKey,
			string mmsWorkflowIngestionURL,
			string mmsBinaryIngestionURL);

	protected:
		shared_ptr<FFMPEGEncoderBase::Encoding>	_encoding;
		int64_t				_encodingJobKey;

		bool				_completedWithError;
		bool				_killedByUser;
		bool				_urlForbidden;
		bool				_urlNotFound;

		long				_tvChannelPort_Start;
		long				_tvChannelPort_MaxNumberOfOffsets;


		string buildAddContentIngestionWorkflow(
			int64_t ingestionJobKey,
			string label,
			string fileFormat,
			string ingester,

			// in case of a new content
			string sourceURL,	// if empty it means binary is ingested later (PUSH)
			string title,
			Json::Value userDataRoot,
			Json::Value ingestedParametersRoot,	// it could be also nullValue

			// in case of a Variant
			int64_t variantOfMediaItemKey = -1,		// in case of a variant, otherwise -1
			int64_t variantEncodingProfileKey = -1);	// in case of a variant, otherwise -1

		void uploadLocalMediaToMMS(
			int64_t ingestionJobKey,
			int64_t encodingJobKey,
			Json::Value ingestedParametersRoot,
			Json::Value encodingProfileDetailsRoot,
			Json::Value encodingParametersRoot,
			string sourceFileExtension,
			string encodedStagingAssetPathName,
			string workflowLabel,
			string ingester,
			int64_t variantOfMediaItemKey = -1,	// in case Media is a variant of a MediaItem already present
			int64_t variantEncodingProfileKey = -1);	// in case Media is a variant of a MediaItem already present

		string downloadMediaFromMMS(
			int64_t ingestionJobKey,
			int64_t encodingJobKey,
			shared_ptr<FFMpeg> ffmpeg,
			string sourceFileExtension,
			string sourcePhysicalDeliveryURL,
			string destAssetPathName);

		void createOrUpdateTVDvbLastConfigurationFile(
			int64_t ingestionJobKey,
			int64_t encodingJobKey,
			string multicastIP,
			string multicastPort,
			string tvType,
			int64_t tvServiceId,
			int64_t tvFrequency,
			int64_t tvSymbolRate,
			int64_t tvBandwidthInMhz,
			string tvModulation,
			int tvVideoPid,
			int tvAudioItalianPid,
			bool toBeAdded
		);

		pair<string, string> getTVMulticastFromDvblastConfigurationFile(
			int64_t ingestionJobKey,
			int64_t encodingJobKey,
			string tvType,
			int64_t tvServiceId,
			int64_t tvFrequency,
			int64_t tvSymbolRate,
			int64_t tvBandwidthInMhz,
			string tvModulation
		);

};

#endif
