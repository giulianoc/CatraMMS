
#include "FFMPEGEncoderTask.h"

class LiveGrid : public FFMPEGEncoderTask
{

  public:
	LiveGrid(
		const shared_ptr<LiveProxyAndGrid> &liveProxyData, const json &configurationRoot,
		mutex *encodingCompletedMutex, map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap
	);

	void encodeContent(const string_view& requestBody);

  private:
	shared_ptr<LiveProxyAndGrid> _liveProxyData;
};
