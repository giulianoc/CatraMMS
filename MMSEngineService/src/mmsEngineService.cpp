
#include <csignal>
#include <fstream>
#include <thread>

#include "JSONUtils.h"
#include "catralibraries/Scheduler2.h"
#include "catralibraries/Service.h"
#include "catralibraries/System.h"

#include "ActiveEncodingsManager.h"
#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "GEOInfoTimes.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "MMSStorage.h"
#include "Magick++.h"
#include "ThreadsStatisticTimes.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <aws/core/Aws.h>
#include <aws/medialive/MediaLiveClient.h>
#include <aws/medialive/model/DescribeChannelRequest.h>
#include <aws/medialive/model/DescribeChannelResult.h>
#include <aws/medialive/model/StartChannelRequest.h>
#include <aws/medialive/model/StopChannelRequest.h>

json loadConfigurationFile(string configurationPathName);

chrono::system_clock::time_point lastSIGSEGVSignal = chrono::system_clock::now();
void signalHandler(int signal)
{
	long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastSIGSEGVSignal).count();
	if (elapsedInSeconds > 30)
	{
		auto logger = spdlog::get("mmsEngineService");
		logger->error(__FILEREF__ + "Received a signal" + ", signal: " + to_string(signal));
	}
}

int main(int iArgc, char *pArgv[])
{

	if (iArgc != 2 && iArgc != 3 && iArgc != 4)
	{
		cerr << "Usage: " << pArgv[0] << " --nodaemon | --resetdata | --pidfile <pid path file name>" << "config-path-name" << endl;

		return 1;
	}

	bool noDaemon = false;
	bool resetdata = false;
	string pidFilePathName;
	string configPathName;

	if (iArgc == 2)
	{
		// only cfg file
		pidFilePathName = "/tmp/cmsEngine.pid";

		configPathName = pArgv[1];
	}
	else if (iArgc == 3)
	{
		// nodaemon and cfg file
		pidFilePathName = "/tmp/cmsEngine.pid";

		if (!strcmp(pArgv[1], "--nodaemon"))
			noDaemon = true;
		else if (!strcmp(pArgv[1], "--resetdata"))
			resetdata = true;

		configPathName = pArgv[2];
	}
	else if (iArgc == 4)
	{
		// pidfile (2 params) and cfg file
		if (!strcmp(pArgv[1], "--pidfile"))
			pidFilePathName = pArgv[2];
		else
			pidFilePathName = "/tmp/cmsEngine.pid";

		configPathName = pArgv[3];
	}

	if (!noDaemon)
		Service::launchUnixDaemon(pidFilePathName);

	Magick::InitializeMagick(*pArgv);

	json configurationRoot = loadConfigurationFile(configPathName);

	string logPathName = JSONUtils::asString(configurationRoot["log"]["mms"], "pathName", "");
	string logErrorPathName = JSONUtils::asString(configurationRoot["log"]["mms"], "errorPathName", "");
	string logType = JSONUtils::asString(configurationRoot["log"]["mms"], "type", "");
	bool stdout = JSONUtils::asBool(configurationRoot["log"]["mms"], "stdout", false);

	std::vector<spdlog::sink_ptr> sinks;
	{
		string logLevel = JSONUtils::asString(configurationRoot["log"]["api"], "level", "");
		if (logType == "daily")
		{
			int logRotationHour = JSONUtils::asInt(configurationRoot["log"]["mms"]["daily"], "rotationHour", 1);
			int logRotationMinute = JSONUtils::asInt(configurationRoot["log"]["mms"]["daily"], "rotationMinute", 1);

			auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt>(logPathName.c_str(), logRotationHour, logRotationMinute);
			sinks.push_back(dailySink);
			if (logLevel == "debug")
				dailySink->set_level(spdlog::level::debug);
			else if (logLevel == "info")
				dailySink->set_level(spdlog::level::info);
			else if (logLevel == "warn")
				dailySink->set_level(spdlog::level::warn);
			else if (logLevel == "err")
				dailySink->set_level(spdlog::level::err);
			else if (logLevel == "critical")
				dailySink->set_level(spdlog::level::critical);

			auto errorDailySink = make_shared<spdlog::sinks::daily_file_sink_mt>(logErrorPathName.c_str(), logRotationHour, logRotationMinute);
			sinks.push_back(errorDailySink);
			errorDailySink->set_level(spdlog::level::err);
		}
		else if (logType == "rotating")
		{
			int64_t maxSizeInKBytes = JSONUtils::asInt64(configurationRoot["log"]["encoder"]["rotating"], "maxSizeInKBytes", 1000);
			int maxFiles = JSONUtils::asInt(configurationRoot["log"]["mms"]["rotating"], "maxFiles", 1);

			auto rotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logPathName.c_str(), maxSizeInKBytes * 1000, maxFiles);
			sinks.push_back(rotatingSink);
			if (logLevel == "debug")
				rotatingSink->set_level(spdlog::level::debug);
			else if (logLevel == "info")
				rotatingSink->set_level(spdlog::level::info);
			else if (logLevel == "warn")
				rotatingSink->set_level(spdlog::level::warn);
			else if (logLevel == "err")
				rotatingSink->set_level(spdlog::level::err);
			else if (logLevel == "critical")
				rotatingSink->set_level(spdlog::level::critical);

			auto errorRotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logErrorPathName.c_str(), maxSizeInKBytes * 1000, maxFiles);
			sinks.push_back(errorRotatingSink);
			errorRotatingSink->set_level(spdlog::level::err);
		}

		if (stdout)
		{
			auto stdoutSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sinks.push_back(stdoutSink);
			stdoutSink->set_level(spdlog::level::debug);
		}
	}

	auto logger = std::make_shared<spdlog::logger>("mmsEngineService", begin(sinks), end(sinks));
	// globally register the loggers so so the can be accessed using spdlog::get(logger_name)
	spdlog::register_logger(logger);

	// auto logger = spdlog::stdout_logger_mt("mmsEngineService");
	// auto logger = spdlog::daily_logger_mt("mmsEngineService", logPathName.c_str(), 11, 20);

	// trigger flush if the log severity is error or higher
	logger->flush_on(spdlog::level::trace);

	spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off

	string pattern = JSONUtils::asString(configurationRoot["log"]["mms"], "pattern", "");
	spdlog::set_pattern(pattern);

	spdlog::set_default_logger(logger);

	// install a signal handler
	signal(SIGSEGV, signalHandler);
	signal(SIGINT, signalHandler);
	signal(SIGABRT, signalHandler);
	// signal(SIGBUS, signalHandler);

