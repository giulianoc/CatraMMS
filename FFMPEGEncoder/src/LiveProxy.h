
#include "FFMPEGEncoderTask.h"

class LiveProxy : public FFMPEGEncoderTask
{

  public:
	LiveProxy(
		const shared_ptr<LiveProxyAndGrid> &liveProxyData, const json &configurationRoot,
		mutex *encodingCompletedMutex, map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap, mutex *tvChannelsPortsMutex,
		long *tvChannelPort_CurrentOffset
	);

	void encodeContent(const string_view &requestBody);

  private:
	mutex *_tvChannelsPortsMutex;
	long *_tvChannelPort_CurrentOffset;
};
