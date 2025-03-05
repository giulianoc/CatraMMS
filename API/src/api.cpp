
#include <csignal>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "CurlWrapper.h"
#include "spdlog/common.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "API.h"
#include "JSONUtils.h"
#include "spdlog/spdlog.h"

chrono::system_clock::time_point lastSIGSEGVSignal = chrono::system_clock::now();
void signalHandler(int signal)
{
	if (signal == 11) // SIGSEGV
	{
		long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastSIGSEGVSignal).count();
		if (elapsedInSeconds > 60) // è inutile loggare infiniti errori, ne loggo solo uno ogni 60 secondi
		{
			lastSIGSEGVSignal = chrono::system_clock::now();
			SPDLOG_ERROR(
				"Received a signal"
				", signal: {}",
				signal
			);
		}
	}
	else
		SPDLOG_ERROR(
			"Received a signal"
			", signal: {}",
			signal
		);
}

shared_ptr<spdlog::logger> setMainLogger(json configurationRoot)
{
	string logPathName = JSONUtils::asString(configurationRoot["log"]["api"], "pathName", "");
	string logErrorPathName = JSONUtils::asString(configurationRoot["log"]["api"], "errorPathName", "");
	string logType = JSONUtils::asString(configurationRoot["log"]["api"], "type", "");
	bool stdout = JSONUtils::asBool(configurationRoot["log"]["api"], "stdout", false);

	std::vector<spdlog::sink_ptr> sinks;
	{
		string logLevel = JSONUtils::asString(configurationRoot["log"]["api"], "level", "");
		if (logType == "daily")
		{
			int logRotationHour = JSONUtils::asInt(configurationRoot["log"]["api"]["daily"], "rotationHour", 1);
			int logRotationMinute = JSONUtils::asInt(configurationRoot["log"]["api"]["daily"], "rotationMinute", 1);

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
			int64_t maxSizeInKBytes = JSONUtils::asInt64(configurationRoot["log"]["api"]["rotating"], "maxSizeInKBytes", 1000);
			int maxFiles = JSONUtils::asInt(configurationRoot["log"]["api"]["rotating"], "maxFiles", 10);

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

	auto logger = std::make_shared<spdlog::logger>("API", begin(sinks), end(sinks));
	spdlog::register_logger(logger);

	// trigger flush if the log severity is error or higher
	logger->flush_on(spdlog::level::trace);

	logger->set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off

	string pattern = JSONUtils::asString(configurationRoot["log"]["api"], "pattern", "");
	spdlog::set_pattern(pattern);

	// globally register the loggers so so the can be accessed using spdlog::get(logger_name)
	// spdlog::register_logger(logger);

	spdlog::set_default_logger(logger);

	return logger;
}

void registerSlowQueryLogger(json configurationRoot)
{
	string logPathName = JSONUtils::asString(configurationRoot["log"]["api"]["slowQuery"], "pathName", "");
	SPDLOG_INFO(
		"Configuration item"
		", log->api->slowQuery->pathName: {}",
		logPathName
	);
	string logType = JSONUtils::asString(configurationRoot["log"]["api"], "type", "");
	SPDLOG_INFO(
		"Configuration item"
		", log->api->type: {}",
		logType
	);

	SPDLOG_INFO("registerSlowQueryLogger");
	std::vector<spdlog::sink_ptr> sinks;
	{
		if (logType == "daily")
		{
			int logRotationHour = JSONUtils::asInt(configurationRoot["log"]["api"]["daily"], "rotationHour", 1);
			int logRotationMinute = JSONUtils::asInt(configurationRoot["log"]["api"]["daily"], "rotationMinute", 1);

			auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt>(logPathName.c_str(), logRotationHour, logRotationMinute);
			sinks.push_back(dailySink);
			dailySink->set_level(spdlog::level::warn);
		}
		else if (logType == "rotating")
		{
			int64_t maxSizeInKBytes = JSONUtils::asInt64(configurationRoot["log"]["api"]["rotating"], "maxSizeInKBytes", 1000);
			int maxFiles = JSONUtils::asInt(configurationRoot["log"]["api"]["rotating"], "maxFiles", 10);

			auto rotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logPathName.c_str(), maxSizeInKBytes * 1000, maxFiles);
			sinks.push_back(rotatingSink);
			rotatingSink->set_level(spdlog::level::warn);
		}
	}

	auto logger = std::make_shared<spdlog::logger>("slow-query", begin(sinks), end(sinks));
	spdlog::register_logger(logger);

	// trigger flush if the log severity is error or higher
	logger->flush_on(spdlog::level::trace);

	// inizializza il livello del logger a trace in modo che ogni messaggio possa raggiungere i logger nei sinks
	logger->set_level(spdlog::level::trace); // trace, debug, info, warn, err, critical, off

	string pattern = JSONUtils::asString(configurationRoot["log"]["api"], "pattern", "");
	SPDLOG_INFO(
		"Configuration item"
		", log->api->pattern: {}",
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

		if (argc == 2)
		{
			string sAPIType = argv[1];
			if (sAPIType == "NoFileSystem")
				noFileSystemAccess = true;
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

		json configuration = FastCGIAPI::loadConfigurationFile(configurationPathName, "MMS_");

		shared_ptr<spdlog::logger> logger = setMainLogger(configuration);
		registerSlowQueryLogger(configuration);

		// install a signal handler
		signal(SIGSEGV, signalHandler);
		signal(SIGINT, signalHandler);
		signal(SIGABRT, signalHandler);
		// signal(SIGBUS, signalHandler);

#ifdef __POSTGRES__
		size_t masterDbPoolSize = JSONUtils::asInt(configuration["postgres"]["master"], "apiPoolSize", 5);
		SPDLOG_INFO(
			"Configuration item"
			", postgres->master->apiPoolSize: {}",
			masterDbPoolSize
		);
		size_t slaveDbPoolSize = JSONUtils::asInt(configuration["postgres"]["slave"], "apiPoolSize", 5);
		SPDLOG_INFO(
			"Configuration item"
			", postgres->slave->apiPoolSize: {}",
			slaveDbPoolSize
		);
#else
		size_t masterDbPoolSize = JSONUtils::asInt(configuration["database"]["master"], "apiPoolSize", 5);
		info(__FILEREF__ + "Configuration item" + ", database->master->apiPoolSize: " + to_string(masterDbPoolSize));
		size_t slaveDbPoolSize = JSONUtils::asInt(configuration["database"]["slave"], "apiPoolSize", 5);
		info(__FILEREF__ + "Configuration item" + ", database->slave->apiPoolSize: " + to_string(slaveDbPoolSize));
#endif
		SPDLOG_INFO("Creating MMSEngineDBFacade");
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade =
			make_shared<MMSEngineDBFacade>(configuration, configuration["log"]["api"]["slowQuery"], masterDbPoolSize, slaveDbPoolSize, logger);

		SPDLOG_INFO(
			"Creating MMSStorage"
			", noFileSystemAccess: {}",
			noFileSystemAccess
		);
		shared_ptr<MMSStorage> mmsStorage = make_shared<MMSStorage>(noFileSystemAccess, mmsEngineDBFacade, configuration, logger);

		shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization =
			make_shared<MMSDeliveryAuthorization>(configuration, mmsStorage, mmsEngineDBFacade);

		FCGX_Init();

		int threadsNumber = JSONUtils::asInt(configuration["api"], "threadsNumber", 1);
		SPDLOG_INFO(
			"Configuration item"
			", api->threadsNumber: {}",
			threadsNumber
		);

		mutex fcgiAcceptMutex;
		API::FileUploadProgressData fileUploadProgressData;

		vector<shared_ptr<API>> apis;
		vector<thread> apiThreads;

		for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
		{
			shared_ptr<API> api = make_shared<API>(
				noFileSystemAccess, configuration, mmsEngineDBFacade, mmsStorage, mmsDeliveryAuthorization, &fcgiAcceptMutex, &fileUploadProgressData
			);

			apis.push_back(api);
			apiThreads.push_back(thread(&API::operator(), api));
		}

		// shutdown should be managed in some way:
		// - mod_fcgid send just one shutdown, so only one thread will go down
		// - mod_fastcgi ???
		if (threadsNumber > 0)
		{
			thread fileUploadProgressThread(&API::fileUploadProgressCheck, apis[0]);

			apiThreads[0].join();

			apis[0]->stopUploadFileProgressThread();
		}

		SPDLOG_INFO("API shutdown");

		// libxml
		{
			// Shutdown libxml
			xmlCleanupParser();

			// this is to debug memory for regression tests
			xmlMemoryDump();
		}

		CurlWrapper::globalTerminate();
	}
	catch (sql::SQLException &se)
	{
		cerr << __FILEREF__ + "main failed. SQL exception" + ", se.what(): " + se.what();

		// throw se;
		return 1;
	}
	catch (runtime_error &e)
	{
		cerr << __FILEREF__ + "main failed" + ", e.what(): " + e.what();

		// throw e;
		return 1;
	}
	catch (exception &e)
	{
		cerr << __FILEREF__ + "main failed" + ", e.what(): " + e.what();

		// throw runtime_error(errorMessage);
		return 1;
	}

	return 0;
}
