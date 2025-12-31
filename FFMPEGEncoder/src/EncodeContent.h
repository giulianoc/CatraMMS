
#include "FFMPEGEncoderTask.h"

class EncodeContent : public FFMPEGEncoderTask
{

  public:
	EncodeContent(
		const std::shared_ptr<Encoding> &encoding, const nlohmann::json &configurationRoot, std::mutex *encodingCompletedMutex,
		std::map<int64_t, std::shared_ptr<EncodingCompleted>> *encodingCompletedMap
	)
		: FFMPEGEncoderTask(encoding, configurationRoot, encodingCompletedMutex, encodingCompletedMap) {};

	void encodeContent(nlohmann::json metadataRoot);
};
