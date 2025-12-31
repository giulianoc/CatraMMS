
#include "FFMPEGEncoderTask.h"

class LiveGrid : public FFMPEGEncoderTask
{

  public:
	LiveGrid(
		const std::shared_ptr<LiveProxyAndGrid> &liveProxyData, const nlohmann::json &configurationRoot,
		std::mutex *encodingCompletedMutex, std::map<int64_t, std::shared_ptr<EncodingCompleted>> *encodingCompletedMap
	);

	void encodeContent(const std::string_view& requestBody);

  private:
	std::shared_ptr<LiveProxyAndGrid> _liveProxyData;
};
