
#include "spdlog/spdlog.h"
#include "MMSEngineDBFacade.h"

class UpdaterEncoderJob {

	public:
		UpdaterEncoderJob(
			shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
			shared_ptr<spdlog::logger> logger
			);

		~UpdaterEncoderJob();

		int updateEncodingJob (
			int64_t encodingJobKey,
			MMSEngineDBFacade::EncodingError encodingError,
			int64_t mediaItemKey,
			int64_t encodedPhysicalPathKey,
			int64_t ingestionJobKey,
			string processorMMS,
			int maxSecondsToWaitUpdateEncodingJobLock);

	private:
		shared_ptr<MMSEngineDBFacade>		_mmsEngineDBFacade;
		shared_ptr<spdlog::logger>			_logger;
};

