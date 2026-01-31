
#include <iostream>
#include <csignal>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "CurlWrapper.h"
#include "spdlog/common.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "API.h"
#include "CPUUsageThread.h"
#include "BandwidthUsageThread.h"
#include "JSONUtils.h"
#include "JsonPath.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;

chrono::system_clock::time_point lastSIGSEGVSignal = chrono::system_clock::now();
void signalHandler(int signal)
{
	if (signal == 11) // SIGSEGV
	{
		long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastSIGSEGVSignal).count();
		if (elapsedInSeconds > 60) // è inutile loggare infiniti errori, ne loggo solo uno ogni 60 secondi
		{
			lastSIGSEGVSignal = chrono::system_clock::now();
			LOG_ERROR(
				"Received a signal"
				", signal: {}",
				signal
			);
		}
	}
	else
		LOG_ERROR(
			"Received a signal"
			", signal: {}",
			signal
		);
}

std::shared_ptr<spdlog::sinks::sink> buildErrorSink(json configurationRoot)
{
	const string logErrorPathName = JSONUtils::as<string>(configurationRoot["log"]["api"], "errorPathName", "");
	string logType = JSONUtils::as<string>(configurationRoot["log"]["api"], "type", "");

	std::shared_ptr<spdlog::sinks::sink> errorSink;
	if (logType == "daily")
	{
		int logRotationHour = JSONUtils::as<int32_t>(configurationRoot["log"]["api"]["daily"], "rotationHour", 1);
		int logRotationMinute = JSONUtils::as<int32_t>(configurationRoot["log"]["api"]["daily"], "rotationMinute", 1);

		errorSink = make_shared<spdlog::sinks::daily_file_sink_mt>(logErrorPathName.c_str(), logRotationHour, logRotationMinute);
	}
	else if (logType == "rotating")
	{
		int64_t maxSizeInKBytes = JSONUtils::as<int64_t>(configurationRoot["log"]["api"]["rotating"], "maxSizeInKBytes", 1000);
		int maxFiles = JSONUtils::as<int32_t>(configurationRoot["log"]["api"]["rotating"], "maxFiles", 10);

		errorSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logErrorPathName.c_str(), maxSizeInKBytes * 1000, maxFiles);
	}
	errorSink->set_level(spdlog::level::err);

	return errorSink;
}

shared_ptr<spdlog::logger> setMainLogger(json configurationRoot, const std::shared_ptr<spdlog::sinks::sink>& errorSink)
{
	string logPathName = JSONUtils::as<string>(configurationRoot["log"]["api"], "pathName", "");
	string logType = JSONUtils::as<string>(configurationRoot["log"]["api"], "type", "");
	bool stdout = JSONUtils::as<bool>(configurationRoot["log"]["api"], "stdout", false);

	std::vector<spdlog::sink_ptr> sinks;
	{
		string logLevel = JSONUtils::as<string>(configurationRoot["log"]["api"], "level", "");
		if (logType == "daily")
		{
			int logRotationHour = JSONUtils::as<int32_t>(configurationRoot["log"]["api"]["daily"], "rotationHour", 1);
			int logRotationMinute = JSONUtils::as<int32_t>(configurationRoot["log"]["api"]["daily"], "rotationMinute", 1);

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
		}
		else if (logType == "rotating")
		{
			int64_t maxSizeInKBytes = JSONUtils::as<int64_t>(configurationRoot["log"]["api"]["rotating"], "maxSizeInKBytes", 1000);
			int maxFiles = JSONUtils::as<int32_t>(configurationRoot["log"]["api"]["rotating"], "maxFiles", 10);

			// livello di log sul sink
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
		}
		sinks.push_back(errorSink);

		if (stdout)
		{
			auto stdoutSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sinks.push_back(stdoutSink);
			stdoutSink->set_level(spdlog::level::debug);
		}
	}

	auto logger = std::make_shared<spdlog::logger>("api-log", begin(sinks), end(sinks));
	spdlog::register_logger(logger);

	// trigger flush if the log severity is error or higher
	logger->flush_on(spdlog::level::trace);

	// livello di log sul logger
	logger->set_level(spdlog::level::trace); // trace, debug, info, warn, err, critical, off

	string pattern = JSONUtils::as<string>(configurationRoot["log"]["api"], "pattern", "");
	spdlog::set_pattern(pattern);

	// globally register the loggers so so the can be accessed using spdlog::get(logger_name)
	// spdlog::register_logger(logger);

	spdlog::set_default_logger(logger);
	// livello di log globale
	spdlog::set_level(spdlog::level::trace);

	return logger;
}

