
#include "MMSEngineDBFacade.h"

class PersistenceLock {

	public:
		PersistenceLock(
			shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
			MMSEngineDBFacade::LockType lockType, int waitingTimeoutInSecondsIfLocked,
			string owner, shared_ptr<spdlog::logger> logger);

		~PersistenceLock();

		void setData(string data);

	private:
		shared_ptr<spdlog::logger>			_logger;
		shared_ptr<MMSEngineDBFacade>		_mmsEngineDBFacade;
		MMSEngineDBFacade::LockType			_lockType;
		string								_data;
		bool								_dataInitialized;
};
