
#include "PersistenceLock.h"

PersistenceLock::PersistenceLock(
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
	MMSEngineDBFacade::LockType lockType, int waitingTimeoutInSecondsIfLocked,
	string owner)
{
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_lockType = lockType;
	_dataInitialized = false;

	_mmsEngineDBFacade->setLock(lockType, waitingTimeoutInSecondsIfLocked, owner);
}

PersistenceLock::~PersistenceLock()
{
	if (_dataInitialized)
		_mmsEngineDBFacade->releaseLock(_lockType, _data);
	else
		_mmsEngineDBFacade->releaseLock(_lockType);
}

void PersistenceLock::setData(string data)
{
	_data	= data;
	_dataInitialized = true;
}

