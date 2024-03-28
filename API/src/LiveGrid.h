
#include "FFMPEGEncoderTask.h"


class LiveGrid: public FFMPEGEncoderTask {

	public:
		LiveGrid(
			shared_ptr<LiveProxyAndGrid> liveProxyData,
			int64_t ingestionJobKey,
			int64_t encodingJobKey,
			json configurationRoot,
			mutex* encodingCompletedMutex,                                                                        
			map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,                                    
			shared_ptr<spdlog::logger> logger);
		~LiveGrid();

		void encodeContent(string requestBody);

	private:
		shared_ptr<LiveProxyAndGrid>	_liveProxyData;
};

