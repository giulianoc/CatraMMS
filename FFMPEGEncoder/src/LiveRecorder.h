
#include "FFMPEGEncoderTask.h"

class LiveRecorder : public FFMPEGEncoderTask
{

  public:
	LiveRecorder(
		shared_ptr<LiveRecording> liveRecording, int64_t ingestionJobKey, int64_t encodingJobKey, json configurationRoot,
		mutex *encodingCompletedMutex, map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap, mutex *tvChannelsPortsMutex,
		long *tvChannelPort_CurrentOffset
	);
	~LiveRecorder();

	void encodeContent(const string_view &requestBody);

  private:
	shared_ptr<LiveRecording> _liveRecording;
	int _liveRecorderChunksIngestionCheckInSeconds;

	mutex *_tvChannelsPortsMutex;
	long *_tvChannelPort_CurrentOffset;
};
