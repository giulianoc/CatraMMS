
#include "FFMPEGEncoderTask.h"

class LiveProxy : public FFMPEGEncoderTask
{

  public:
	LiveProxy(
		const std::shared_ptr<LiveProxyAndGrid> &liveProxyData, const nlohmann::json &configurationRoot,
		std::mutex *encodingCompletedMutex, std::map<int64_t, std::shared_ptr<EncodingCompleted>> *encodingCompletedMap, std::mutex *tvChannelsPortsMutex,
		long *tvChannelPort_CurrentOffset
	);

	void encodeContent(const std::string_view &requestBody);

  private:
	std::mutex *_tvChannelsPortsMutex;
	long *_tvChannelPort_CurrentOffset;
};