void registerSlowQueryLogger(json configurationRoot)
{
	string logPathName = JSONUtils::as<string>(configurationRoot["log"]["api"]["slowQuery"], "pathName", "");
	LOG_INFO(
		"Configuration item"
		", log->api->slowQuery->pathName: {}",
		logPathName
	);
	string logType = JSONUtils::as<string>(configurationRoot["log"]["api"], "type", "");
	LOG_INFO(
		"Configuration item"
		", log->api->type: {}",
		logType
	);

	LOG_INFO("registerSlowQueryLogger");
	std::vector<spdlog::sink_ptr> sinks;
	{
		if (logType == "daily")
		{
			int logRotationHour = JSONUtils::as<int32_t>(configurationRoot["log"]["api"]["daily"], "rotationHour", 1);
			int logRotationMinute = JSONUtils::as<int32_t>(configurationRoot["log"]["api"]["daily"], "rotationMinute", 1);

			auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt>(logPathName.c_str(), logRotationHour, logRotationMinute);
			sinks.push_back(dailySink);
			dailySink->set_level(spdlog::level::warn);
		}
		else if (logType == "rotating")
		{
			int64_t maxSizeInKBytes = JSONUtils::as<int64_t>(configurationRoot["log"]["api"]["rotating"], "maxSizeInKBytes", 1000);
			int maxFiles = JSONUtils::as<int32_t>(configurationRoot["log"]["api"]["rotating"], "maxFiles", 10);

			auto rotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logPathName.c_str(), maxSizeInKBytes * 1000, maxFiles);
			sinks.push_back(rotatingSink);
			rotatingSink->set_level(spdlog::level::warn);
		}
	}

	auto logger = std::make_shared<spdlog::logger>("slow-query-log", begin(sinks), end(sinks));
	spdlog::register_logger(logger);

	// trigger flush if the log severity is error or higher
	logger->flush_on(spdlog::level::trace);

	// inizializza il livello del logger a trace in modo che ogni messaggio possa raggiungere i logger nei sinks
	logger->set_level(spdlog::level::trace); // trace, debug, info, warn, err, critical, off

	string pattern = JSONUtils::as<string>(configurationRoot["log"]["api"], "pattern", "");
	LOG_INFO(
		"Configuration item"
		", log->api->pattern: {}",
		pattern
	);
	logger->set_pattern(pattern);

	// logger->warn("Test...");
}

