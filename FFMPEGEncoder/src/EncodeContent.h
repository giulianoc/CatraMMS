
#include "FFMPEGEncoderTask.h"

class EncodeContent : public FFMPEGEncoderTask
{

  public:
	EncodeContent(
		const shared_ptr<Encoding> &encoding, const json &configurationRoot, mutex *encodingCompletedMutex,
		map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap
	)
		: FFMPEGEncoderTask(encoding, configurationRoot, encodingCompletedMutex, encodingCompletedMap) {};

	void encodeContent(json metadataRoot);
};
