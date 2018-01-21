
#include <thread>

#include "catralibraries/Scheduler2.h"

#include "CMSEngineProcessor.h"
#include "CheckIngestionTimes.h"


int main (int iArgc, char *pArgv [])
{

    auto logger = spdlog::stdout_logger_mt("encodingEngine");
    spdlog::set_level(spdlog::level::trace);
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);


    shared_ptr<MultiEventsSet>          multiEventsSet = make_shared<MultiEventsSet>();
    multiEventsSet->addDestination(CMSENGINEPROCESSORNAME);

    CMSEngineProcessor      cmsEngineProcessor(logger);
    
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