void registerNewLogger(const json& configurationLoggerRoot, const string& loggerName, const std::shared_ptr<spdlog::sinks::sink>& errorSink)
{
	auto logPathName = JsonPath(&configurationLoggerRoot)[loggerName]["pathName"].as<string>();
	LOG_INFO(
		"Configuration item"
		", log->{}->pathName: {}",
		loggerName, logPathName
	);
	auto logType = JsonPath(&configurationLoggerRoot)[loggerName]["type"].as<string>();
	LOG_INFO(
		"Configuration item"
		", log->{}->type: {}",
		loggerName, logType
	);
	auto logLevel = JsonPath(&configurationLoggerRoot)[loggerName]["level"].as<string>();
	LOG_INFO(
		"Configuration item"
		", log->{}->level: {}",
		loggerName, logLevel
	);

	LOG_INFO("registerLogger");
	std::vector<spdlog::sink_ptr> sinks;
	{
		if (logType == "daily")
		{
			int logRotationHour = JsonPath(&configurationLoggerRoot)["daily"]["rotationHour"].as<int32_t>(1);
			int logRotationMinute = JsonPath(&configurationLoggerRoot)["daily"]["rotationMinute"].as<int32_t>(1);

			const auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt>(logPathName.c_str(), logRotationHour, logRotationMinute);
			sinks.push_back(dailySink);

			// livello di log sul sink
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
		}
		else if (logType == "rotating")
		{
			const auto maxSizeInKBytes = JsonPath(&configurationLoggerRoot)["rotating"]["maxSizeInKBytes"].as<int64_t>(1000);
			int maxFiles = JsonPath(&configurationLoggerRoot)["rotating"]["maxFiles"].as<int32_t>(10);

			const auto rotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logPathName.c_str(), maxSizeInKBytes * 1000, maxFiles);
			sinks.push_back(rotatingSink);

			// livello di log sul sink
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
		}
	}
	sinks.push_back(errorSink);

	const auto logger = std::make_shared<spdlog::logger>(std::format("{}-log", loggerName), begin(sinks), end(sinks));
	spdlog::register_logger(logger);

	// trigger flush if the log severity is error or higher
	logger->flush_on(spdlog::level::trace);

	// inizializza il livello del logger a trace in modo che ogni messaggio possa raggiungere i logger nei sinks
	logger->set_level(spdlog::level::trace); // trace, debug, info, warn, err, critical, off

	auto pattern = JsonPath(&configurationLoggerRoot)["pattern"].as<string>();
	LOG_INFO(
		"Configuration item"
		", log->pattern: {}",
		pattern
	);
	logger->set_pattern(pattern);

	// logger->warn("Test...");
}

