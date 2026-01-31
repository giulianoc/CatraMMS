
#include "BandwidthUsageThread.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <csignal>
#include <iostream>

#include "CurlWrapper.h"
#include "FFMPEGEncoder.h"
#include "FFMPEGEncoderDaemons.h"
#include "JSONUtils.h"
#include "LiveRecorderDaemons.h"
#include "MMSStorage.h"
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
	const string logErrorPathName = JSONUtils::as<string>(configurationRoot["log"]["encoder"], "errorPathName", "");
	string logType = JSONUtils::as<string>(configurationRoot["log"]["encoder"], "type", "");

	std::shared_ptr<spdlog::sinks::sink> errorSink;
	if (logType == "daily")
	{
		int logRotationHour = JSONUtils::as<int32_t>(configurationRoot["log"]["encoder"]["daily"], "rotationHour", 1);
		int logRotationMinute = JSONUtils::as<int32_t>(configurationRoot["log"]["encoder"]["daily"], "rotationMinute", 1);

		errorSink = make_shared<spdlog::sinks::daily_file_sink_mt>(logErrorPathName.c_str(), logRotationHour, logRotationMinute);
	}
	else if (logType == "rotating")
	{
		int64_t maxSizeInKBytes = JSONUtils::as<int64_t>(configurationRoot["log"]["encoder"]["rotating"], "maxSizeInKBytes", 1000);
		int maxFiles = JSONUtils::as<int32_t>(configurationRoot["log"]["encoder"]["rotating"], "maxFiles", 10);

		errorSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logErrorPathName.c_str(), maxSizeInKBytes * 1000, maxFiles);
	}
	errorSink->set_level(spdlog::level::err);

	return errorSink;
}

