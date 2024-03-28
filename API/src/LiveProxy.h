
#include "FFMPEGEncoderTask.h"


class LiveProxy: public FFMPEGEncoderTask {

	public:
		LiveProxy(
			shared_ptr<LiveProxyAndGrid> liveProxyData,
			int64_t ingestionJobKey,
			int64_t encodingJobKey,
			json configurationRoot,
			mutex* encodingCompletedMutex,
			map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,                                    
			shared_ptr<spdlog::logger> logger,
			mutex* tvChannelsPortsMutex,                                                                              
			long* tvChannelPort_CurrentOffset);
		~LiveProxy();

		void encodeContent(string requestBody);

	private:
		shared_ptr<LiveProxyAndGrid>	_liveProxyData;

		mutex*						_tvChannelsPortsMutex;
		long*						_tvChannelPort_CurrentOffset;
};

