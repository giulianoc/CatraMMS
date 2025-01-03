
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <csignal>

#include "CurlWrapper.h"
#include "FFMPEGEncoder.h"
#include "FFMPEGEncoderDaemons.h"
#include "JSONUtils.h"
#include "LiveRecorderDaemons.h"
#include "MMSStorage.h"

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

int main(int argc, char **argv)
{
	try
	{
		const char *configurationPathName = getenv("MMS_CONFIGPATHNAME");
		if (configurationPathName == nullptr)
		{
			cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;

			return 1;
		}

		CurlWrapper::globalInitialize();

		json configurationRoot = FastCGIAPI::loadConfigurationFile(configurationPathName);

		string logPathName = JSONUtils::asString(configurationRoot["log"]["encoder"], "pathName", "");
		string logErrorPathName = JSONUtils::asString(configurationRoot["log"]["encoder"], "errorPathName", "");
		string logType = JSONUtils::asString(configurationRoot["log"]["encoder"], "type", "");
		bool stdout = JSONUtils::asBool(configurationRoot["log"]["encoder"], "stdout", false);

		std::vector<spdlog::sink_ptr> sinks;
		{
			string logLevel = JSONUtils::asString(configurationRoot["log"]["api"], "level", "");
			if (logType == "daily")
			{
				int logRotationHour = JSONUtils::asInt(configurationRoot["log"]["encoder"]["daily"], "rotationHour", 1);
				int logRotationMinute = JSONUtils::asInt(configurationRoot["log"]["encoder"]["daily"], "rotationMinute", 1);

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
				int maxFiles = JSONUtils::asInt(configurationRoot["log"]["encoder"]["rotating"], "maxFiles", 10);

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

				auto errorRotatingSink =
					make_shared<spdlog::sinks::rotating_file_sink_mt>(logErrorPathName.c_str(), maxSizeInKBytes * 1000, maxFiles);
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

		auto logger = std::make_shared<spdlog::logger>("Encoder", begin(sinks), end(sinks));
		spdlog::register_logger(logger);

		// shared_ptr<spdlog::logger> logger = spdlog::stdout_logger_mt("API");
		// shared_ptr<spdlog::logger> logger = spdlog::daily_logger_mt("API", logPathName.c_str(), 11, 20);

		// trigger flush if the log severity is error or higher
		logger->flush_on(spdlog::level::trace);

		spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off
		string pattern = JSONUtils::asString(configurationRoot["log"]["encoder"], "pattern", "");
		spdlog::set_pattern(pattern);

		// globally register the loggers so so the can be accessed using spdlog::get(logger_name)
		// spdlog::register_logger(logger);

		spdlog::set_default_logger(logger);

		// install a signal handler
		signal(SIGSEGV, signalHandler);
		signal(SIGINT, signalHandler);
		signal(SIGABRT, signalHandler);
		// signal(SIGBUS, signalHandler);

		// MMSStorage::createDirectories(configurationRoot, logger);

		FCGX_Init();

		int threadsNumber = JSONUtils::asInt(configurationRoot["ffmpeg"], "encoderThreadsNumber", 1);
		logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderThreadsNumber: " + to_string(threadsNumber));

		mutex fcgiAcceptMutex;

		mutex cpuUsageMutex;
		deque<int> cpuUsage;
		int numberOfLastCPUUsageToBeChecked = 3;
		for (int cpuUsageIndex = 0; cpuUsageIndex < numberOfLastCPUUsageToBeChecked; cpuUsageIndex++)
		{
			cpuUsage.push_front(0);
		}

		// 2021-09-24: chrono is already thread safe.
		// mutex lastEncodingAcceptedTimeMutex;
		chrono::system_clock::time_point lastEncodingAcceptedTime = chrono::system_clock::now();

		// here is allocated all it is shared among FFMPEGEncoder threads
		mutex encodingMutex;
		vector<shared_ptr<FFMPEGEncoderBase::Encoding>> encodingsCapability;

		mutex liveProxyMutex;
		vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> liveProxiesCapability;

		mutex liveRecordingMutex;
		vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> liveRecordingsCapability;

		mutex encodingCompletedMutex;
		map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> encodingCompletedMap;
		chrono::system_clock::time_point lastEncodingCompletedCheck;

		{
			// int maxEncodingsCapability =  JSONUtils::asInt(
			// 	encoderCapabilityConfiguration["ffmpeg"], "maxEncodingsCapability", 1);
			// logger->info(__FILEREF__ + "Configuration item"
			// 	+ ", ffmpeg->maxEncodingsCapability: " + to_string(maxEncodingsCapability)
			// );

			for (int encodingIndex = 0; encodingIndex < VECTOR_MAX_CAPACITY; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = make_shared<FFMPEGEncoderBase::Encoding>();
				encoding->_available = true;
				encoding->_childPid = 0; // not running
				encoding->_ffmpeg = make_shared<FFMpeg>(configurationRoot, logger);

				encodingsCapability.push_back(encoding);
			}

			// int maxLiveProxiesCapability =  JSONUtils::asInt(encoderCapabilityConfiguration["ffmpeg"],
			// 		"maxLiveProxiesCapability", 10);
			// logger->info(__FILEREF__ + "Configuration item"
			// 	+ ", ffmpeg->maxLiveProxiesCapability: " + to_string(maxLiveProxiesCapability)
			// );

			for (int liveProxyIndex = 0; liveProxyIndex < VECTOR_MAX_CAPACITY; liveProxyIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy = make_shared<FFMPEGEncoderBase::LiveProxyAndGrid>();
				liveProxy->_available = true;
				liveProxy->_childPid = 0; // not running
				liveProxy->_ingestionJobKey = 0;
				liveProxy->_ffmpeg = make_shared<FFMpeg>(configurationRoot, logger);

				liveProxiesCapability.push_back(liveProxy);
			}

			// int maxLiveRecordingsCapability =  JSONUtils::asInt(encoderCapabilityConfiguration["ffmpeg"],
			// 		"maxLiveRecordingsCapability", 10);
			// logger->info(__FILEREF__ + "Configuration item"
			// 	+ ", ffmpeg->maxLiveRecordingsCapability: " + to_string(maxLiveRecordingsCapability)
			// );

			for (int liveRecordingIndex = 0; liveRecordingIndex < VECTOR_MAX_CAPACITY; liveRecordingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording = make_shared<FFMPEGEncoderBase::LiveRecording>();
				liveRecording->_available = true;
				liveRecording->_childPid = 0; // not running
				liveRecording->_ingestionJobKey = 0;
				liveRecording->_encodingParametersRoot = nullptr;
				liveRecording->_ffmpeg = make_shared<FFMpeg>(configurationRoot, logger);

				liveRecordingsCapability.push_back(liveRecording);
			}
		}

		mutex tvChannelsPortsMutex;
		long tvChannelPort_CurrentOffset = 0;

		vector<shared_ptr<FFMPEGEncoder>> ffmpegEncoders;
		vector<thread> ffmpegEncoderThreads;

		for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
		{
			shared_ptr<FFMPEGEncoder> ffmpegEncoder = make_shared<FFMPEGEncoder>(
				configurationRoot,
				// encoderCapabilityConfigurationPathName,

				&fcgiAcceptMutex,

				&cpuUsageMutex, &cpuUsage,

				// &lastEncodingAcceptedTimeMutex,
				&lastEncodingAcceptedTime,

				&encodingMutex, &encodingsCapability,

				&liveProxyMutex, &liveProxiesCapability,

				&liveRecordingMutex, &liveRecordingsCapability,

				&encodingCompletedMutex, &encodingCompletedMap, &lastEncodingCompletedCheck,

				&tvChannelsPortsMutex, &tvChannelPort_CurrentOffset,

				logger
			);

			ffmpegEncoders.push_back(ffmpegEncoder);
			ffmpegEncoderThreads.push_back(thread(&FFMPEGEncoder::operator(), ffmpegEncoder));
		}

		// shutdown should be managed in some way:
		// - mod_fcgid send just one shutdown, so only one thread will go down
		// - mod_fastcgi ???
		// if (threadsNumber > 0)
		{
			// thread liveRecorderChunksIngestion(&FFMPEGEncoder::liveRecorderChunksIngestionThread,
			// 		ffmpegEncoders[0]);
			// thread liveRecorderVirtualVODIngestion(&FFMPEGEncoder::liveRecorderVirtualVODIngestionThread,
			// 		ffmpegEncoders[0]);
			shared_ptr<LiveRecorderDaemons> liveRecorderDaemons =
				make_shared<LiveRecorderDaemons>(configurationRoot, &liveRecordingMutex, &liveRecordingsCapability, logger);

			thread liveRecorderChunksIngestion(&LiveRecorderDaemons::startChunksIngestionThread, liveRecorderDaemons);
			thread liveRecorderVirtualVODIngestion(&LiveRecorderDaemons::startVirtualVODIngestionThread, liveRecorderDaemons);

			// thread monitor(&FFMPEGEncoder::monitorThread, ffmpegEncoders[0]);
			// thread cpuUsage(&FFMPEGEncoder::cpuUsageThread, ffmpegEncoders[0]);
			shared_ptr<FFMPEGEncoderDaemons> ffmpegEncoderDaemons = make_shared<FFMPEGEncoderDaemons>(
				configurationRoot, &liveRecordingMutex, &liveRecordingsCapability, &liveProxyMutex, &liveProxiesCapability, &cpuUsageMutex, &cpuUsage,
				logger
			);

			thread monitor(&FFMPEGEncoderDaemons::startMonitorThread, ffmpegEncoderDaemons);
			thread cpuUsage(&FFMPEGEncoderDaemons::startCPUUsageThread, ffmpegEncoderDaemons);

			ffmpegEncoderThreads[0].join();

			// ffmpegEncoders[0]->stopLiveRecorderVirtualVODIngestionThread();
			// ffmpegEncoders[0]->stopLiveRecorderChunksIngestionThread();
			liveRecorderDaemons->stopVirtualVODIngestionThread();
			liveRecorderDaemons->stopChunksIngestionThread();

			// ffmpegEncoders[0]->stopMonitorThread();
			// ffmpegEncoders[0]->stopCPUUsageThread();
			ffmpegEncoderDaemons->stopMonitorThread();
			ffmpegEncoderDaemons->stopCPUUsageThread();
		}

		CurlWrapper::globalTerminate();

		logger->info(__FILEREF__ + "FFMPEGEncoder shutdown");
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
