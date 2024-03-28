
#include "FFMPEGEncoderTask.h"


class AddSilentAudio: public FFMPEGEncoderTask {

	public:
		AddSilentAudio(
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
};

