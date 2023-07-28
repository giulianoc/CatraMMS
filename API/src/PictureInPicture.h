
#include "FFMPEGEncoderTask.h"


class PictureInPicture: public FFMPEGEncoderTask {

	public:
		PictureInPicture(
			shared_ptr<Encoding> encoding,
			int64_t ingestionJobKey,
			int64_t encodingJobKey,
			Json::Value configuration,
			mutex* encodingCompletedMutex,                                                                        
			map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,                                    
			shared_ptr<spdlog::logger> logger):
		FFMPEGEncoderTask(encoding, ingestionJobKey, encodingJobKey, configuration, encodingCompletedMutex,
			encodingCompletedMap, logger)
		{ };

		void encodeContent(Json::Value metadataRoot);

	private:
};

