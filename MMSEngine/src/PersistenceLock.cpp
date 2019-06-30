
#include "PersistenceLock.h"

PersistenceLock::PersistenceLock(
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
	MMSEngineDBFacade::LockType lockType, int waitingTimeoutInSecondsIfLocked,
	string owner, string label, shared_ptr<spdlog::logger> logger)
{
	_logger = logger;
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_label = label;
	_lockType = lockType;
	_dataInitialized = false;

	_lockDone = false;
	_mmsEngineDBFacade->setLock(lockType, waitingTimeoutInSecondsIfLocked, owner, label);
	// no exception means lock is done
	_lockDone = true;
}

PersistenceLock::~PersistenceLock()
{
	try
	{
		if (_lockDone)
		{
			if (_dataInitialized)
				_mmsEngineDBFacade->releaseLock(_lockType, _label, _data);
			else
				_mmsEngineDBFacade->releaseLock(_lockType, _label);
		}
		else
		{
			_logger->info(__FILEREF__ + "Destructor PersistenceLock, no releaseLock"
				+ ", _lockType: " + MMSEngineDBFacade::toString(_lockType)
			);
		}
	}
	catch(runtime_error e)
	{
		_logger->error(__FILEREF__ + "releaseLock failed"
			+ ", _lockType: " + MMSEngineDBFacade::toString(_lockType)
			+ ", exception: " + e.what()
		);
	}
	catch(exception e)
	{
		_logger->error(__FILEREF__ + "releaseLock failed"
			+ ", _lockType: " + MMSEngineDBFacade::toString(_lockType)
		);
	}
}

void PersistenceLock::setData(string data)
{
	_data	= data;
	_dataInitialized = true;
}

