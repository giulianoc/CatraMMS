
#include "MMSEngineDBFacade.h"

class PersistenceLock {

	public:
		PersistenceLock(
			MMSEngineDBFacade* mmsEngineDBFacade,
			MMSEngineDBFacade::LockType lockType, int waitingTimeoutInSecondsIfLocked,
			std::string owner, std::string label, int milliSecondsToSleepWaitingLock,
			std::shared_ptr<spdlog::logger> logger);

		~PersistenceLock();

		void setData(std::string data);

	private:
		std::shared_ptr<spdlog::logger>			_logger;
		MMSEngineDBFacade*					_mmsEngineDBFacade;
		MMSEngineDBFacade::LockType			_lockType;
		std::string								_data;
		std::string								_label;
		bool								_dataInitialized;
		bool								_lockDone;
};
