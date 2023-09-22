
#include "PersistenceLock.h"

PersistenceLock::PersistenceLock(
	MMSEngineDBFacade* mmsEngineDBFacade,
	MMSEngineDBFacade::LockType lockType, int waitingTimeoutInSecondsIfLocked,
	string owner, string label, int milliSecondsToSleepWaitingLock,
	shared_ptr<spdlog::logger> logger)
{
	_logger = logger;
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_label = label;
	_lockType = lockType;
	_dataInitialized = false;

	_lockDone = false;
	_mmsEngineDBFacade->setLock(lockType, waitingTimeoutInSecondsIfLocked, owner, label,
			milliSecondsToSleepWaitingLock);
	// no exception means lock is done
	_lockDone = true;
}

PersistenceLock::~PersistenceLock()
{
	try
	{
		// 2019-07-01: check on _lockDone is useless because when the PersistenceLock constructor
		//	raise an exception, the PersistenceLock destructor is not called
		// if (_lockDone)
		{
			if (_dataInitialized)
				_mmsEngineDBFacade->releaseLock(_lockType, _label, _data);
			else
				_mmsEngineDBFacade->releaseLock(_lockType, _label);
		}
		/*
		else
		{
			_logger->info(__FILEREF__ + "Destructor PersistenceLock, no releaseLock"
				+ ", _lockType: " + MMSEngineDBFacade::toString(_lockType)
			);
		}
		*/
	}
    catch(sql::SQLException& se)
    {
		_logger->error(__FILEREF__ + "releaseLock failed"
			+ ", _lockType: " + MMSEngineDBFacade::toString(_lockType)
			+ ", exception: " + se.what()
		);
	}
	catch(runtime_error& e)
	{
		_logger->error(__FILEREF__ + "releaseLock failed"
			+ ", _lockType: " + MMSEngineDBFacade::toString(_lockType)
			+ ", exception: " + e.what()
		);
	}
	catch(exception& e)
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

