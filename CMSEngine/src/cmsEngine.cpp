
#include <thread>

#include "catralibraries/Scheduler2.h"

#include "CMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "CMSEngineDBFacade.h"


int main (int iArgc, char *pArgv [])
{

    auto logger = spdlog::stdout_logger_mt("encodingEngine");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    size_t dbPoolSize = 5;
    string dbServer ("tcp://127.0.0.1:3306");
    string dbUsername("root");
    string dbPassword("root");
    string dbName("CatraCMS");
    shared_ptr<CMSEngineDBFacade>       cmsEngineDBFacade = make_shared<CMSEngineDBFacade>(
            dbPoolSize, dbServer, dbUsername, dbPassword, dbName);

    shared_ptr<Customers>       customers = make_shared<Customers>(cmsEngineDBFacade);
    
    shared_ptr<MultiEventsSet>          multiEventsSet = make_shared<MultiEventsSet>();
    multiEventsSet->addDestination(CMSENGINEPROCESSORNAME);

    CMSEngineProcessor      cmsEngineProcessor(logger, cmsEngineDBFacade, customers);
    
    unsigned long           ulThreadSleepInMilliSecs = 100;
    Scheduler2              scheduler(ulThreadSleepInMilliSecs);


    thread cmsEngineProcessorThread (cmsEngineProcessor, multiEventsSet);

    thread schedulerThread (ref(scheduler));

    unsigned long           checkIngestionTimesPeriodInMilliSecs = 5 * 1000;
    shared_ptr<CheckIngestionTimes>     checkIngestionTimes =
            make_shared<CheckIngestionTimes>(checkIngestionTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkIngestionTimes->start();
    scheduler.activeTimes(checkIngestionTimes);

    cmsEngineProcessorThread.join();
    schedulerThread.join();

    
    return 0;
}
