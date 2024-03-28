
#include "FFMPEGEncoderTask.h"


class GenerateFrames: public FFMPEGEncoderTask {

	public:
		GenerateFrames(
			shared_ptr<Encoding> encoding,
			int64_t ingestionJobKey,
			int64_t encodingJobKey,
			json configurationRoot,
			mutex* encodingCompletedMutex,                                                                        
			map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,                                    
			shared_ptr<spdlog::logger> logger):
		FFMPEGEncoderTask(encoding, ingestionJobKey, encodingJobKey, configurationRoot, encodingCompletedMutex,
			encodingCompletedMap, logger)
		{ };

		void encodeContent(json metadataRoot);

	private:
		int64_t generateFrames_ingestFrame(
			int64_t ingestionJobKey,
			bool externalEncoder,
			string imagesDirectory, string generatedFrameFileName,
			string addContentTitle,
			json userDataRoot,
			string outputFileFormat,
			json ingestedParametersRoot,
			json encodingParametersRoot);
};