#ifdef __POSTGRES__
	size_t masterDbPoolSize = JSONUtils::asInt(configurationRoot["postgres"]["master"], "enginePoolSize", 5);
	logger->info(__FILEREF__ + "Configuration item" + ", postgres->master->enginePoolSize: " + to_string(masterDbPoolSize));
	size_t slaveDbPoolSize = JSONUtils::asInt(configurationRoot["postgres"]["slave"], "enginePoolSize", 5);
	logger->info(__FILEREF__ + "Configuration item" + ", postgres->slave->enginePoolSize: " + to_string(slaveDbPoolSize));
#else
	size_t masterDbPoolSize = JSONUtils::asInt(configurationRoot["database"]["master"], "enginePoolSize", 5);
	logger->info(__FILEREF__ + "Configuration item" + ", database->master->enginePoolSize: " + to_string(masterDbPoolSize));
	size_t slaveDbPoolSize = JSONUtils::asInt(configurationRoot["database"]["slave"], "enginePoolSize", 5);
	logger->info(__FILEREF__ + "Configuration item" + ", database->slave->enginePoolSize: " + to_string(slaveDbPoolSize));
#endif
	logger->info(__FILEREF__ + "Creating MMSEngineDBFacade");
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(configurationRoot, masterDbPoolSize, slaveDbPoolSize, logger);

	if (resetdata)
	{
		string processorMMS = System::getHostName();

		try
		{
			mmsEngineDBFacade->resetProcessingJobsIfNeeded(processorMMS);
		}
		catch (runtime_error &e)
		{
			logger->error(__FILEREF__ + "mmsEngineDBFacade->resetProcessingJobsIfNeeded failed" + ", exception: " + e.what());

			// throw e;
			return 1;
		}
		catch (exception &e)
		{
			logger->error(__FILEREF__ + "mmsEngineDBFacade->resetProcessingJobsIfNeeded failed");

			// throw e;
			return 1;
		}

		return 0;
	}

	bool noFileSystemAccess = false;
	logger->info(__FILEREF__ + "Creating MMSStorage" + ", noFileSystemAccess: " + to_string(noFileSystemAccess));
	shared_ptr<MMSStorage> mmsStorage = make_shared<MMSStorage>(noFileSystemAccess, mmsEngineDBFacade, configurationRoot, logger);

	logger->info(__FILEREF__ + "Creating MultiEventsSet" + ", addDestination: " + MMSENGINEPROCESSORNAME);
	shared_ptr<MultiEventsSet> multiEventsSet = make_shared<MultiEventsSet>();
	multiEventsSet->addDestination(MMSENGINEPROCESSORNAME);

	logger->info(__FILEREF__ + "Creating ActiveEncodingsManager");
	ActiveEncodingsManager activeEncodingsManager(configurationRoot, multiEventsSet, mmsEngineDBFacade, mmsStorage, logger);

	mutex cpuUsageMutex;
	deque<int> cpuUsage;
	int numberOfLastCPUUsageToBeChecked = 3;
	for (int cpuUsageIndex = 0; cpuUsageIndex < numberOfLastCPUUsageToBeChecked; cpuUsageIndex++)
	{
		cpuUsage.push_front(0);
	}

	shared_ptr<ThreadsStatistic> mmsThreadsStatistic = make_shared<ThreadsStatistic>(logger);

	shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization =
		make_shared<MMSDeliveryAuthorization>(configurationRoot, mmsStorage, mmsEngineDBFacade, logger);

	vector<shared_ptr<MMSEngineProcessor>> mmsEngineProcessors;
	{
		int processorThreads = JSONUtils::asInt(configurationRoot["mms"], "processorThreads", 1);
		shared_ptr<long> processorsThreadsNumber = make_shared<long>(0);

		for (int processorThreadIndex = 0; processorThreadIndex < processorThreads; processorThreadIndex++)
		{
			logger->info(__FILEREF__ + "Creating MMSEngineProcessor" + ", processorThreadIndex: " + to_string(processorThreadIndex));
			shared_ptr<MMSEngineProcessor> mmsEngineProcessor = make_shared<MMSEngineProcessor>(
				processorThreadIndex, logger, multiEventsSet, mmsEngineDBFacade, mmsStorage, processorsThreadsNumber, mmsThreadsStatistic,
				mmsDeliveryAuthorization, &activeEncodingsManager, &cpuUsageMutex, &cpuUsage, configurationRoot
			);
			mmsEngineProcessors.push_back(mmsEngineProcessor);
		}
	}

	unsigned long ulThreadSleepInMilliSecs = JSONUtils::asInt(configurationRoot["scheduler"], "threadSleepInMilliSecs", 5);
	logger->info(__FILEREF__ + "Creating Scheduler2" + ", ulThreadSleepInMilliSecs: " + to_string(ulThreadSleepInMilliSecs));
	Scheduler2 scheduler(ulThreadSleepInMilliSecs);

	logger->info(__FILEREF__ + "Starting ActiveEncodingsManager");
	thread activeEncodingsManagerThread(ref(activeEncodingsManager));

	vector<shared_ptr<thread>> mmsEngineProcessorsThread;
	{
		//    thread mmsEngineProcessorThread (mmsEngineProcessor);
		for (int mmsProcessorIndex = 0; mmsProcessorIndex < mmsEngineProcessors.size(); mmsProcessorIndex++)
		{
			logger->info(__FILEREF__ + "Starting MMSEngineProcessor" + ", mmsProcessorIndex: " + to_string(mmsProcessorIndex));
			mmsEngineProcessorsThread.push_back(make_shared<thread>(&MMSEngineProcessor::operator(), mmsEngineProcessors[mmsProcessorIndex]));

			if (mmsProcessorIndex == 0)
			{
				thread cpuUsageThread(&MMSEngineProcessor::cpuUsageThread, mmsEngineProcessors[mmsProcessorIndex]);
				cpuUsageThread.detach();
			}
		}
	}

	logger->info(__FILEREF__ + "Starting Scheduler2");
	thread schedulerThread(ref(scheduler));

	unsigned long checkIngestionTimesPeriodInMilliSecs =
		JSONUtils::asInt(configurationRoot["scheduler"], "checkIngestionTimesPeriodInMilliSecs", 2000);
	logger->info(
		__FILEREF__ + "Creating and Starting CheckIngestionTimes" +
		", checkIngestionTimesPeriodInMilliSecs: " + to_string(checkIngestionTimesPeriodInMilliSecs)
	);
	shared_ptr<CheckIngestionTimes> checkIngestionTimes =
		make_shared<CheckIngestionTimes>(checkIngestionTimesPeriodInMilliSecs, multiEventsSet, logger);
	checkIngestionTimes->start();
	scheduler.activeTimes(checkIngestionTimes);

	unsigned long checkEncodingTimesPeriodInMilliSecs =
		JSONUtils::asInt(configurationRoot["scheduler"], "checkEncodingTimesPeriodInMilliSecs", 10000);
	logger->info(
		__FILEREF__ + "Creating and Starting CheckEncodingTimes" +
		", checkEncodingTimesPeriodInMilliSecs: " + to_string(checkEncodingTimesPeriodInMilliSecs)
	);
	shared_ptr<CheckEncodingTimes> checkEncodingTimes = make_shared<CheckEncodingTimes>(checkEncodingTimesPeriodInMilliSecs, multiEventsSet, logger);
	checkEncodingTimes->start();
	scheduler.activeTimes(checkEncodingTimes);

	unsigned long threadsStatisticTimesPeriodInMilliSecs =
		JSONUtils::asInt(configurationRoot["scheduler"], "threadsStatisticTimesPeriodInMilliSecs", 60000);
	logger->info(
		__FILEREF__ + "Creating and Starting ThreadsStatisticTimes" +
		", threadsStatisticTimesPeriodInMilliSecs: " + to_string(threadsStatisticTimesPeriodInMilliSecs)
	);
	shared_ptr<ThreadsStatisticTimes> threadsStatisticTimes =
		make_shared<ThreadsStatisticTimes>(threadsStatisticTimesPeriodInMilliSecs, multiEventsSet, logger);
	threadsStatisticTimes->start();
	scheduler.activeTimes(threadsStatisticTimes);

	string contentRetentionTimesSchedule = JSONUtils::asString(configurationRoot["scheduler"], "contentRetentionTimesSchedule", "");
	logger->info(__FILEREF__ + "Creating and Starting ContentRetentionTimes" + ", contentRetentionTimesSchedule: " + contentRetentionTimesSchedule);
	shared_ptr<ContentRetentionTimes> contentRetentionTimes =
		make_shared<ContentRetentionTimes>(contentRetentionTimesSchedule, multiEventsSet, logger);
	contentRetentionTimes->start();
	scheduler.activeTimes(contentRetentionTimes);

	string dbDataRetentionTimesSchedule = JSONUtils::asString(configurationRoot["scheduler"], "dbDataRetentionTimesSchedule", "");
	logger->info(__FILEREF__ + "Creating and Starting DBDataRetentionTimes" + ", dbDataRetentionTimesSchedule: " + dbDataRetentionTimesSchedule);
	shared_ptr<DBDataRetentionTimes> dbDataRetentionTimes = make_shared<DBDataRetentionTimes>(dbDataRetentionTimesSchedule, multiEventsSet, logger);
	dbDataRetentionTimes->start();
	scheduler.activeTimes(dbDataRetentionTimes);

	string geoInfoTimesSchedule = JSONUtils::asString(configurationRoot["scheduler"], "geoInfoTimesSchedule", "");
	logger->info(__FILEREF__ + "Creating and Starting GEOInfoTimes" + ", geoInfoTimesSchedule: " + geoInfoTimesSchedule);
	shared_ptr<GEOInfoTimes> geoInfoTimes = make_shared<GEOInfoTimes>(geoInfoTimesSchedule, multiEventsSet, logger);
	geoInfoTimes->start();
	scheduler.activeTimes(geoInfoTimes);

	string checkRefreshPartitionFreeSizeTimesSchedule =
		JSONUtils::asString(configurationRoot["scheduler"], "checkRefreshPartitionFreeSizeTimesSchedule", "");
	logger->info(
		__FILEREF__ + "Creating and Starting CheckRefreshPartitionFreeSizeTimes" +
		", checkRefreshPartitionFreeSizeTimesSchedule: " + checkRefreshPartitionFreeSizeTimesSchedule
	);
	shared_ptr<CheckRefreshPartitionFreeSizeTimes> checkRefreshPartitionFreeSizeTimes =
		make_shared<CheckRefreshPartitionFreeSizeTimes>(checkRefreshPartitionFreeSizeTimesSchedule, multiEventsSet, logger);
	checkRefreshPartitionFreeSizeTimes->start();
	scheduler.activeTimes(checkRefreshPartitionFreeSizeTimes);

	Aws::SDKOptions options;
	Aws::InitAPI(options);

	logger->info(__FILEREF__ + "Waiting ActiveEncodingsManager");
	activeEncodingsManagerThread.join();

	Aws::ShutdownAPI(options);

	{
		for (int mmsProcessorIndex = 0; mmsProcessorIndex < mmsEngineProcessorsThread.size(); mmsProcessorIndex++)
		{
			logger->info(__FILEREF__ + "Waiting MMSEngineProcessor" + ", mmsProcessorIndex: " + to_string(mmsProcessorIndex));
			// I guess if join is called once the thread is already exits generates
			// memory leak. I do not care about this because the process is going down
			mmsEngineProcessorsThread[mmsProcessorIndex]->join();

			if (mmsProcessorIndex == 0)
				mmsEngineProcessors[mmsProcessorIndex]->stopCPUUsageThread();
		}
	}

	scheduler.cancel();
	logger->info(__FILEREF__ + "Waiting Scheduler2");
	// I guess if join is called once the thread is already exits generates
	// memory leak. I do not care about this because the process is going down
	schedulerThread.join();

	logger->info(__FILEREF__ + "Shutdown done");

	return 0;
}

json loadConfigurationFile(string configurationPathName)
{
	try
	{
		ifstream configurationFile(configurationPathName.c_str(), std::ifstream::binary);
		return json::parse(
			configurationFile,
			nullptr, // callback
			true,	 // allow exceptions
			true	 // ignore_comments
		);
	}
	catch (...)
	{
		string errorMessage = fmt::format(
			"wrong json configuration format"
			", configurationPathName: {}",
			configurationPathName
		);

		throw runtime_error(errorMessage);
	}
}
