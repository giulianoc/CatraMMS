
#include <thread>

#include "catralibraries/Scheduler2.h"

#include "MMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "CheckEncodingTimes.h"
#include "MMSEngineDBFacade.h"
#include "ActiveEncodingsManager.h"
#include "MMSStorage.h"
#include "MMSEngine.h"


int main (int iArgc, char *pArgv [])
{

    string logPathName ("/tmp/mmsEngineService.log");
    // auto logger = spdlog::stdout_logger_mt("mmsEngineService");
    auto logger = spdlog::daily_logger_mt("mmsEngineService", logPathName.c_str(), 11, 20);
    
    // trigger flush if the log severity is error or higher
    logger->flush_on(spdlog::level::trace);
    
    spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [tid %t] %v");

    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    size_t dbPoolSize = 5;
    string dbServer ("tcp://127.0.0.1:3306");
    #ifdef __APPLE__
        string dbUsername("root"); string dbPassword("giuliano"); string dbName("workKing");
    #else
        string dbUsername("root"); string dbPassword("root"); string dbName("catracms");
    #endif
    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
        + ", dbPoolSize: " + to_string(dbPoolSize)
        + ", dbServer: " + dbServer
        + ", dbUsername: " + dbUsername
        + ", dbPassword: " + dbPassword
        + ", dbName: " + dbName
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            dbPoolSize, dbServer, dbUsername, dbPassword, dbName, logger);
    
    #ifdef __APPLE__
        string storageRootPath ("/Users/multi/GestioneProgetti/Development/catrasoftware/storage/");
    #else
        string storageRootPath ("/home/giuliano/storage/");
    #endif
    unsigned long freeSpaceToLeaveInEachPartitionInMB = 5;
    logger->info(__FILEREF__ + "Creating MMSStorage"
        + ", storageRootPath: " + storageRootPath
        + ", freeSpaceToLeaveInEachPartitionInMB: " + to_string(freeSpaceToLeaveInEachPartitionInMB)
            );
    shared_ptr<MMSStorage>       mmsStorage = make_shared<MMSStorage>(
            storageRootPath, 
            freeSpaceToLeaveInEachPartitionInMB,
            logger);

    logger->info(__FILEREF__ + "Creating MMSEngine"
            );
    shared_ptr<MMSEngine>       mmsEngine = make_shared<MMSEngine>(mmsEngineDBFacade, logger);
        
    logger->info(__FILEREF__ + "Creating MultiEventsSet"
        + ", addDestination: " + MMSENGINEPROCESSORNAME
            );
    shared_ptr<MultiEventsSet>          multiEventsSet = make_shared<MultiEventsSet>();
    multiEventsSet->addDestination(MMSENGINEPROCESSORNAME);

    logger->info(__FILEREF__ + "Creating ActiveEncodingsManager"
            );
    ActiveEncodingsManager      activeEncodingsManager(mmsEngineDBFacade, mmsStorage, logger);

    logger->info(__FILEREF__ + "Creating MMSEngineProcessor"
            );
    MMSEngineProcessor      mmsEngineProcessor(logger, multiEventsSet, mmsEngineDBFacade, mmsStorage, &activeEncodingsManager);
    
    unsigned long           ulThreadSleepInMilliSecs = 100;
    logger->info(__FILEREF__ + "Creating Scheduler2"
        + ", ulThreadSleepInMilliSecs: " + to_string(ulThreadSleepInMilliSecs)
            );
    Scheduler2              scheduler(ulThreadSleepInMilliSecs);


    logger->info(__FILEREF__ + "Starting ActiveEncodingsManager"
            );
    thread activeEncodingsManagerThread (ref(activeEncodingsManager));

    logger->info(__FILEREF__ + "Starting MMSEngineProcessor"
            );
    thread mmsEngineProcessorThread (mmsEngineProcessor);

    logger->info(__FILEREF__ + "Starting Scheduler2"
            );
    thread schedulerThread (ref(scheduler));

    unsigned long           checkIngestionTimesPeriodInMilliSecs = 2 * 1000;
    logger->info(__FILEREF__ + "Creating and Starting CheckIngestionTimes"
        + ", checkIngestionTimesPeriodInMilliSecs: " + to_string(checkIngestionTimesPeriodInMilliSecs)
            );
    shared_ptr<CheckIngestionTimes>     checkIngestionTimes =
            make_shared<CheckIngestionTimes>(checkIngestionTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkIngestionTimes->start();
    scheduler.activeTimes(checkIngestionTimes);

    unsigned long           checkEncodingTimesPeriodInMilliSecs = 10 * 1000;
    logger->info(__FILEREF__ + "Creating and Starting CheckEncodingTimes"
        + ", checkEncodingTimesPeriodInMilliSecs: " + to_string(checkEncodingTimesPeriodInMilliSecs)
            );
    shared_ptr<CheckEncodingTimes>     checkEncodingTimes =
            make_shared<CheckEncodingTimes>(checkEncodingTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkEncodingTimes->start();
    scheduler.activeTimes(checkEncodingTimes);

    
    logger->info(__FILEREF__ + "Waiting ActiveEncodingsManager"
            );
    activeEncodingsManagerThread.join();
    
    logger->info(__FILEREF__ + "Waiting MMSEngineProcessor"
            );
    mmsEngineProcessorThread.join();
    
    logger->info(__FILEREF__ + "Waiting Scheduler2"
            );
    schedulerThread.join();

    logger->info(__FILEREF__ + "Shutdown done"
            );
    
    return 0;
}
