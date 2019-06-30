
#include <fstream>
#include "PersistenceLock.h"

Json::Value loadConfigurationFile(const char* configurationPathName);

int main (int iArgc, char *pArgv [])
{

    if (iArgc != 3)
    {
        cerr << "Usage: " << pArgv[0] << " seconds-to-wait-lock config-path-name" << endl;
        
        return 1;
    }

	int secondsToWaitLock = atol(pArgv[1]);
	string configFileName = pArgv[2];
    
    Json::Value configuration = loadConfigurationFile(configFileName.c_str());

    auto logger = spdlog::stdout_logger_mt("encodingEngine");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, 3, logger);

	// this_thread::sleep_for(chrono::seconds(secondsToWait));

	int waitingTimeoutInSecondsIfLocked = secondsToWaitLock;

	try
	{
		int milliSecondsToSleepWaitingLock = 500;

		cout << endl << endl << "First PersistenceLock" << endl << endl ;
		PersistenceLock persistenceLock(mmsEngineDBFacade,
			MMSEngineDBFacade::LockType::Ingestion,
			waitingTimeoutInSecondsIfLocked,
			"test", "Test", milliSecondsToSleepWaitingLock, logger);

		this_thread::sleep_for(chrono::seconds(30));
	}
	catch (exception e)
	{
		cout << endl << endl << "First. Exception: " << e.what() << endl << endl ;
	}
  

	try
	{
		int milliSecondsToSleepWaitingLock = 500;

		cout << endl << endl << "Second PersistenceLock" << endl << endl ;
		PersistenceLock persistenceLock(mmsEngineDBFacade,
			MMSEngineDBFacade::LockType::Ingestion,
			waitingTimeoutInSecondsIfLocked,
			"test", "Test", milliSecondsToSleepWaitingLock, logger);
	}
	catch (exception e)
	{
		cout << endl << endl << "Second. Exception: " << e.what() << endl << endl ;
	}

    logger->info(__FILEREF__ + "Shutdown done"
            );

    return 0;
}

Json::Value loadConfigurationFile(const char* configurationPathName)
{
    Json::Value configurationJson;
    
    try
    {
        ifstream configurationFile(configurationPathName, std::ifstream::binary);
        configurationFile >> configurationJson;
    }
    catch(...)
    {
        cerr << string("wrong json configuration format")
                + ", configurationPathName: " + configurationPathName
            << endl;
    }
    
    return configurationJson;
}
