
#include "FFMPEGEncoderTask.h"


class SlideShow: public FFMPEGEncoderTask {

	public:
		SlideShow(
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

