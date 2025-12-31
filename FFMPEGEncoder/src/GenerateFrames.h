
#include "FFMPEGEncoderTask.h"

class GenerateFrames : public FFMPEGEncoderTask
{

  public:
	GenerateFrames(
		const std::shared_ptr<Encoding> &encoding, const nlohmann::json &configurationRoot, std::mutex *encodingCompletedMutex,
		std::map<int64_t, std::shared_ptr<EncodingCompleted>> *encodingCompletedMap
	)
		: FFMPEGEncoderTask(encoding, configurationRoot, encodingCompletedMutex, encodingCompletedMap) {};

	void encodeContent(nlohmann::json metadataRoot);

  private:
	int64_t generateFrames_ingestFrame(
		int64_t ingestionJobKey, bool externalEncoder, const std::string &imagesDirectory, const std::string &generatedFrameFileName,
		const std::string &addContentTitle, nlohmann::json userDataRoot, std::string outputFileFormat, nlohmann::json ingestedParametersRoot, const nlohmann::json &encodingParametersRoot
	);
};
