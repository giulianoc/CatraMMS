
#include "FFMPEGEncoderTask.h"

class SlideShow : public FFMPEGEncoderTask
{

  public:
	SlideShow(
		shared_ptr<Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json configurationRoot, mutex *encodingCompletedMutex,
		map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap
	)
		: FFMPEGEncoderTask(encoding, ingestionJobKey, encodingJobKey, configurationRoot, encodingCompletedMutex, encodingCompletedMap) {};

	void encodeContent(json metadataRoot);

  private:
};
