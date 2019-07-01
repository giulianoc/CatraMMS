
#include "PersistenceLock.h"
#include "UpdaterIngestionJob.h"

UpdaterIngestionJob::UpdaterIngestionJob(
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
	shared_ptr<spdlog::logger> logger)
{
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_logger = logger;
}

UpdaterIngestionJob::~UpdaterIngestionJob()
{
}

void UpdaterIngestionJob::updateIngestionJob (
	int64_t ingestionJobKey,
	MMSEngineDBFacade::IngestionStatus newIngestionStatus,
	string errorMessage,
	string processorMMS,
	int maxSecondsToWaitUpdateIngestionJobLock)
{

	try
	{
		int milliSecondsToSleepWaitingLock = 500;

		PersistenceLock persistenceLock(_mmsEngineDBFacade,
			MMSEngineDBFacade::LockType::Ingestion,
			maxSecondsToWaitUpdateIngestionJobLock,
			processorMMS, "UpdateIngestionJob",
			milliSecondsToSleepWaitingLock, _logger);

		_mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, newIngestionStatus,                                                                   
				errorMessage, processorMMS);

		_logger->info(__FILEREF__ + "updateIngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
		);
	}
	catch(AlreadyLocked e)
	{
		_logger->error(__FILEREF__ + "updateIngestionJob failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
			+ ", errorMessage: " + errorMessage
			+ ", processorMMS: " + processorMMS
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch(runtime_error e)
	{
		_logger->error(__FILEREF__ + "updateIngestionJob failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
			+ ", errorMessage: " + errorMessage
			+ ", processorMMS: " + processorMMS
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch(exception e)
	{
		_logger->error(__FILEREF__ + "updateIngestionJob failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
			+ ", errorMessage: " + errorMessage
			+ ", processorMMS: " + processorMMS
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

