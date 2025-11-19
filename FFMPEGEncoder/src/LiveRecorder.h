
#include "FFMPEGEncoderTask.h"

class LiveRecorder : public FFMPEGEncoderTask
{

  public:
	LiveRecorder(
		const shared_ptr<LiveRecording> &liveRecording, const json& configurationRoot,
		mutex *encodingCompletedMutex, map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap, mutex *tvChannelsPortsMutex,
		long *tvChannelPort_CurrentOffset
	);

	void encodeContent(const string_view &requestBody);

  private:
	int _liveRecorderChunksIngestionCheckInSeconds;

	mutex *_tvChannelsPortsMutex;
	long *_tvChannelPort_CurrentOffset;
};
