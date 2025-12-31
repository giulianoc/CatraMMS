
#include "FFMPEGEncoderTask.h"

class LiveRecorder : public FFMPEGEncoderTask
{

  public:
	LiveRecorder(
		const std::shared_ptr<LiveRecording> &liveRecording, const nlohmann::json& configurationRoot,
		std::mutex *encodingCompletedMutex, std::map<int64_t, std::shared_ptr<EncodingCompleted>> *encodingCompletedMap, std::mutex *tvChannelsPortsMutex,
		long *tvChannelPort_CurrentOffset
	);

	void encodeContent(const std::string_view &requestBody);

  private:
	int _liveRecorderChunksIngestionCheckInSeconds;

	std::mutex *_tvChannelsPortsMutex;
	long *_tvChannelPort_CurrentOffset;
};