shared_ptr<spdlog::logger> setMainLogger(json configurationRoot, const std::shared_ptr<spdlog::sinks::sink>& errorSink)
{
	string logPathName = JSONUtils::as<string>(configurationRoot["log"]["encoder"], "pathName", "");
	string logType = JSONUtils::as<string>(configurationRoot["log"]["encoder"], "type", "");
	bool stdout = JSONUtils::as<bool>(configurationRoot["log"]["encoder"], "stdout", false);

	std::vector<spdlog::sink_ptr> sinks;
	{
		string logLevel = JSONUtils::as<string>(configurationRoot["log"]["encoder"], "level", "");
		if (logType == "daily")
		{
			int logRotationHour = JSONUtils::as<int32_t>(configurationRoot["log"]["encoder"]["daily"], "rotationHour", 1);
			int logRotationMinute = JSONUtils::as<int32_t>(configurationRoot["log"]["encoder"]["daily"], "rotationMinute", 1);

			auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt>(logPathName.c_str(), logRotationHour, logRotationMinute);
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
			int64_t maxSizeInKBytes = JSONUtils::as<int64_t>(configurationRoot["log"]["encoder"]["rotating"], "maxSizeInKBytes", 1000);
			int maxFiles = JSONUtils::as<int32_t>(configurationRoot["log"]["encoder"]["rotating"], "maxFiles", 10);

			auto rotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logPathName.c_str(), maxSizeInKBytes * 1000, maxFiles);
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
		sinks.push_back(errorSink);

		if (stdout)
		{
			auto stdoutSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sinks.push_back(stdoutSink);
			stdoutSink->set_level(spdlog::level::debug);
		}
	}

	auto logger = std::make_shared<spdlog::logger>("encoder-log", begin(sinks), end(sinks));
	spdlog::register_logger(logger);

	// shared_ptr<spdlog::logger> logger = spdlog::stdout_logger_mt("API");
	// shared_ptr<spdlog::logger> logger = spdlog::daily_logger_mt("API", logPathName.c_str(), 11, 20);

	// trigger flush if the log severity is error or higher
	logger->flush_on(spdlog::level::trace);

	// livello di log sul logger
	logger->set_level(spdlog::level::trace); // trace, debug, info, warn, err, critical, off
	string pattern = JSONUtils::as<string>(configurationRoot["log"]["encoder"], "pattern", "");
	spdlog::set_pattern(pattern);

	// globally register the loggers so so the can be accessed using spdlog::get(logger_name)
	// spdlog::register_logger(logger);

	spdlog::set_default_logger(logger);
	// livello di log globale
	spdlog::set_level(spdlog::level::trace);

	return logger;
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
		const char *configurationPathName = getenv("MMS_CONFIGPATHNAME");
		if (configurationPathName == nullptr)
		{
			cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;

			return 1;
		}

		CurlWrapper::globalInitialize();

		auto configurationRoot = JSONUtils::loadConfigurationFile<json>(configurationPathName, "MMS_");

		std::shared_ptr<spdlog::sinks::sink> errorSink = buildErrorSink(configurationRoot);
		shared_ptr<spdlog::logger> logger = setMainLogger(configurationRoot, errorSink);
		registerNewLogger(JsonPath(&configurationRoot)["log"]["encoder"].as<json>(), "monitor", errorSink);
		registerNewLogger(JsonPath(&configurationRoot)["log"]["encoder"].as<json>(), "stats", errorSink);

		// install a signal handler
		signal(SIGSEGV, signalHandler);
		signal(SIGINT, signalHandler);
		signal(SIGABRT, signalHandler);
		// signal(SIGBUS, signalHandler);

		// MMSStorage::createDirectories(configurationRoot, logger);

		FCGX_Init();

		int threadsNumber = JSONUtils::as<int32_t>(configurationRoot["ffmpeg"], "encoderThreadsNumber", 1);
		LOG_INFO(
			"Configuration item"
			", ffmpeg->encoderThreadsNumber: {}",
			threadsNumber
		);

		mutex fcgiAcceptMutex;

		// 2021-09-24: chrono is already thread safe.
		// mutex lastEncodingAcceptedTimeMutex;
		// chrono::system_clock::time_point lastEncodingAcceptedTime = chrono::system_clock::now();

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
			// int maxEncodingsCapability =  JSONUtils::as<int32_t>(
			// 	encoderCapabilityConfiguration["ffmpeg"], "maxEncodingsCapability", 1);
			// info(__FILEREF__ + "Configuration item"
			// 	+ ", ffmpeg->maxEncodingsCapability: " + to_string(maxEncodingsCapability)
			// );

			for (int encodingIndex = 0; encodingIndex < VECTOR_MAX_CAPACITY; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = make_shared<FFMPEGEncoderBase::Encoding>();
				encoding->_available = true;
				encoding->_childProcessId.reset(); // not running
				encoding->_ffmpeg = make_shared<FFMpegWrapper>(configurationRoot);
				encoding->_callbackData = make_shared<FFMpegEngine::CallbackData>();

				encodingsCapability.push_back(encoding);
			}

			// int maxLiveProxiesCapability =  JSONUtils::as<int32_t>(encoderCapabilityConfiguration["ffmpeg"],
			// 		"maxLiveProxiesCapability", 10);
			// info(__FILEREF__ + "Configuration item"
			// 	+ ", ffmpeg->maxLiveProxiesCapability: " + to_string(maxLiveProxiesCapability)
			// );

			for (int liveProxyIndex = 0; liveProxyIndex < VECTOR_MAX_CAPACITY; liveProxyIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy = make_shared<FFMPEGEncoderBase::LiveProxyAndGrid>();
				liveProxy->_available = true;
				liveProxy->_childProcessId.reset(); // not running
				liveProxy->_ingestionJobKey = 0;
				liveProxy->_ffmpeg = make_shared<FFMpegWrapper>(configurationRoot);
				liveProxy->_callbackData = make_shared<FFMpegEngine::CallbackData>();

				liveProxiesCapability.push_back(liveProxy);
			}

			// int maxLiveRecordingsCapability =  JSONUtils::as<int32_t>(encoderCapabilityConfiguration["ffmpeg"],
			// 		"maxLiveRecordingsCapability", 10);
			// info(__FILEREF__ + "Configuration item"
			// 	+ ", ffmpeg->maxLiveRecordingsCapability: " + to_string(maxLiveRecordingsCapability)
			// );

			for (int liveRecordingIndex = 0; liveRecordingIndex < VECTOR_MAX_CAPACITY; liveRecordingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording = make_shared<FFMPEGEncoderBase::LiveRecording>();
				liveRecording->_available = true;
				liveRecording->_childProcessId.reset(); // not running
				liveRecording->_ingestionJobKey = 0;
				liveRecording->_encodingParametersRoot = nullptr;
				liveRecording->_ffmpeg = make_shared<FFMpegWrapper>(configurationRoot);
				liveRecording->_callbackData = make_shared<FFMpegEngine::CallbackData>();

				liveRecordingsCapability.push_back(liveRecording);
			}
		}

		mutex tvChannelsPortsMutex;
		long tvChannelPort_CurrentOffset = 0;

		auto bandwidthUsageInterfaceNameToMonitor = JsonPath(&configurationRoot)["ffmpeg"]["bandwithUsageInterfaceName"].as<string>();
		std::optional<std::string> optInterfaceNameToMonitor = nullopt;
		if (!bandwidthUsageInterfaceNameToMonitor.empty() && !bandwidthUsageInterfaceNameToMonitor.starts_with("${"))
			optInterfaceNameToMonitor = bandwidthUsageInterfaceNameToMonitor;
		auto bandwidthUsageThread = make_shared<EncoderBandwidthUsageThread>(configurationRoot, optInterfaceNameToMonitor);
		bandwidthUsageThread->start();

		auto cpuStatsUpdateIntervalInSeconds = JsonPath(&configurationRoot)["scheduler"]["cpuStatsUpdateIntervalInSeconds"].
			as<int16_t>(10);
		auto cpuUsageThread = make_shared<EncoderCPUUsageThread>(configurationRoot, cpuStatsUpdateIntervalInSeconds);
		cpuUsageThread->start();

		vector<shared_ptr<FFMPEGEncoder>> ffmpegEncoders;
		vector<thread> ffmpegEncoderThreads;

		for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
		{
			auto ffmpegEncoder = make_shared<FFMPEGEncoder>(
				configurationRoot,
				// encoderCapabilityConfigurationPathName,

				&fcgiAcceptMutex,

				&encodingMutex, &encodingsCapability,

				&liveProxyMutex, &liveProxiesCapability,

				&liveRecordingMutex, &liveRecordingsCapability,

				&encodingCompletedMutex, &encodingCompletedMap, &lastEncodingCompletedCheck,

				&tvChannelsPortsMutex, &tvChannelPort_CurrentOffset, cpuUsageThread, bandwidthUsageThread
			);

			ffmpegEncoders.push_back(ffmpegEncoder);
			ffmpegEncoderThreads.emplace_back(&FFMPEGEncoder::operator(), ffmpegEncoder);
		}

		// shutdown should be managed in some way:
		// - mod_fcgid send just one shutdown, so only one thread will go down
		// - mod_fastcgi ???
		{
			shared_ptr<LiveRecorderDaemons> liveRecorderDaemons =
				make_shared<LiveRecorderDaemons>(configurationRoot, &liveRecordingMutex, &liveRecordingsCapability);

			thread liveRecorderChunksIngestion(&LiveRecorderDaemons::startChunksIngestionThread, liveRecorderDaemons);
			thread liveRecorderVirtualVODIngestion(&LiveRecorderDaemons::startVirtualVODIngestionThread, liveRecorderDaemons);

			// thread monitor(&FFMPEGEncoder::monitorThread, ffmpegEncoders[0]);
			shared_ptr<FFMPEGEncoderDaemons> ffmpegEncoderDaemons = make_shared<FFMPEGEncoderDaemons>(
				configurationRoot, &liveRecordingMutex, &liveRecordingsCapability, &liveProxyMutex, &liveProxiesCapability
			);

			thread monitor(&FFMPEGEncoderDaemons::startMonitorThread, ffmpegEncoderDaemons);

			ffmpegEncoderThreads[0].join();

			// ffmpegEncoders[0]->stopLiveRecorderVirtualVODIngestionThread();
			// ffmpegEncoders[0]->stopLiveRecorderChunksIngestionThread();
			liveRecorderDaemons->stopVirtualVODIngestionThread();
			liveRecorderDaemons->stopChunksIngestionThread();

			ffmpegEncoderDaemons->stopMonitorThread();

			cpuUsageThread->stop();
			bandwidthUsageThread->stop();
		}

		CurlWrapper::globalTerminate();

		LOG_INFO("FFMPEGEncoder shutdown");
	}
	catch (exception &e)
	{
		cerr << __FILEREF__ + "main failed" + ", e.what(): " + e.what();

		// throw runtime_error(errorMessage);
		return 1;
	}

	return 0;
}
