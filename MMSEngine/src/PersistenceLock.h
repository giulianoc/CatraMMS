
#include "MMSEngineDBFacade.h"

class PersistenceLock {

	public:
		PersistenceLock(
			shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
			MMSEngineDBFacade::LockType lockType, int waitingTimeoutInSecondsIfLocked,
			string owner, string label, shared_ptr<spdlog::logger> logger);

		~PersistenceLock();

		void setData(string data);

	private:
		shared_ptr<spdlog::logger>			_logger;
		shared_ptr<MMSEngineDBFacade>		_mmsEngineDBFacade;
		MMSEngineDBFacade::LockType			_lockType;
		string								_data;
		string								_label;
		bool								_dataInitialized;
		bool								_lockDone;
};