int main(int argc, char **argv)
{
	try
	{
		bool noFileSystemAccess = false;

		// utilizzato nel caso di "external delivery", cioè un delivery distribuito come una CDN.
		// In questo caso non bisogna accedere al database e tutte le richieste vengono autorizzate tramite controllo dell'autorization tramite path
		bool noDatabaseAccess = false;

		if (argc == 2)
		{
			string sAPIType = argv[1];
			if (sAPIType == "NoFileSystem")
				noFileSystemAccess = true;
			else if (sAPIType == "NoDatabase")
				noDatabaseAccess = true;
		}

		CurlWrapper::globalInitialize();

		// Init libxml
		{
			xmlInitParser();
			LIBXML_TEST_VERSION
		}

		const char *configurationPathName = getenv("MMS_CONFIGPATHNAME");
		if (configurationPathName == nullptr)
		{
			cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;

			return 1;
		}

		auto configurationRoot = JSONUtils::loadConfigurationFile<json>(configurationPathName, "MMS_");

		std::shared_ptr<spdlog::sinks::sink> errorSink = buildErrorSink(configurationRoot);
		shared_ptr<spdlog::logger> logger = setMainLogger(configurationRoot, errorSink);
		registerSlowQueryLogger(configurationRoot);
		registerNewLogger(JsonPath(&configurationRoot)["log"]["api"].as<json>(), "stats", errorSink);

		// install a signal handler
		signal(SIGSEGV, signalHandler);
		signal(SIGINT, signalHandler);
		signal(SIGABRT, signalHandler);
		// signal(SIGBUS, signalHandler);

#ifdef __POSTGRES__
		size_t masterDbPoolSize;
		if (noDatabaseAccess)
			masterDbPoolSize = 0;
		else
			masterDbPoolSize = JSONUtils::as<int32_t>(configurationRoot["postgres"]["master"], "apiPoolSize", 5);
		LOG_INFO(
			"Configuration item"
			", postgres->master->apiPoolSize: {}",
			masterDbPoolSize
		);
		size_t slaveDbPoolSize;
		if (noDatabaseAccess)
			slaveDbPoolSize = 0;
		else
			slaveDbPoolSize = JSONUtils::as<int32_t>(configurationRoot["postgres"]["slave"], "apiPoolSize", 5);
		LOG_INFO(
			"Configuration item"
			", postgres->slave->apiPoolSize: {}",
			slaveDbPoolSize
		);
#else
		size_t masterDbPoolSize = JSONUtils::as<int32_t>(configuration["database"]["master"], "apiPoolSize", 5);
		info(__FILEREF__ + "Configuration item" + ", database->master->apiPoolSize: " + to_string(masterDbPoolSize));
		size_t slaveDbPoolSize = JSONUtils::as<int32_t>(configuration["database"]["slave"], "apiPoolSize", 5);
		info(__FILEREF__ + "Configuration item" + ", database->slave->apiPoolSize: " + to_string(slaveDbPoolSize));
#endif
		LOG_INFO("Creating MMSEngineDBFacade");
		auto mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(configurationRoot,
			JsonPath(&configurationRoot)["log"]["api"]["slowQuery"].as<json>(),
			masterDbPoolSize, slaveDbPoolSize, logger);

		LOG_TRACE(
			"Creating MMSStorage"
			", noFileSystemAccess: {}",
			noFileSystemAccess
		);
		auto mmsStorage = make_shared<MMSStorage>(noFileSystemAccess, noDatabaseAccess, mmsEngineDBFacade, configurationRoot, logger);

		auto mmsDeliveryAuthorization =
			make_shared<MMSDeliveryAuthorization>(configurationRoot, mmsStorage, mmsEngineDBFacade);
		mmsDeliveryAuthorization->startUpdateExternalDeliveriesGroupsBandwidthUsageThread();

		FCGX_Init();

		int threadsNumber = JSONUtils::as<int32_t>(configurationRoot["api"], "threadsNumber", 1);
		LOG_INFO(
			"Configuration item"
			", api->threadsNumber: {}",
			threadsNumber
		);

		mutex fcgiAcceptMutex;
		API::FileUploadProgressData fileUploadProgressData;

		auto bandwidthUsageInterfaceNameToMonitor = JsonPath(&configurationRoot)["api"]["bandwithUsageInterfaceName"].as<string>();
		std::optional<std::string> optInterfaceNameToMonitor = nullopt;
		if (!bandwidthUsageInterfaceNameToMonitor.empty() && !bandwidthUsageInterfaceNameToMonitor.starts_with("${"))
			optInterfaceNameToMonitor = bandwidthUsageInterfaceNameToMonitor;
		auto bandwidthUsageThread = make_shared<BandwidthUsageThread>(optInterfaceNameToMonitor);
		bandwidthUsageThread->start();

		auto cpuStatsUpdateIntervalInSeconds = JsonPath(&configurationRoot)["scheduler"]["cpuStatsUpdateIntervalInSeconds"].
			as<int16_t>(10);
		const auto cpuUsageThread = make_shared<CPUUsageThread>(cpuStatsUpdateIntervalInSeconds);
		cpuUsageThread->start();

		vector<shared_ptr<API>> apis;
		vector<thread> apiThreads;

		for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
		{
			auto api = make_shared<API>(noFileSystemAccess, configurationRoot, mmsEngineDBFacade, mmsStorage, mmsDeliveryAuthorization,
				&fcgiAcceptMutex, &fileUploadProgressData, bandwidthUsageThread);

			apis.push_back(api);
			apiThreads.emplace_back(&API::operator(), api);
		}

		// shutdown should be managed in some way:
		// - mod_fcgid send just one shutdown, so only one thread will go down
		// - mod_fastcgi ???
		if (threadsNumber > 0)
		{
			thread fileUploadProgressThread(&API::fileUploadProgressCheckThread, apis[0]);

			apiThreads[0].join();

			apis[0]->stopUploadFileProgressThread();
		}

		cpuUsageThread->stop();
		bandwidthUsageThread->stop();
		mmsDeliveryAuthorization->stopUpdateExternalDeliveriesGroupsBandwidthUsageThread();

		LOG_INFO("API shutdown");

		// libxml
		{
			// Shutdown libxml
			xmlCleanupParser();

			// this is to debug memory for regression tests
			xmlMemoryDump();
		}

		CurlWrapper::globalTerminate();
	}
	catch (exception &e)
	{
		cerr << std::format("main failed"
			", exception: {}", e.what()) << endl;

		// throw runtime_error(errorMessage);
		return 1;
	}

	return 0;
}
