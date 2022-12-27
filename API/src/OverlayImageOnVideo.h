
#include "FFMPEGEncoderTask.h"


class OverlayImageOnVideo: public FFMPEGEncoderTask {

	public:
		OverlayImageOnVideo(
			shared_ptr<Encoding> encoding,
			int64_t encodingJobKey,
			Json::Value configuration,
			mutex* encodingCompletedMutex,                                                                        
			map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,                                    
			shared_ptr<spdlog::logger> logger):
		FFMPEGEncoderTask(encoding, encodingJobKey, configuration, encodingCompletedMutex,
			encodingCompletedMap, logger)
		{ };

		void encodeContent(string requestBody);

	private:
};

