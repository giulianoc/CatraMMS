
#include <thread>

#include "catralibraries/Scheduler2.h"

#include "CMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "CMSEngineDBFacade.h"
#include "CMSStorage.h"
#include "CMSEngine.h"


int main (int iArgc, char *pArgv [])
{

    auto logger = spdlog::stdout_logger_mt("encodingEngine");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    size_t dbPoolSize = 5;
    string dbServer ("tcp://127.0.0.1:3306");
    string dbUsername("root");
    string dbPassword("giuliano");
    string dbName("workKing");
    logger->info(string("Creating CMSEngineDBFacade")
        + ", dbPoolSize: " + to_string(dbPoolSize)
        + ", dbServer: " + dbServer
        + ", dbUsername: " + dbUsername
        + ", dbPassword: " + dbPassword
        + ", dbName: " + dbName
            );
    shared_ptr<CMSEngineDBFacade>       cmsEngineDBFacade = make_shared<CMSEngineDBFacade>(
            dbPoolSize, dbServer, dbUsername, dbPassword, dbName, logger);
    
    string storage ("/Users/multi/GestioneProgetti/Development/catrasoftware/storage/");
    unsigned long freeSpaceToLeaveInEachPartitionInMB = 5;
    logger->info(string("Creating CMSStorage")
        + ", storage: " + storage
        + ", freeSpaceToLeaveInEachPartitionInMB: " + to_string(freeSpaceToLeaveInEachPartitionInMB)
            );
    shared_ptr<CMSStorage>       cmsStorage = make_shared<CMSStorage>(
            storage, 
            freeSpaceToLeaveInEachPartitionInMB,
            logger);

    logger->info(string("Creating CMSEngine")
            );
    shared_ptr<CMSEngine>       cmsEngine = make_shared<CMSEngine>(cmsEngineDBFacade, logger);
    
    logger->info(string("Creating MultiEventsSet")
        + ", addDestination: " + CMSENGINEPROCESSORNAME
            );
    shared_ptr<MultiEventsSet>          multiEventsSet = make_shared<MultiEventsSet>();
    multiEventsSet->addDestination(CMSENGINEPROCESSORNAME);

    logger->info(string("Creating CMSEngineProcessor")
            );
    CMSEngineProcessor      cmsEngineProcessor(logger, multiEventsSet, cmsEngineDBFacade, cmsStorage);
    
    unsigned long           ulThreadSleepInMilliSecs = 100;
    logger->info(string("Creating Scheduler2")
        + ", ulThreadSleepInMilliSecs: " + to_string(ulThreadSleepInMilliSecs)
            );
    Scheduler2              scheduler(ulThreadSleepInMilliSecs);


    logger->info(string("Starting CMSEngineProcessor")
            );
    thread cmsEngineProcessorThread (cmsEngineProcessor);

    logger->info(string("Starting Scheduler2")
            );
    thread schedulerThread (ref(scheduler));

    unsigned long           checkIngestionTimesPeriodInMilliSecs = 5 * 1000;
    logger->info(string("Creating and Starting CheckIngestionTimes")
        + ", checkIngestionTimesPeriodInMilliSecs: " + to_string(checkIngestionTimesPeriodInMilliSecs)
            );
    shared_ptr<CheckIngestionTimes>     checkIngestionTimes =
            make_shared<CheckIngestionTimes>(checkIngestionTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkIngestionTimes->start();
    scheduler.activeTimes(checkIngestionTimes);

    /*
    cmsEngine->addCustomer(
	"Warner",                       // string customerName,
        "Warner",                       // string password,
	"",                             // string street,
        "",                             // string city,
        "",                             // string state,
	"",                             // string zip,
        "",                             // string phone,
        "",                             // string countryCode,
        CMSEngineDBFacade::CustomerType::EncodingOnly,  // CMSEngineDBFacade::CustomerType customerType
	"",                             // string deliveryURL,
        true,                           // bool enabled,
        CMSEngineDBFacade::EncodingPriority::Default,   //  CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
        CMSEngineDBFacade::EncodingPeriod::Daily,       //  CMSEngineDBFacade::EncodingPeriod encodingPeriod,
	10,                             // long maxIngestionsNumber,
        10,                             // long maxStorageInGB,
	"",                             // string languageCode,
        "giuliano",                     // string userName,
        "giuliano",                     // string userPassword,
        "giulianoc@catrasoftware.it",   // string userEmailAddress,
        chrono::system_clock::now()     // chrono::system_clock::time_point userExpirationDate
    );
    cmsEngine->addCustomer(
	"Universal",                       // string customerName,
        "Universal",                       // string password,
	"",                             // string street,
        "",                             // string city,
        "",                             // string state,
	"",                             // string zip,
        "",                             // string phone,
        "",                             // string countryCode,
        CMSEngineDBFacade::CustomerType::EncodingOnly,  // CMSEngineDBFacade::CustomerType customerType
	"",                             // string deliveryURL,
        true,                           // bool enabled,
        CMSEngineDBFacade::EncodingPriority::Default,   //  CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
        CMSEngineDBFacade::EncodingPeriod::Daily,       //  CMSEngineDBFacade::EncodingPeriod encodingPeriod,
	10,                             // long maxIngestionsNumber,
        10,                             // long maxStorageInGB,
	"",                             // string languageCode,
        "giuliano",                     // string userName,
        "giuliano",                     // string userPassword,
        "giulianoc@catrasoftware.it",   // string userEmailAddress,
        chrono::system_clock::now()     // chrono::system_clock::time_point userExpirationDate
    );
    cmsEngine->addCustomer(
	"Sony",                       // string customerName,
        "Sony",                       // string password,
	"",                             // string street,
        "",                             // string city,
        "",                             // string state,
	"",                             // string zip,
        "",                             // string phone,
        "",                             // string countryCode,
        CMSEngineDBFacade::CustomerType::EncodingOnly,  // CMSEngineDBFacade::CustomerType customerType
	"",                             // string deliveryURL,
        true,                           // bool enabled,
        CMSEngineDBFacade::EncodingPriority::Default,   //  CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
        CMSEngineDBFacade::EncodingPeriod::Daily,       //  CMSEngineDBFacade::EncodingPeriod encodingPeriod,
	10,                             // long maxIngestionsNumber,
        10,                             // long maxStorageInGB,
	"",                             // string languageCode,
        "giuliano",                     // string userName,
        "giuliano",                     // string userPassword,
        "giulianoc@catrasoftware.it",   // string userEmailAddress,
        chrono::system_clock::now()     // chrono::system_clock::time_point userExpirationDate
    );
     */
    
    logger->info(string("Waiting CMSEngineProcessor")
            );
    cmsEngineProcessorThread.join();
    logger->info(string("Waiting Scheduler2")
            );
    schedulerThread.join();

    logger->info(string("Shutdown done")
            );
    
    return 0;
}
