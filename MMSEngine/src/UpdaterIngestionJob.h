
#include "spdlog/spdlog.h"
#include "MMSEngineDBFacade.h"

class UpdaterIngestionJob {

	public:
		UpdaterIngestionJob(
			shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
			shared_ptr<spdlog::logger> logger
			);

		~UpdaterIngestionJob();

		void updateIngestionJob (
			int64_t ingestionJobKey,
			MMSEngineDBFacade::IngestionStatus newIngestionStatus,
			string errorMessage,
			string processorMMS = "noToBeUpdated",
			int maxSecondsToWaitUpdateIngestionJobLock = 500);

	private:
		shared_ptr<MMSEngineDBFacade>		_mmsEngineDBFacade;
		shared_ptr<spdlog::logger>			_logger;
};

