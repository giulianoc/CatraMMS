
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include "JSONUtils.h"
#include "API.h"

int main(int argc, char** argv) 
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

		// Init libxml
		{
			xmlInitParser();
			LIBXML_TEST_VERSION
		}

		const char* configurationPathName = getenv("MMS_CONFIGPATHNAME");
		if (configurationPathName == nullptr)
		{
			cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;
        
			return 1;
		}
    
		Json::Value configuration = FastCGIAPI::loadConfigurationFile(configurationPathName);
    
		string logPathName =  JSONUtils::asString(configuration["log"]["api"], "pathName", "");
		string logErrorPathName =  JSONUtils::asString(configuration["log"]["api"], "errorPathName", "");
		string logType =  JSONUtils::asString(configuration["log"]["api"], "type", "");
		bool stdout =  JSONUtils::asBool(configuration["log"]["api"], "stdout", false);
    
		std::vector<spdlog::sink_ptr> sinks;
		{
			string logLevel =  JSONUtils::asString(configuration["log"]["api"], "level", "");
			if(logType == "daily")
			{
				int logRotationHour = JSONUtils::asInt(configuration["log"]["api"]["daily"],
					"rotationHour", 1);
				int logRotationMinute = JSONUtils::asInt(configuration["log"]["api"]["daily"],
					"rotationMinute", 1);

				auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logPathName.c_str(),
					logRotationHour, logRotationMinute);
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

				auto errorDailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logErrorPathName.c_str(),
					logRotationHour, logRotationMinute);
				sinks.push_back(errorDailySink);
				errorDailySink->set_level(spdlog::level::err);
			}
			else if(logType == "rotating")
			{
				int64_t maxSizeInKBytes = JSONUtils::asInt64(configuration["log"]["api"]["rotating"],
					"maxSizeInKBytes", 1000);
				int maxFiles = JSONUtils::asInt(configuration["log"]["api"]["rotating"],
					"maxFiles", 10);

				auto rotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt> (logPathName.c_str(),
					maxSizeInKBytes * 1000, maxFiles);
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

				auto errorRotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt> (logErrorPathName.c_str(),
					maxSizeInKBytes * 1000, maxFiles);
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
    
		spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off

		string pattern =  JSONUtils::asString(configuration["log"]["api"], "pattern", "");
		spdlog::set_pattern(pattern);

		// globally register the loggers so so the can be accessed using spdlog::get(logger_name)
		// spdlog::register_logger(logger);

		spdlog::set_default_logger(logger);

		size_t masterDbPoolSize = JSONUtils::asInt(configuration["database"]["master"], "apiPoolSize", 5);
		logger->info(__FILEREF__ + "Configuration item"
			+ ", database->master->apiPoolSize: " + to_string(masterDbPoolSize)
		);
		size_t slaveDbPoolSize = JSONUtils::asInt(configuration["database"]["slave"], "apiPoolSize", 5);
		logger->info(__FILEREF__ + "Configuration item"
			+ ", database->slave->apiPoolSize: " + to_string(slaveDbPoolSize)
		);
		logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, masterDbPoolSize, slaveDbPoolSize, logger);

		logger->info(__FILEREF__ + "Creating MMSStorage"
			+ ", noFileSystemAccess: " + to_string(noFileSystemAccess)
		);
		shared_ptr<MMSStorage> mmsStorage = make_shared<MMSStorage>(
			noFileSystemAccess, mmsEngineDBFacade, configuration, logger);

		shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization =
			make_shared<MMSDeliveryAuthorization>(configuration,
			mmsStorage, mmsEngineDBFacade, logger);

		FCGX_Init();

		int threadsNumber = JSONUtils::asInt(configuration["api"], "threadsNumber", 1);
		logger->info(__FILEREF__ + "Configuration item"
			+ ", api->threadsNumber: " + to_string(threadsNumber)
		);

		mutex fcgiAcceptMutex;
		API::FileUploadProgressData fileUploadProgressData;

		vector<shared_ptr<API>> apis;
		vector<thread> apiThreads;

		for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
		{
			shared_ptr<API> api = make_shared<API>(
				noFileSystemAccess,
				configuration, 
                mmsEngineDBFacade,
				mmsStorage,
				mmsDeliveryAuthorization,
                &fcgiAcceptMutex,
                &fileUploadProgressData,
                logger
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

		logger->info(__FILEREF__ + "API shutdown");

		// libxml
		{
			// Shutdown libxml
			xmlCleanupParser();

			// this is to debug memory for regression tests
			xmlMemoryDump();
		}
	}
    catch(sql::SQLException& se)
    {
        cerr << __FILEREF__ + "main failed. SQL exception"
            + ", se.what(): " + se.what()
        ;

        // throw se;
		return 1;
    }
    catch(runtime_error& e)
    {
        cerr << __FILEREF__ + "main failed"
            + ", e.what(): " + e.what()
        ;

        // throw e;
		return 1;
    }
    catch(exception& e)
    {
        cerr << __FILEREF__ + "main failed"
            + ", e.what(): " + e.what()
        ;

        // throw runtime_error(errorMessage);
		return 1;
    }

    return 0;
}

