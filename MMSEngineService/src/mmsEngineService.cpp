
#include <thread>
#include <fstream>
#include <csignal>

#include "catralibraries/Service.h"
#include "catralibraries/Scheduler2.h"

#include "MMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "CheckEncodingTimes.h"
#include "RetentionTimes.h"
#include "MMSEngineDBFacade.h"
#include "ActiveEncodingsManager.h"
#include "MMSStorage.h"

Json::Value loadConfigurationFile(string configurationPathName);

void signalHandler(int signal)
{
    auto logger = spdlog::get("mmsEngineService");
    
    logger->error(__FILEREF__ + "Received a signal"
        + ", signal: " + to_string(signal)
    );
}

int main (int iArgc, char *pArgv [])
{
    
    if (iArgc != 2 && iArgc != 3 && iArgc != 4)
    {
        cerr << "Usage: " 
                << pArgv[0] 
                << " --nodaemon | --pidfile <pid path file name>" 
                << "config-path-name" 
                << endl;
        
        return 1;
    }
    
    bool noDaemon = false;
    string pidFilePathName;
    string configPathName;
    
    if (iArgc == 2)
    {
        pidFilePathName = "/tmp/cmsEngine.pid";
    
        configPathName = pArgv[1];
    }
    else if (iArgc == 3)
    {
        pidFilePathName = "/tmp/cmsEngine.pid";

        if (!strcmp (pArgv[1], "--nodaemon"))
            noDaemon = true;

        configPathName = pArgv[2];
    }
    else if (iArgc == 4)
    {
        if (!strcmp (pArgv[1], "--pidfile"))
            pidFilePathName = pArgv[2];
        else
            pidFilePathName = "/tmp/cmsEngine.pid";

        configPathName = pArgv[3];
    }
    
    if (!noDaemon)
        Service::launchUnixDaemon(pidFilePathName);

    Json::Value configuration = loadConfigurationFile(configPathName);

    string logPathName =  configuration["log"]["mms"].get("pathName", "XXX").asString();
    bool stdout =  configuration["log"]["mms"].get("stdout", "XXX").asBool();
    
    std::vector<spdlog::sink_ptr> sinks;
    auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logPathName.c_str(), 11, 20);
    sinks.push_back(dailySink);
    if (stdout)
    {
        auto stdoutSink = spdlog::sinks::stdout_sink_mt::instance();
        sinks.push_back(stdoutSink);
    }
    auto logger = std::make_shared<spdlog::logger>("mmsEngineService", begin(sinks), end(sinks));
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    spdlog::register_logger(logger);

    // auto logger = spdlog::stdout_logger_mt("mmsEngineService");
    // auto logger = spdlog::daily_logger_mt("mmsEngineService", logPathName.c_str(), 11, 20);
    
    // trigger flush if the log severity is error or higher
    logger->flush_on(spdlog::level::trace);
    
    string logLevel =  configuration["log"]["mms"].get("level", "XXX").asString();
    if (logLevel == "debug")
        spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "info")
        spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "err")
        spdlog::set_level(spdlog::level::err); // trace, debug, info, warn, err, critical, off

    string pattern =  configuration["log"]["mms"].get("pattern", "XXX").asString();
    spdlog::set_pattern(pattern);

    // install a signal handler
    signal(SIGSEGV, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGABRT, signalHandler);
    // signal(SIGBUS, signalHandler);

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade>       mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, logger);
    
    logger->info(__FILEREF__ + "Creating MMSStorage"
            );
    shared_ptr<MMSStorage>       mmsStorage = make_shared<MMSStorage>(
            configuration, mmsEngineDBFacade, logger);
    
    logger->info(__FILEREF__ + "Creating MultiEventsSet"
        + ", addDestination: " + MMSENGINEPROCESSORNAME
            );
    shared_ptr<MultiEventsSet>          multiEventsSet = make_shared<MultiEventsSet>();
    multiEventsSet->addDestination(MMSENGINEPROCESSORNAME);

    logger->info(__FILEREF__ + "Creating ActiveEncodingsManager"
            );
    ActiveEncodingsManager      activeEncodingsManager(configuration, mmsEngineDBFacade, mmsStorage, logger);

    logger->info(__FILEREF__ + "Creating MMSEngineProcessor"
            );
    MMSEngineProcessor      mmsEngineProcessor(logger, multiEventsSet, 
            mmsEngineDBFacade, mmsStorage, &activeEncodingsManager, configuration);
    
    unsigned long           ulThreadSleepInMilliSecs = configuration["scheduler"].get("threadSleepInMilliSecs", 5).asInt();
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

    unsigned long           checkIngestionTimesPeriodInMilliSecs = configuration["scheduler"].get("checkIngestionTimesPeriodInMilliSecs", 2000).asInt();
    logger->info(__FILEREF__ + "Creating and Starting CheckIngestionTimes"
        + ", checkIngestionTimesPeriodInMilliSecs: " + to_string(checkIngestionTimesPeriodInMilliSecs)
            );
    shared_ptr<CheckIngestionTimes>     checkIngestionTimes =
            make_shared<CheckIngestionTimes>(checkIngestionTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkIngestionTimes->start();
    scheduler.activeTimes(checkIngestionTimes);

    unsigned long           checkEncodingTimesPeriodInMilliSecs = configuration["scheduler"].get("checkEncodingTimesPeriodInMilliSecs", 10000).asInt();
    logger->info(__FILEREF__ + "Creating and Starting CheckEncodingTimes"
        + ", checkEncodingTimesPeriodInMilliSecs: " + to_string(checkEncodingTimesPeriodInMilliSecs)
            );
    shared_ptr<CheckEncodingTimes>     checkEncodingTimes =
            make_shared<CheckEncodingTimes>(checkEncodingTimesPeriodInMilliSecs, multiEventsSet, logger);
    checkEncodingTimes->start();
    scheduler.activeTimes(checkEncodingTimes);

    string           contentRetentionTimesSchedule = configuration["scheduler"].get("contentRetentionTimesSchedule", "").asString();
    logger->info(__FILEREF__ + "Creating and Starting RetentionTimes"
        + ", contentRetentionTimesSchedule: " + contentRetentionTimesSchedule
            );
    shared_ptr<RetentionTimes>     retentionTimes =
            make_shared<RetentionTimes>(contentRetentionTimesSchedule, multiEventsSet, logger);
    retentionTimes->start();
    scheduler.activeTimes(retentionTimes);


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

Json::Value loadConfigurationFile(string configurationPathName)
{
    Json::Value configurationJson;
    
    try
    {
        ifstream configurationFile(configurationPathName.c_str(), std::ifstream::binary);
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