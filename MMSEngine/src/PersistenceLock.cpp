
#include "PersistenceLock.h"

PersistenceLock::PersistenceLock(
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
	MMSEngineDBFacade::LockType lockType, int waitingTimeoutInSecondsIfLocked,
	string owner, shared_ptr<spdlog::logger> logger)
{
	_logger = logger;
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_lockType = lockType;
	_dataInitialized = false;

	_mmsEngineDBFacade->setLock(lockType, waitingTimeoutInSecondsIfLocked, owner);
}

PersistenceLock::~PersistenceLock()
{
	try
	{
		if (_dataInitialized)
			_mmsEngineDBFacade->releaseLock(_lockType, _data);
		else
			_mmsEngineDBFacade->releaseLock(_lockType);
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

