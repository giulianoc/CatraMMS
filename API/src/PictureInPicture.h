
#include "FFMPEGEncoderTask.h"

class PictureInPicture : public FFMPEGEncoderTask
{

  public:
	PictureInPicture(
		shared_ptr<Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json configurationRoot, mutex *encodingCompletedMutex,
		map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap
	)
		: FFMPEGEncoderTask(encoding, ingestionJobKey, encodingJobKey, configurationRoot, encodingCompletedMutex, encodingCompletedMap) {};

	void encodeContent(json metadataRoot);

  private:
};
