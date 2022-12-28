
#include "FFMPEGEncoderTask.h"


class EncodeContent: public FFMPEGEncoderTask {

	public:
		EncodeContent(
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
