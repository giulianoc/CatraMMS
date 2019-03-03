
#include "MMSEngineDBFacade.h"

class PersistenceLock {

	public:
		PersistenceLock(
			shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
			MMSEngineDBFacade::LockType lockType, int waitingTimeoutInSecondsIfLocked,
			string owner);

		~PersistenceLock();

		void setData(string data);

	private:
		shared_ptr<MMSEngineDBFacade>		_mmsEngineDBFacade;
		MMSEngineDBFacade::LockType			_lockType;
		string								_data;
		bool								_dataInitialized;
};
