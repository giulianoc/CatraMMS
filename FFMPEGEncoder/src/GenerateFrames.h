
#include "FFMPEGEncoderTask.h"

class GenerateFrames : public FFMPEGEncoderTask
{

  public:
	GenerateFrames(
		const shared_ptr<Encoding> &encoding, const json &configurationRoot, mutex *encodingCompletedMutex,
		map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap
	)
		: FFMPEGEncoderTask(encoding, configurationRoot, encodingCompletedMutex, encodingCompletedMap) {};

	void encodeContent(json metadataRoot);

  private:
	int64_t generateFrames_ingestFrame(
		int64_t ingestionJobKey, bool externalEncoder, const string &imagesDirectory, const string &generatedFrameFileName,
		const string &addContentTitle, json userDataRoot, string outputFileFormat, json ingestedParametersRoot, const json &encodingParametersRoot
	);
};
