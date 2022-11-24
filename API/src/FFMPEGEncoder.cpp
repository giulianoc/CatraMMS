/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   FFMPEGEncoder.cpp
 * Author: giuliano
 * 
 * Created on February 18, 2018, 1:27 AM
 */

#include "JSONUtils.h"
#include "MMSCURL.h"
#include <fstream>
#include <sstream>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/System.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/Convert.h"
#include "catralibraries/FileIO.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/GetCpuUsage.h"
#include "FFMPEGEncoder.h"
#include "MMSStorage.h"

extern char** environ;

int main(int argc, char** argv) 
{
	try
	{
		const char* configurationPathName = getenv("MMS_CONFIGPATHNAME");
		if (configurationPathName == nullptr)
		{
			cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;

			return 1;
		}

		Json::Value configuration = APICommon::loadConfigurationFile(configurationPathName);

		string logPathName =  configuration["log"]["encoder"].get("pathName", "").asString();
		string logType =  configuration["log"]["encoder"].get("type", "").asString();
		bool stdout =  JSONUtils::asBool(configuration["log"]["encoder"], "stdout", false);

		std::vector<spdlog::sink_ptr> sinks;
		{
			if(logType == "daily")
			{
				int logRotationHour = JSONUtils::asInt(configuration["log"]["encoder"]["daily"],
					"rotationHour", 1);
				int logRotationMinute = JSONUtils::asInt(configuration["log"]["encoder"]["daily"],
					"rotationMinute", 1);

				auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logPathName.c_str(),
					logRotationHour, logRotationMinute);
				sinks.push_back(dailySink);
			}
			else if(logType == "rotating")
			{
				int64_t maxSizeInKBytes = JSONUtils::asInt64(configuration["log"]["encoder"]["rotating"],
					"maxSizeInKBytes", 1000);
				int maxFiles = JSONUtils::asInt(configuration["log"]["encoder"]["rotating"],
					"maxFiles", 10);

				auto rotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt>(logPathName.c_str(),
					maxSizeInKBytes * 1000, maxFiles);
				sinks.push_back(rotatingSink);
			}

			if (stdout)
			{
				auto stdoutSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
				sinks.push_back(stdoutSink);
			}
		}

		auto logger = std::make_shared<spdlog::logger>("Encoder", begin(sinks), end(sinks));
		spdlog::register_logger(logger);
    
		// shared_ptr<spdlog::logger> logger = spdlog::stdout_logger_mt("API");
		// shared_ptr<spdlog::logger> logger = spdlog::daily_logger_mt("API", logPathName.c_str(), 11, 20);
    
		// trigger flush if the log severity is error or higher
		logger->flush_on(spdlog::level::trace);
    
		string logLevel =  configuration["log"]["encoder"].get("level", "err").asString();
		if (logLevel == "debug")
			spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off
		else if (logLevel == "info")
			spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off
		else if (logLevel == "err")
			spdlog::set_level(spdlog::level::err); // trace, debug, info, warn, err, critical, off
		string pattern =  configuration["log"]["encoder"].get("pattern", "XXX").asString();
		spdlog::set_pattern(pattern);

		// globally register the loggers so so the can be accessed using spdlog::get(logger_name)
		// spdlog::register_logger(logger);

		/*
		// the log is written in the apache error log (stderr)
		_logger = spdlog::stderr_logger_mt("API");

		// make sure only responses are written to the standard output
		spdlog::set_level(spdlog::level::trace);
    
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [tid %t] %v");
    
		// globally register the loggers so so the can be accessed using spdlog::get(logger_name)
		// spdlog::register_logger(logger);
		*/

		MMSStorage::createDirectories(configuration, logger);

		FCGX_Init();

		int threadsNumber = JSONUtils::asInt(configuration["ffmpeg"], "encoderThreadsNumber", 1);
		logger->info(__FILEREF__ + "Configuration item"
			+ ", ffmpeg->encoderThreadsNumber: " + to_string(threadsNumber)
		);

		mutex fcgiAcceptMutex;

		mutex cpuUsageMutex;
		deque<int> cpuUsage;
		int numberOfLastCPUUsageToBeChecked = 3;
		for (int cpuUsageIndex = 0; cpuUsageIndex < numberOfLastCPUUsageToBeChecked;
			cpuUsageIndex++)
		{
			cpuUsage.push_front(0);
		}

		// 2021-09-24: chrono is already thread safe.
		// mutex lastEncodingAcceptedTimeMutex;
		chrono::system_clock::time_point    lastEncodingAcceptedTime = chrono::system_clock::now();

		// here is allocated all it is shared among FFMPEGEncoder threads
		mutex encodingMutex;
		#ifdef __VECTOR__
			vector<shared_ptr<Encoding>> encodingsCapability;
		#else	// __MAP__
			map<int64_t, shared_ptr<Encoding>> encodingsCapability;
		#endif

		mutex liveProxyMutex;
		#ifdef __VECTOR__
			vector<shared_ptr<LiveProxyAndGrid>> liveProxiesCapability;
		#else	// __MAP__
			map<int64_t, shared_ptr<LiveProxyAndGrid>> liveProxiesCapability;
		#endif

		mutex liveRecordingMutex;
		#ifdef __VECTOR__
			vector<shared_ptr<LiveRecording>> liveRecordingsCapability;
		#else	// __MAP__
			map<int64_t, shared_ptr<LiveRecording>> liveRecordingsCapability;
		#endif

		mutex encodingCompletedMutex;
		map<int64_t, shared_ptr<EncodingCompleted>> encodingCompletedMap;
		chrono::system_clock::time_point lastEncodingCompletedCheck;

		#ifdef __VECTOR__
		{
			// int maxEncodingsCapability =  JSONUtils::asInt(
			// 	encoderCapabilityConfiguration["ffmpeg"], "maxEncodingsCapability", 1);
			// logger->info(__FILEREF__ + "Configuration item"
			// 	+ ", ffmpeg->maxEncodingsCapability: " + to_string(maxEncodingsCapability)
			// );

			for (int encodingIndex = 0; encodingIndex < VECTOR_MAX_CAPACITY; encodingIndex++)
			{
				shared_ptr<Encoding>    encoding = make_shared<Encoding>();
				encoding->_running   = false;
				encoding->_childPid		= 0;
				encoding->_ffmpeg   = make_shared<FFMpeg>(configuration, logger);

				encodingsCapability.push_back(encoding);
			}

			// int maxLiveProxiesCapability =  JSONUtils::asInt(encoderCapabilityConfiguration["ffmpeg"],
			// 		"maxLiveProxiesCapability", 10);
			// logger->info(__FILEREF__ + "Configuration item"
			// 	+ ", ffmpeg->maxLiveProxiesCapability: " + to_string(maxLiveProxiesCapability)
			// );

			for (int liveProxyIndex = 0; liveProxyIndex < VECTOR_MAX_CAPACITY; liveProxyIndex++)
			{
				shared_ptr<LiveProxyAndGrid>    liveProxy = make_shared<LiveProxyAndGrid>();
				liveProxy->_running					= false;
				liveProxy->_ingestionJobKey			= 0;
				liveProxy->_childPid				= 0;
				liveProxy->_ffmpeg   = make_shared<FFMpeg>(configuration, logger);

				liveProxiesCapability.push_back(liveProxy);
			}

			// int maxLiveRecordingsCapability =  JSONUtils::asInt(encoderCapabilityConfiguration["ffmpeg"],
			// 		"maxLiveRecordingsCapability", 10);
			// logger->info(__FILEREF__ + "Configuration item"
			// 	+ ", ffmpeg->maxLiveRecordingsCapability: " + to_string(maxLiveRecordingsCapability)
			// );

			for (int liveRecordingIndex = 0; liveRecordingIndex < VECTOR_MAX_CAPACITY;
				liveRecordingIndex++)
			{
				shared_ptr<LiveRecording>    liveRecording = make_shared<LiveRecording>();
				liveRecording->_running   = false;
				liveRecording->_ingestionJobKey		= 0;
				liveRecording->_encodingParametersRoot = Json::nullValue;
				liveRecording->_childPid		= 0;
				liveRecording->_ffmpeg   = make_shared<FFMpeg>(configuration, logger);

				liveRecordingsCapability.push_back(liveRecording);

			}
		}
		#else	// __MAP__
		#endif

		mutex tvChannelsPortsMutex;
		long tvChannelPort_CurrentOffset = 0;

		vector<shared_ptr<FFMPEGEncoder>> ffmpegEncoders;
		vector<thread> ffmpegEncoderThreads;

		for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
		{
			shared_ptr<FFMPEGEncoder> ffmpegEncoder = make_shared<FFMPEGEncoder>(
				configuration, 
				// encoderCapabilityConfigurationPathName, 

				&fcgiAcceptMutex,

				&cpuUsageMutex,
				&cpuUsage,

				// &lastEncodingAcceptedTimeMutex,
				&lastEncodingAcceptedTime,

				&encodingMutex,
				&encodingsCapability,

				&liveProxyMutex,
				&liveProxiesCapability,

				&liveRecordingMutex,
				&liveRecordingsCapability,

				&encodingCompletedMutex,
				&encodingCompletedMap,
				&lastEncodingCompletedCheck,

				&tvChannelsPortsMutex,
				&tvChannelPort_CurrentOffset,

				logger
			);

			ffmpegEncoders.push_back(ffmpegEncoder);
			ffmpegEncoderThreads.push_back(thread(&FFMPEGEncoder::operator(), ffmpegEncoder));
		}

		// shutdown should be managed in some way:
		// - mod_fcgid send just one shutdown, so only one thread will go down
		// - mod_fastcgi ???
		if (threadsNumber > 0)
		{
			thread liveRecorderChunksIngestion(&FFMPEGEncoder::liveRecorderChunksIngestionThread,
					ffmpegEncoders[0]);
			thread liveRecorderVirtualVODIngestion(&FFMPEGEncoder::liveRecorderVirtualVODIngestionThread,
					ffmpegEncoders[0]);

			thread monitor(&FFMPEGEncoder::monitorThread, ffmpegEncoders[0]);

			thread cpuUsage(&FFMPEGEncoder::cpuUsageThread, ffmpegEncoders[0]);

			ffmpegEncoderThreads[0].join();
        
			ffmpegEncoders[0]->stopLiveRecorderVirtualVODIngestionThread();
			ffmpegEncoders[0]->stopLiveRecorderChunksIngestionThread();
			ffmpegEncoders[0]->stopMonitorThread();
			ffmpegEncoders[0]->stopCPUUsageThread();
		}

		logger->info(__FILEREF__ + "FFMPEGEncoder shutdown");
	}
    catch(runtime_error e)
    {
        cerr << __FILEREF__ + "main failed"
            + ", e.what(): " + e.what()
        ;

        // throw e;
		return 1;
    }
    catch(exception e)
    {
        cerr << __FILEREF__ + "main failed"
            + ", e.what(): " + e.what()
        ;

        // throw runtime_error(errorMessage);
		return 1;
    }

    return 0;
}

FFMPEGEncoder::FFMPEGEncoder(

		Json::Value configuration, 
		// string encoderCapabilityConfigurationPathName,

        mutex* fcgiAcceptMutex,

        mutex* cpuUsageMutex,
		deque<int>* cpuUsage,

        // mutex* lastEncodingAcceptedTimeMutex,
		chrono::system_clock::time_point* lastEncodingAcceptedTime,

		mutex* encodingMutex,
		#ifdef __VECTOR__
			vector<shared_ptr<Encoding>>* encodingsCapability,
		#else	// __MAP__
			map<int64_t, shared_ptr<Encoding>>* encodingsCapability,
		#endif

		mutex* liveProxyMutex,
		#ifdef __VECTOR__
			vector<shared_ptr<LiveProxyAndGrid>>* liveProxiesCapability,
		#else	// __MAP__
			map<int64_t, shared_ptr<LiveProxyAndGrid>>* liveProxiesCapability,
		#endif

		mutex* liveRecordingMutex,
		#ifdef __VECTOR__
			vector<shared_ptr<LiveRecording>>* liveRecordingsCapability,
		#else	// __MAP__
			map<int64_t, shared_ptr<LiveRecording>>* liveRecordingsCapability,
		#endif

		mutex* encodingCompletedMutex,
		map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,
		chrono::system_clock::time_point* lastEncodingCompletedCheck,

		mutex* tvChannelsPortsMutex,
		long* tvChannelPort_CurrentOffset,

        shared_ptr<spdlog::logger> logger)
    : APICommon(configuration, 
		fcgiAcceptMutex,
		logger) 
{
	// _encoderCapabilityConfigurationPathName = encoderCapabilityConfigurationPathName;

    _monitorCheckInSeconds =  JSONUtils::asInt(_configuration["ffmpeg"], "monitorCheckInSeconds", 5);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->monitorCheckInSeconds: " + to_string(_monitorCheckInSeconds)
    );

    _liveRecorderChunksIngestionCheckInSeconds =  JSONUtils::asInt(_configuration["ffmpeg"], "liveRecorderChunksIngestionCheckInSeconds", 5);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveRecorderChunksIngestionCheckInSeconds: " + to_string(_liveRecorderChunksIngestionCheckInSeconds)
    );

    _liveRecorderVirtualVODIngestionInSeconds = JSONUtils::asInt(_configuration["ffmpeg"], "liveRecorderVirtualVODIngestionInSeconds", 5);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveRecorderVirtualVODIngestionInSeconds: " + to_string(_liveRecorderVirtualVODIngestionInSeconds)
    );
    _liveRecorderVirtualVODRetention = _configuration["ffmpeg"].get("liveRecorderVirtualVODRetention", "15m").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveRecorderVirtualVODRetention: " + _liveRecorderVirtualVODRetention
    );

	_tvChannelConfigurationDirectory = _configuration["ffmpeg"].
		get("tvChannelConfigurationDirectory", "").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->tvChannelConfigurationDirectory: " + _tvChannelConfigurationDirectory
	);

    _encodingCompletedRetentionInSeconds = JSONUtils::asInt(_configuration["ffmpeg"], "encodingCompletedRetentionInSeconds", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encodingCompletedRetentionInSeconds: " + to_string(_encodingCompletedRetentionInSeconds)
    );

    _mmsAPITimeoutInSeconds = JSONUtils::asInt(_configuration["api"], "timeoutInSeconds", 120);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->timeoutInSeconds: " + to_string(_mmsAPITimeoutInSeconds)
    );

    _mmsBinaryTimeoutInSeconds = JSONUtils::asInt(_configuration["api"]["binary"], "timeoutInSeconds", 120);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->timeoutInSeconds: " + to_string(_mmsBinaryTimeoutInSeconds)
    );

	_cpuUsageThresholdForEncoding =  JSONUtils::asInt(_configuration["ffmpeg"],
		"cpuUsageThresholdForEncoding", 50);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->cpuUsageThresholdForEncoding: " + to_string(_cpuUsageThresholdForEncoding)
	);
	_cpuUsageThresholdForRecording =  JSONUtils::asInt(_configuration["ffmpeg"],
		"cpuUsageThresholdForRecording", 60);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->cpuUsageThresholdForRecording: " + to_string(_cpuUsageThresholdForRecording)
	);
	_cpuUsageThresholdForProxy =  JSONUtils::asInt(_configuration["ffmpeg"],
		"cpuUsageThresholdForProxy", 70);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->cpuUsageThresholdForProxy: " + to_string(_cpuUsageThresholdForProxy)
	);
    _intervalInSecondsBetweenEncodingAccept = JSONUtils::asInt(_configuration["ffmpeg"], "intervalInSecondsBetweenEncodingAccept", 5);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->intervalInSecondsBetweenEncodingAccept: " + to_string(_intervalInSecondsBetweenEncodingAccept)
    );


	_cpuUsageMutex = cpuUsageMutex;
	_cpuUsage = cpuUsage;

	// _lastEncodingAcceptedTimeMutex = lastEncodingAcceptedTimeMutex;
	_lastEncodingAcceptedTime = lastEncodingAcceptedTime;

	_encodingMutex = encodingMutex;
	_encodingsCapability = encodingsCapability;
	// _maxEncodingsCapability =  JSONUtils::asInt(_encoderCapabilityConfiguration["ffmpeg"],
	// 	"maxEncodingsCapability", 1);
	// _logger->info(__FILEREF__ + "Configuration item"
	// 	+ ", ffmpeg->maxEncodingsCapability: " + to_string(_maxEncodingsCapability)
	// );

	_liveProxyMutex = liveProxyMutex;
	_liveProxiesCapability = liveProxiesCapability;
	// _maxLiveProxiesCapability =  JSONUtils::asInt(_encoderCapabilityConfiguration["ffmpeg"],
	// 	"maxLiveProxiesCapability", 10);
	// _logger->info(__FILEREF__ + "Configuration item"
	// 	+ ", ffmpeg->maxLiveProxiesCapability: " + to_string(_maxLiveProxiesCapability)
	// );

	_liveRecordingMutex = liveRecordingMutex;
	_liveRecordingsCapability = liveRecordingsCapability;
	// _maxLiveRecordingsCapability =  JSONUtils::asInt(_encoderCapabilityConfiguration["ffmpeg"],
	// 	"maxLiveRecordingsCapability", 10);
	// logger->info(__FILEREF__ + "Configuration item"
	// 	+ ", ffmpeg->maxLiveRecordingsCapability: " + to_string(_maxLiveRecordingsCapability)
	// );

	_tvChannelPort_Start = 8000;
	_tvChannelPort_MaxNumberOfOffsets = 100;

	_cpuUsageThreadShutdown = false;
	_monitorThreadShutdown = false;
	_liveRecorderChunksIngestionThreadShutdown = false;
	_liveRecorderVirtualVODIngestionThreadShutdown = false;

	_encodingCompletedMutex = encodingCompletedMutex;
	_encodingCompletedMap = encodingCompletedMap;
	_lastEncodingCompletedCheck = lastEncodingCompletedCheck;

	_tvChannelsPortsMutex = tvChannelsPortsMutex;
	_tvChannelPort_CurrentOffset = tvChannelPort_CurrentOffset;

	*_lastEncodingCompletedCheck = chrono::system_clock::now();
}

FFMPEGEncoder::~FFMPEGEncoder() {
}

// 2020-06-11: FFMPEGEncoder is just one thread, so make sure manageRequestAndResponse is very fast because
//	the time used by manageRequestAndResponse is time FFMPEGEncoder is not listening
//	for new connections (encodingStatus, ...)

void FFMPEGEncoder::manageRequestAndResponse(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        bool basicAuthenticationPresent,
        tuple<int64_t,shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>& userKeyWorkspaceAndFlags,
		string apiKey,
        unsigned long contentLength,
        string requestBody,
        unordered_map<string, string>& requestDetails
)
{
	// chrono::system_clock::time_point startManageRequestAndResponse = chrono::system_clock::now();

    auto methodIt = queryParameters.find("method");
    if (methodIt == queryParameters.end())
    {
        _logger->error(__FILEREF__ + "The 'method' parameter is not found");

		string errorMessage = string("Internal server error");

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    string method = methodIt->second;

    if (method == "status")
    {
        try
        {
			Json::Value responseBodyRoot;
			responseBodyRoot["status"] = "Encoder up and running";

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "status failed"
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "info")
    {
        try
        {
			int lastBiggerCpuUsage = -1;
			{
				lock_guard<mutex> locker(*_cpuUsageMutex);

				for(int cpuUsage: *_cpuUsage)
				{
					if (cpuUsage > lastBiggerCpuUsage)
						lastBiggerCpuUsage = cpuUsage;
				}
			}

			Json::Value infoRoot;
			infoRoot["status"] = "Encoder up and running";
			infoRoot["cpuUsage"] = lastBiggerCpuUsage;

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, infoRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "status failed"
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "encodeContent")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
			_logger->error(__FILEREF__ + "The 'encodingJobKey' parameter is not found");

			string errorMessage = string("Internal server error");

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			shared_ptr<Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (!encoding->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxEncodingsCapability = getMaxEncodingsCapability();
			if (_encodingsCapability->size() >= maxEncodingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{
				// lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<Encoding> selectedEncoding = make_shared<Encoding>();
				selectedEncoding->_running		 = false;
				selectedEncoding->_childPid		= 0;
				selectedEncoding->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				selectedEncoding->_running = true;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating encodeContent thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread encodeContentThread(&FFMPEGEncoder::encodeContentThread, this,
					selectedEncoding, encodingJobKey, requestBody);
				encodeContentThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_encodingsCapability->insert(make_pair(selectedEncoding->_encodingJobKey, selectedEncoding));
				_logger->info(__FILEREF__ + "_encodingsCapability->insert (encodeContent)"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(selectedEncoding->_encodingJobKey)
				);
				#endif

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedEncoding->_running = false;
				selectedEncoding->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "encodeContentThread failed"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "encodeContentThread failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "cutFrameAccurate")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			shared_ptr<Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (!encoding->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxEncodingsCapability = getMaxEncodingsCapability();
			if (_encodingsCapability->size() >= maxEncodingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{
				// lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<Encoding> selectedEncoding = make_shared<Encoding>();
				selectedEncoding->_running		 = false;
				selectedEncoding->_childPid		= 0;
				selectedEncoding->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				selectedEncoding->_running = true;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating cut thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread cutFrameAccurateThread(&FFMPEGEncoder::cutFrameAccurateThread,
					this, selectedEncoding, encodingJobKey, requestBody);
				cutFrameAccurateThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_encodingsCapability->insert(
					make_pair(selectedEncoding->_encodingJobKey, selectedEncoding));
				_logger->info(__FILEREF__ + "_encodingsCapability->insert (cut)"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(selectedEncoding->_encodingJobKey)
				);
				#endif

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedEncoding->_running = false;
				selectedEncoding->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "cutFrameAccurateThread failed"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {            
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "cutFrameAccurateThread failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "overlayImageOnVideo")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			shared_ptr<Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (!encoding->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxEncodingsCapability = getMaxEncodingsCapability();
			if (_encodingsCapability->size() >= maxEncodingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{
				// lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<Encoding> selectedEncoding = make_shared<Encoding>();
				selectedEncoding->_running		 = false;
				selectedEncoding->_childPid		= 0;
				selectedEncoding->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				selectedEncoding->_running = true;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating overlayImageOnVideo thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread overlayImageOnVideoThread(&FFMPEGEncoder::overlayImageOnVideoThread,
					this, selectedEncoding, encodingJobKey, requestBody);
				overlayImageOnVideoThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_encodingsCapability->insert(make_pair(selectedEncoding->_encodingJobKey, selectedEncoding));
				_logger->info(__FILEREF__ + "_encodingsCapability->insert (overlayImageOnVideo)"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(selectedEncoding->_encodingJobKey)
				);
				#endif

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedEncoding->_running = false;
				selectedEncoding->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "overlayImageOnVideoThread failed"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "overlayImageOnVideoThread failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "overlayTextOnVideo")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			shared_ptr<Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (!encoding->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxEncodingsCapability = getMaxEncodingsCapability();
			if (_encodingsCapability->size() >= maxEncodingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{            
				// lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<Encoding> selectedEncoding = make_shared<Encoding>();
				selectedEncoding->_running		 = false;
				selectedEncoding->_childPid		= 0;
				selectedEncoding->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				selectedEncoding->_running = true;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating overlayTextOnVideo thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread overlayTextOnVideoThread(&FFMPEGEncoder::overlayTextOnVideoThread,
					this, selectedEncoding, encodingJobKey, requestBody);
				overlayTextOnVideoThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_encodingsCapability->insert(make_pair(selectedEncoding->_encodingJobKey, selectedEncoding));
				_logger->info(__FILEREF__ + "_encodingsCapability->insert (overlayTextOnVideo)"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(selectedEncoding->_encodingJobKey)
				);
				#endif

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedEncoding->_running = false;
				selectedEncoding->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "overlayTextOnVideoThread failed"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {            
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "overlayTextOnVideoThread failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "generateFrames")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			shared_ptr<Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (!encoding->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxEncodingsCapability = getMaxEncodingsCapability();
			if (_encodingsCapability->size() >= maxEncodingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{            
				// lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<Encoding> selectedEncoding = make_shared<Encoding>();
				selectedEncoding->_running		 = false;
				selectedEncoding->_childPid		= 0;
				selectedEncoding->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				selectedEncoding->_running = true;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating generateFrames thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread generateFramesThread(&FFMPEGEncoder::generateFramesThread,
					this, selectedEncoding, encodingJobKey, requestBody);
				generateFramesThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_encodingsCapability->insert(make_pair(selectedEncoding->_encodingJobKey, selectedEncoding));
				_logger->info(__FILEREF__ + "_encodingsCapability->insert (generateFrames)"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(selectedEncoding->_encodingJobKey)
				);
				#endif

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedEncoding->_running = false;
				selectedEncoding->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "generateFrames failed"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {            
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "generateFramesThread failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "slideShow")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			shared_ptr<Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (!encoding->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxEncodingsCapability = getMaxEncodingsCapability();
			if (_encodingsCapability->size() >= maxEncodingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{            
				// lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<Encoding> selectedEncoding = make_shared<Encoding>();
				selectedEncoding->_running		 = false;
				selectedEncoding->_childPid		= 0;
				selectedEncoding->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				selectedEncoding->_running = true;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating slideShow thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread slideShowThread(&FFMPEGEncoder::slideShowThread,
					this, selectedEncoding, encodingJobKey, requestBody);
				slideShowThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_encodingsCapability->insert(make_pair(selectedEncoding->_encodingJobKey, selectedEncoding));
				_logger->info(__FILEREF__ + "_encodingsCapability->insert (slideShow)"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(selectedEncoding->_encodingJobKey)
				);
				#endif

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedEncoding->_running = false;
				selectedEncoding->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "slideShow failed"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {            
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "slideShowThread failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "videoSpeed")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			shared_ptr<Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (!encoding->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxEncodingsCapability = getMaxEncodingsCapability();
			if (_encodingsCapability->size() >= maxEncodingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{            
				// lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<Encoding> selectedEncoding = make_shared<Encoding>();
				selectedEncoding->_running		 = false;
				selectedEncoding->_childPid		= 0;
				selectedEncoding->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				selectedEncoding->_running = true;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating videoSpeed thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread videoSpeedThread(&FFMPEGEncoder::videoSpeedThread,
					this, selectedEncoding, encodingJobKey, requestBody);
				videoSpeedThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_encodingsCapability->insert(make_pair(selectedEncoding->_encodingJobKey, selectedEncoding));
				_logger->info(__FILEREF__ + "_encodingsCapability->insert (videoSpeed)"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(selectedEncoding->_encodingJobKey)
				);
				#endif

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedEncoding->_running = false;
				selectedEncoding->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "videoSpeedThread failed"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {            
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "sendSuccess failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "pictureInPicture")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			shared_ptr<Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (!encoding->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxEncodingsCapability = getMaxEncodingsCapability();
			if (_encodingsCapability->size() >= maxEncodingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{            
				// lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<Encoding> selectedEncoding = make_shared<Encoding>();
				selectedEncoding->_running		 = false;
				selectedEncoding->_childPid		= 0;
				selectedEncoding->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				selectedEncoding->_running = true;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating pictureInPicture thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread pictureInPictureThread(&FFMPEGEncoder::pictureInPictureThread,
					this, selectedEncoding, encodingJobKey, requestBody);
				pictureInPictureThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_encodingsCapability->insert(make_pair(selectedEncoding->_encodingJobKey, selectedEncoding));
				_logger->info(__FILEREF__ + "_encodingsCapability->insert (pictureInPicture)"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(selectedEncoding->_encodingJobKey)
				);
				#endif

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedEncoding->_running = false;
				selectedEncoding->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "pictureInPictureThread failed"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {            
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "sendSuccess failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "introOutroOverlay")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			shared_ptr<Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (!encoding->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxEncodingsCapability = getMaxEncodingsCapability();
			if (_encodingsCapability->size() >= maxEncodingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{            
				// lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<Encoding> selectedEncoding = make_shared<Encoding>();
				selectedEncoding->_running		 = false;
				selectedEncoding->_childPid		= 0;
				selectedEncoding->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				selectedEncoding->_running = true;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating introOutroOverlay thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread introOutroOverlayThread(&FFMPEGEncoder::introOutroOverlayThread,
					this, selectedEncoding, encodingJobKey, requestBody);
				introOutroOverlayThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_encodingsCapability->insert(make_pair(selectedEncoding->_encodingJobKey, selectedEncoding));
				_logger->info(__FILEREF__ + "_encodingsCapability->insert (introOutroOverlay)"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(selectedEncoding->_encodingJobKey)
				);
				#endif

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedEncoding->_running = false;
				selectedEncoding->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "introOutroOverlayThread failed"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {            
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "sendSuccess failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "liveRecorder")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			#ifdef __VECTOR__
			shared_ptr<LiveRecording>    selectedLiveRecording;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
			int maxLiveRecordingsCapability = getMaxLiveRecordingsCapability();
			for(int liveRecordingIndex = 0; liveRecordingIndex < maxLiveRecordingsCapability;
				liveRecordingIndex++)
			{
				shared_ptr<LiveRecording> liveRecording = (*_liveRecordingsCapability)[liveRecordingIndex];

				if (!liveRecording->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedLiveRecording = liveRecording;
					}
				}
				else
				{
					if (liveRecording->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxLiveRecordingsCapability = getMaxLiveRecordingsCapability();
			if (_liveRecordingsCapability->size() >= maxLiveRecordingsCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<LiveRecording>>::iterator it =
				_liveRecordingsCapability->find(encodingJobKey);
			if (it != _liveRecordingsCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{
				/*
				 * 2021-09-15: live-recorder cannot wait. Scenario: received a lot of requests that fail
				 * Those requests set _lastEncodingAcceptedTime and delay a lot
				 * the requests that would work fine
				 * Consider that Live-Recorder is a Task where FFMPEGEncoder
				 * could receive a lot of close requests
				lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}
				*/

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<LiveRecording> selectedLiveRecording = make_shared<LiveRecording>();
				selectedLiveRecording->_running		 = false;
				selectedLiveRecording->_encodingParametersRoot = Json::nullValue;
				selectedLiveRecording->_childPid		= 0;
				selectedLiveRecording->_ffmpeg		= make_shared<FFMpeg>(_configuration, _logger);
				#endif

				// 2022-11-23: ho visto che, in caso di autoRenew, monitoring generates errors and trys to kill
				//		the process. Moreover the selectedLiveRecording->_errorMessage remain initialized
				//		with the error (like killed because segment file is not present).
				//		For this reason, _recordingStart is initialized to make sure monitoring does not perform
				//		his checks before recorder is not really started.
				//		_recordingStart will be initialized correctly into the liveRecorderThread method
				selectedLiveRecording->_recordingStart = chrono::system_clock::now() + chrono::seconds(60);
				selectedLiveRecording->_running = true;
				selectedLiveRecording->_encodingJobKey = encodingJobKey;
				selectedLiveRecording->_childPid = 0;

				_logger->info(__FILEREF__ + "Creating liveRecorder thread"
					+ ", selectedLiveRecording->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread liveRecorderThread(&FFMPEGEncoder::liveRecorderThread,
					this, selectedLiveRecording, encodingJobKey, requestBody);
				liveRecorderThread.detach();

				#ifdef __VECTOR__
				#else	// __MAP__
				_liveRecordingsCapability->insert(make_pair(selectedLiveRecording->_encodingJobKey, selectedLiveRecording));
				_logger->info(__FILEREF__ + "_liveRecordingsCapability->insert"
					+ ", selectedLiveRecording->_encodingJobKey: " + to_string(selectedLiveRecording->_encodingJobKey)
				);
				#endif

				// *_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedLiveRecording->_running = false;
				selectedLiveRecording->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "liveRecorder failed"
					+ ", selectedLiveRecording->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "liveRecorderThread failed"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "liveProxy"
		// || method == "vodProxy"
		|| method == "liveGrid"
		// || method == "countdown"
	)
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			#ifdef __VECTOR__
			shared_ptr<LiveProxyAndGrid>    selectedLiveProxy;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			int maxLiveProxiesCapability = getMaxLiveProxiesCapability();
			for(int liveProxyIndex = 0; liveProxyIndex < maxLiveProxiesCapability; liveProxyIndex++)
			{
				shared_ptr<LiveProxyAndGrid> liveProxy = (*_liveProxiesCapability)[liveProxyIndex];

				if (!liveProxy->_running)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedLiveProxy = liveProxy;
					}
				}
				else
				{
					if (liveProxy->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + NoEncodingAvailable().what();

				_logger->error(__FILEREF__ + errorMessage
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#else	// __MAP__
			int maxLiveProxiesCapability = getMaxLiveProxiesCapability();
			if (_liveProxiesCapability->size() >= maxLiveProxiesCapability)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingAvailable().what();
            
				_logger->warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
        
			map<int64_t, shared_ptr<LiveProxyAndGrid>>::iterator it =
				_liveProxiesCapability->find(encodingJobKey);
			if (it != _liveProxiesCapability->end())
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + EncodingIsAlreadyRunning().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			#endif

			try
			{
				/*
				 * 2021-09-15: liveProxy cannot wait. Scenario: received a lot of requests that fail
				 * Those requests set _lastEncodingAcceptedTime and delay a lot
				 * the requests that would work fine
				 * Consider that Live-Proxy is a Task where FFMPEGEncoder
				 * could receive a lot of close requests
				lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request
				// in order to give time to the cpuUsage variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime <
					chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					int secondsToWait =
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
						chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count();
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: "
							+ to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}
				*/

				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<LiveProxyAndGrid>    selectedLiveProxy = make_shared<LiveProxyAndGrid>();
				selectedLiveProxy->_running   = false;
				selectedLiveProxy->_childPid		= 0;
				selectedLiveProxy->_ffmpeg   = make_shared<FFMpeg>(_configuration, _logger);
				selectedLiveProxy->_ingestionJobKey		= 0;
				#endif

				selectedLiveProxy->_running = true;
				selectedLiveProxy->_encodingJobKey = encodingJobKey;
				selectedLiveProxy->_method = method;
				selectedLiveProxy->_childPid = 0;

				if (method == "liveProxy")
				{
					_logger->info(__FILEREF__ + "Creating liveProxy thread"
						+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
						+ ", requestBody: " + requestBody
					);
					thread liveProxyThread(&FFMPEGEncoder::liveProxyThread,
						this, selectedLiveProxy, encodingJobKey, requestBody);
					liveProxyThread.detach();
				}
				/*
				else if (method == "vodProxy")
				{
					_logger->info(__FILEREF__ + "Creating vodProxy thread"
						+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
						+ ", requestBody: " + requestBody
					);
					thread vodProxyThread(&FFMPEGEncoder::vodProxyThread,
						this, selectedLiveProxy, encodingJobKey, requestBody);
					vodProxyThread.detach();
				}
				*/
				else if (method == "liveGrid")
				{
					_logger->info(__FILEREF__ + "Creating liveGrid thread"
						+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
						+ ", requestBody: " + requestBody
					);
					thread liveGridThread(&FFMPEGEncoder::liveGridThread,
						this, selectedLiveProxy, encodingJobKey, requestBody);
					liveGridThread.detach();
				}
				/*
				else // if (method == "countdown")
				{
					_logger->info(__FILEREF__ + "Creating countdown thread"
						+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
						+ ", requestBody: " + requestBody
					);
					thread awaitingTheBeginningThread(&FFMPEGEncoder::awaitingTheBeginningThread,
						this, selectedLiveProxy, encodingJobKey, requestBody);
					awaitingTheBeginningThread.detach();
				}
				*/

				#ifdef __VECTOR__
				#else	// __MAP__
				_liveProxiesCapability->insert(make_pair(selectedLiveProxy->_encodingJobKey, selectedLiveProxy));
				_logger->info(__FILEREF__ + "_liveProxiesCapability->insert"
					+ ", selectedLiveProxy->_encodingJobKey: " + to_string(selectedLiveProxy->_encodingJobKey)
				);
				#endif

				// *_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				#ifdef __VECTOR__
				selectedLiveProxy->_running = false;
				selectedLiveProxy->_childPid = 0;
				#else	// __MAP__
				#endif

				_logger->error(__FILEREF__ + "liveProxyThread failed"
					+ ", method: " + method
					+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
					+ ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        try
        {            
			Json::Value responseBodyRoot;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "liveProxy/vodProxy/liveGrid/awaitingTheBeginning Thread failed"
                + ", method: " + method
                + ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else if (method == "encodingStatus")
	{
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
        
		chrono::system_clock::time_point startEncodingStatus = chrono::system_clock::now();

		bool                    encodingFound = false;
		shared_ptr<Encoding>    selectedEncoding;

		bool                    liveProxyFound = false;
		shared_ptr<LiveProxyAndGrid>	selectedLiveProxy;

		bool                    liveRecordingFound = false;
		shared_ptr<LiveRecording>    selectedLiveRecording;

		bool                    encodingCompleted = false;
		shared_ptr<EncodingCompleted>    selectedEncodingCompleted;

		int encodingCompletedMutexDuration = -1;
		int encodingMutexDuration = -1;
		int liveProxyMutexDuration = -1;
		int liveRecordingMutexDuration = -1;
		{
			chrono::system_clock::time_point startLockTime = chrono::system_clock::now();
			lock_guard<mutex> locker(*_encodingCompletedMutex);
			chrono::system_clock::time_point endLockTime = chrono::system_clock::now();
			encodingCompletedMutexDuration = chrono::duration_cast<chrono::seconds>(
				endLockTime - startLockTime).count();

			map<int64_t, shared_ptr<EncodingCompleted>>::iterator it =
				_encodingCompletedMap->find(encodingJobKey);
			if (it != _encodingCompletedMap->end())
			{
				encodingCompleted = true;
				selectedEncodingCompleted = it->second;
			}
		}

		if (!encodingCompleted)
		{
			// next \{ is to make the lock free as soon as the check is done
			{
				// see comment 2020-11-30
				#if defined(__VECTOR__) && defined(__VECTOR__NO_LOCK_FOR_ENCODINGSTATUS)
				#else
				chrono::system_clock::time_point startLockTime = chrono::system_clock::now();
				lock_guard<mutex> locker(*_encodingMutex);
				chrono::system_clock::time_point endLockTime = chrono::system_clock::now();
				encodingMutexDuration = chrono::duration_cast<chrono::seconds>(
					endLockTime - startLockTime).count();
				#endif

				#ifdef __VECTOR__
				for (shared_ptr<Encoding> encoding: *_encodingsCapability)
				{
					if (encoding->_encodingJobKey == encodingJobKey)
					{
						encodingFound = true;
						selectedEncoding = encoding;

						break;
					}
				}
				#else	// __MAP__
				map<int64_t, shared_ptr<Encoding>>::iterator it =
					_encodingsCapability->find(encodingJobKey);
				if (it != _encodingsCapability->end())
				{
					encodingFound = true;
					selectedEncoding = it->second;
				}
				#endif
			}

			if (!encodingFound)
			{
				// next \{ is to make the lock free as soon as the check is done
				{
/*
 * 2020-11-30
 * CIBORTV PROJECT. SCENARIO:
 *	- The encodingStatus is called by the mmsEngine periodically for each running transcoding.
 *		Often this method takes a lot of times to answer, depend on the period encodingStatus is called,
 *		50 secs in case it is called every 5 seconds, 35 secs in case it is called every 30 secs.
 *		This because the Lock (lock_guard) does not provide any guarantee, in case there are a lot of threads,
 *		as it is our case, may be a thread takes the lock and the OS switches to another thread. It could
 *		take time the OS re-switch on the previous thread in order to release the lock.
 *
 *	To solve this issue we should found an algorithm that guarantees the Lock is managed
 *	in a fast way also in case of a lot of threads. I do not have now a solution for this.
 *	For this since I thought:
 *	- in case of __VECTOR__ all the structure is "fixes", every thing is allocated at the beggining
 *		and do not change
 *	- so for this method, since it checks some attribute in a "static" structure,
 *		WE MAY AVOID THE USING OF THE LOCK
 *
 */
					// see comment 2020-11-30
					#if defined(__VECTOR__) && defined(__VECTOR__NO_LOCK_FOR_ENCODINGSTATUS)
					#else
					chrono::system_clock::time_point startLockTime = chrono::system_clock::now();
					lock_guard<mutex> locker(*_liveProxyMutex);
					chrono::system_clock::time_point endLockTime = chrono::system_clock::now();
					liveProxyMutexDuration = chrono::duration_cast<chrono::seconds>(
						endLockTime - startLockTime).count();
					#endif

					#ifdef __VECTOR__
					for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
					{
						if (liveProxy->_encodingJobKey == encodingJobKey)
						{
							liveProxyFound = true;
							selectedLiveProxy = liveProxy;

							break;
						}
					}
					#else	// __MAP__
					map<int64_t, shared_ptr<LiveProxyAndGrid>>::iterator it =
						_liveProxiesCapability->find(encodingJobKey);
					if (it != _liveProxiesCapability->end())
					{
						liveProxyFound = true;
						selectedLiveProxy = it->second;
					}
					#endif
				}

				if (!liveProxyFound)
				{
					// see comment 2020-11-30
					#if defined(__VECTOR__) && defined(__VECTOR__NO_LOCK_FOR_ENCODINGSTATUS)
					#else
					chrono::system_clock::time_point startLockTime = chrono::system_clock::now();
					lock_guard<mutex> locker(*_liveRecordingMutex);
					chrono::system_clock::time_point endLockTime = chrono::system_clock::now();
					liveRecordingMutexDuration = chrono::duration_cast<chrono::seconds>(
						endLockTime - startLockTime).count();
					#endif

					#ifdef __VECTOR__
					for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
					{
						if (liveRecording->_encodingJobKey == encodingJobKey)
						{
							liveRecordingFound = true;
							selectedLiveRecording = liveRecording;

							break;
						}
					}
					#else	// __MAP__
					map<int64_t, shared_ptr<LiveRecording>>::iterator it =
						_liveRecordingsCapability->find(encodingJobKey);
					if (it != _liveRecordingsCapability->end())
					{
						liveRecordingFound = true;
						selectedLiveRecording = it->second;
					}
					#endif
				}
			}
		}

		chrono::system_clock::time_point endLookingForEncodingStatus = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "encodingStatus"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", encodingFound: " + to_string(encodingFound)
			+ ", liveProxyFound: " + to_string(liveProxyFound)
			+ ", liveRecordingFound: " + to_string(liveRecordingFound)
			+ ", encodingCompleted: " + to_string(encodingCompleted)
			+ ", encodingCompletedMutexDuration: " + to_string(encodingCompletedMutexDuration)
			+ ", encodingMutexDuration: " + to_string(encodingMutexDuration)
			+ ", liveProxyMutexDuration: " + to_string(liveProxyMutexDuration)
			+ ", liveRecordingMutexDuration: " + to_string(liveRecordingMutexDuration)
			+ ", @MMS statistics@ - duration looking for encodingStatus (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endLookingForEncodingStatus - startEncodingStatus).count()) + "@"
		);

        string responseBody;
        if (!encodingFound && !liveProxyFound && !liveRecordingFound && !encodingCompleted)
        {
			// it should never happen
            responseBody = string("{ ")
                + "\"encodingJobKey\": " + to_string(encodingJobKey)
                + ", \"pid\": 0"
				+ ", \"killedByUser\": false"
                + ", \"encodingFinished\": true "
                + ", \"encodingProgress\": 100 "
                + "}";
        }
        else
        {
            /* in case we see the encoding is finished but encoding->_running is true,
            it means the thread encodingContent is hanged.
             In this case we can implement the method _ffmpeg->stillEncoding
             doing a check may be looking if the encoded file is growing.
             That will allow us to know for sure if the encoding finished and
             reset the Encoding structure
            bool stillEncoding = encoding->_ffmpeg->stillEncoding();
            if (!stillEncoding)
            {
                encoding->_running = false;
            }
            */
            
			if (encodingCompleted)
			{
				Json::Value responseBodyRoot;

				string field = "encodingJobKey";
				responseBodyRoot[field] = selectedEncodingCompleted->_encodingJobKey;

				field = "pid";
				responseBodyRoot[field] = 0;

				field = "killedByUser";
				responseBodyRoot[field] = selectedEncodingCompleted->_killedByUser;

				field = "urlForbidden";
				responseBodyRoot[field] = selectedEncodingCompleted->_urlForbidden;

				field = "urlNotFound";
				responseBodyRoot[field] = selectedEncodingCompleted->_urlNotFound;

				field = "completedWithError";
				responseBodyRoot[field] = selectedEncodingCompleted->_completedWithError;

				field = "errorMessage";
				responseBodyRoot[field] = selectedEncodingCompleted->_errorMessage;

				field = "encodingFinished";
				responseBodyRoot[field] = true;

				field = "encodingProgress";
				responseBodyRoot[field] = 100;

				Json::StreamWriterBuilder wbuilder;
				responseBody = Json::writeString(wbuilder, responseBodyRoot);
			}
			else if (encodingFound)
			{
				int encodingProgress = -2;
				try
				{
					chrono::system_clock::time_point startEncodingProgress = chrono::system_clock::now();

					encodingProgress = selectedEncoding->_ffmpeg->getEncodingProgress();

					chrono::system_clock::time_point endEncodingProgress = chrono::system_clock::now();
					_logger->info(__FILEREF__ + "getEncodingProgress statistics"
							+ ", @MMS statistics@ - encodingProgress (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									endEncodingProgress - startEncodingProgress).count()) + "@"
							);
				}
				catch(FFMpegEncodingStatusNotAvailable e)
				{
					string errorMessage = string("_ffmpeg->getEncodingProgress failed")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
					;
					_logger->info(__FILEREF__ + errorMessage);

					// sendError(request, 500, errorMessage);

					// throw e;
					// return;
				}
				catch(exception e)
				{
					string errorMessage = string("_ffmpeg->getEncodingProgress failed")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
                    ;
					_logger->error(__FILEREF__ + errorMessage);

					// sendError(request, 500, errorMessage);

					// throw e;
					// return;
				}

				Json::Value responseBodyRoot;

				string field = "encodingJobKey";
				responseBodyRoot[field] = selectedEncoding->_encodingJobKey;

				field = "pid";
				responseBodyRoot[field] = selectedEncoding->_childPid;

				field = "killedByUser";
				responseBodyRoot[field] = false;

				field = "urlForbidden";
				responseBodyRoot[field] = false;

				field = "urlNotFound";
				responseBodyRoot[field] = false;

				field = "errorMessage";
				responseBodyRoot[field] = selectedEncoding->_errorMessage;

				field = "encodingFinished";
				responseBodyRoot[field] = !selectedEncoding->_running;

				field = "encodingProgress";
				if (encodingProgress == -2)
					responseBodyRoot[field] = Json::nullValue;
				else
					responseBodyRoot[field] = encodingProgress;

				Json::StreamWriterBuilder wbuilder;
				responseBody = Json::writeString(wbuilder, responseBodyRoot);
			}
			else if (liveProxyFound)
			{
				Json::Value responseBodyRoot;

				string field = "encodingJobKey";
				responseBodyRoot[field] = selectedLiveProxy->_encodingJobKey;

				field = "pid";
				responseBodyRoot[field] = selectedLiveProxy->_childPid;

				field = "killedByUser";
				responseBodyRoot[field] = false;

				field = "urlForbidden";
				responseBodyRoot[field] = false;

				field = "urlNotFound";
				responseBodyRoot[field] = false;

				field = "errorMessage";
				responseBodyRoot[field] = selectedLiveProxy->_errorMessage;

				// 2022-07-20: if we do not have a correct pid after 10 minutes
				//	we will force encodingFinished to true
				{
					field = "encodingFinished";
					if (selectedLiveProxy->_childPid <= 0 && selectedLiveProxy->_running)
					{
						// 2022-07-21: pensavo si verificasse questo scenario
						//	(selectedLiveProxy->_childPid <= 0 && selectedLiveProxy->_running)
						//	in realtà sembra non si verifica mai
						int64_t liveProxyLiveTimeInMinutes = 0;
						{
							chrono::system_clock::time_point now = chrono::system_clock::now();                           

							if (now > selectedLiveProxy->_proxyStart)
								liveProxyLiveTimeInMinutes = chrono::duration_cast<
									chrono::minutes>(now - selectedLiveProxy->_proxyStart).count();
							else	// it will be negative
								liveProxyLiveTimeInMinutes = chrono::duration_cast<
									chrono::minutes>(now - selectedLiveProxy->_proxyStart).count();
						}
						if (liveProxyLiveTimeInMinutes > 10)
						{
							_logger->error(__FILEREF__ + "encodingStatus, force encodingFinished to true"
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", liveProxyLiveTimeInMinutes: " + to_string(liveProxyLiveTimeInMinutes)
								+ ", selectedLiveProxy->_running: " + to_string(selectedLiveProxy->_running)
							);

							responseBodyRoot[field] = true;
						}
						else
						{
							_logger->warn(__FILEREF__ + "encodingStatus, encoding running but pid is <= 0!!!"
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", liveProxyLiveTimeInMinutes: " + to_string(liveProxyLiveTimeInMinutes)
								+ ", selectedLiveProxy->_running: " + to_string(selectedLiveProxy->_running)
							);

							responseBodyRoot[field] = !selectedLiveProxy->_running;
						}
					}
					else
						responseBodyRoot[field] = !selectedLiveProxy->_running;
				}

				// 2020-06-11: it's a live, it does not have sense the encoding progress
				field = "encodingProgress";
				responseBodyRoot[field] = Json::nullValue;

				Json::StreamWriterBuilder wbuilder;
				responseBody = Json::writeString(wbuilder, responseBodyRoot);
			}
			else // if (liveRecording)
			{
				Json::Value responseBodyRoot;

				string field = "encodingJobKey";
				responseBodyRoot[field] = selectedLiveRecording->_encodingJobKey;

				field = "pid";
				responseBodyRoot[field] = selectedLiveRecording->_childPid;

				field = "killedByUser";
				responseBodyRoot[field] = false;

				field = "urlForbidden";
				responseBodyRoot[field] = false;

				field = "urlNotFound";
				responseBodyRoot[field] = false;

				field = "errorMessage";
				responseBodyRoot[field] = selectedLiveRecording->_errorMessage;

				field = "encodingFinished";
				responseBodyRoot[field] = !selectedLiveRecording->_running;

				// 2020-10-13: we do not have here the information to calculate the encoding progress,
				//	it is calculated in EncoderVideoAudioProxy.cpp
				field = "encodingProgress";
				responseBodyRoot[field] = Json::nullValue;

				Json::StreamWriterBuilder wbuilder;
				responseBody = Json::writeString(wbuilder, responseBodyRoot);
			}
        }

		chrono::system_clock::time_point endEncodingStatus = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "encodingStatus"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", encodingFound: " + to_string(encodingFound)
			+ ", liveProxyFound: " + to_string(liveProxyFound)
			+ ", liveRecordingFound: " + to_string(liveRecordingFound)
			+ ", encodingCompleted: " + to_string(encodingCompleted)
			+ ", @MMS statistics@ - duration encodingStatus (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(endEncodingStatus - startEncodingStatus).count()) + "@"
		);

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, requestURI, requestMethod, 200, responseBody);
    }
    else if (method == "killEncodingJob")
    {
        /*
        bool isAdminAPI = get<1>(workspaceAndFlags);
        if (!isAdminAPI)
        {
            string errorMessage = string("APIKey flags does not have the ADMIN permission"
                    ", isAdminAPI: " + to_string(isAdminAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        */

        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
		}
        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		bool lightKill = false;
        auto lightKillIt = queryParameters.find("lightKill");
        if (lightKillIt != queryParameters.end())
			lightKill = lightKillIt->second == "true" ? true : false;

		_logger->info(__FILEREF__ + "Received killEncodingJob"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", lightKill: " + to_string(lightKill)
		);

		pid_t			pidToBeKilled;
		bool			encodingFound = false;
		bool			liveProxyFound = false;
		bool			liveRecorderFound = false;

		{
			// see comment 2020-11-30
			#if defined(__VECTOR__) && defined(__VECTOR__NO_LOCK_FOR_ENCODINGSTATUS)
			#else
			lock_guard<mutex> locker(*_encodingMutex);
			#endif

			#ifdef __VECTOR__
			for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			{
				if (encoding->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					pidToBeKilled = encoding->_childPid;

					break;
				}
			}
			#else	// __MAP__
			map<int64_t, shared_ptr<Encoding>>::iterator it =
				_encodingsCapability->find(encodingJobKey);
			if (it != _encodingsCapability->end())
			{
				encodingFound = true;
				pidToBeKilled = it->second->_childPid;
			}
			#endif
		}

		if (!encodingFound)
		{
			// see comment 2020-11-30
			#if defined(__VECTOR__) && defined(__VECTOR__NO_LOCK_FOR_ENCODINGSTATUS)
			#else
			lock_guard<mutex> locker(*_liveProxyMutex);
			#endif

			#ifdef __VECTOR__
			for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			{
				if (liveProxy->_encodingJobKey == encodingJobKey)
				{
					liveProxyFound = true;
					pidToBeKilled = liveProxy->_childPid;

					break;
				}
			}
			#else	// __MAP__
			map<int64_t, shared_ptr<LiveProxyAndGrid>>::iterator it =
				_liveProxiesCapability->find(encodingJobKey);
			if (it != _liveProxiesCapability->end())
			{
				liveProxyFound = true;
				pidToBeKilled = it->second->_childPid;
			}
			#endif
		}

		if (!encodingFound && !liveProxyFound)
		{
			// see comment 2020-11-30
			#if defined(__VECTOR__) && defined(__VECTOR__NO_LOCK_FOR_ENCODINGSTATUS)
			#else
			lock_guard<mutex> locker(*_liveRecordingMutex);
			#endif

			#ifdef __VECTOR__
			for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
			{
				if (liveRecording->_encodingJobKey == encodingJobKey)
				{
					liveRecorderFound = true;
					pidToBeKilled = liveRecording->_childPid;

					break;
				}
			}
			#else	// __MAP__
			map<int64_t, shared_ptr<LiveRecording>>::iterator it =
				_liveRecordingsCapability->find(encodingJobKey);
			if (it != _liveRecordingsCapability->end())
			{
				liveRecorderFound = true;
				pidToBeKilled = it->second->_childPid;
			}
			#endif
		}

        if (!encodingFound && !liveProxyFound && !liveRecorderFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingJobKeyFound().what();
            
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(errorMessage);
			return;
        }

		_logger->info(__FILEREF__ + "ProcessUtility::killProcess. Found Encoding to kill"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", pidToBeKilled: " + to_string(pidToBeKilled)
			+ ", lightKill: " + to_string(lightKill)
		);

		if (pidToBeKilled == 0)
		{
			_logger->error(__FILEREF__
				+ "The EncodingJob seems not running (see pidToBeKilled)"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", pidToBeKilled: " + to_string(pidToBeKilled)
			);

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
		}

        try
		{
			chrono::system_clock::time_point startKillProcess
				= chrono::system_clock::now();

			if (lightKill)
			{
				// 2022-11-02: SIGQUIT is managed inside FFMpeg.cpp by liverecording e liveProxy
				_logger->info(__FILEREF__ + "ProcessUtility::quitProcess"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", pidToBeKilled: " + to_string(pidToBeKilled)
				);
				ProcessUtility::quitProcess(pidToBeKilled);
			}
			else
			{
				_logger->info(__FILEREF__ + "ProcessUtility::killProcess"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", pidToBeKilled: " + to_string(pidToBeKilled)
				);
				ProcessUtility::killProcess(pidToBeKilled);
			}

			chrono::system_clock::time_point endKillProcess = chrono::system_clock::now();
			_logger->info(__FILEREF__ + "killProcess statistics"
				+ ", @MMS statistics@ - killProcess (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(
					endKillProcess - startKillProcess).count()) + "@"
			);
        }
        catch(runtime_error e)
        {
            string errorMessage = string("ProcessUtility::killProcess failed")
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", pidToBeKilled: " + to_string(pidToBeKilled)
                + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw e;
        }

		string responseBody = string("{ ")
			+ "\"encodingJobKey\": " + to_string(encodingJobKey)
			+ ", \"pid\": " + to_string(pidToBeKilled)
			+ "}";

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, requestURI, requestMethod, 200, responseBody);
    }
    else if (method == "changeLiveProxyPlaylist")
    {
        /*
        bool isAdminAPI = get<1>(workspaceAndFlags);
        if (!isAdminAPI)
        {
            string errorMessage = string("APIKey flags does not have the ADMIN permission"
                    ", isAdminAPI: " + to_string(isAdminAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        */

        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		string switchBehaviour;
        auto switchBehaviourIt = queryParameters.find("switchBehaviour");
        if (switchBehaviourIt == queryParameters.end())
			switchBehaviour = "applyNewPlaylistNow";
		else
			switchBehaviour = switchBehaviourIt->second;

		_logger->info(__FILEREF__ + "Received changeLiveProxyPlaylist"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		);

		bool			encodingFound = false;

		Json::Value newInputsRoot;
		try
		{
			Json::CharReaderBuilder builder;
			Json::CharReader* reader = builder.newCharReader();
			string errors;

			bool parsingSuccessful = reader->parse(requestBody.c_str(),
				requestBody.c_str() + requestBody.size(), 
				&newInputsRoot, &errors);
			delete reader;

			if (!parsingSuccessful)
			{
				string errorMessage = __FILEREF__ + "failed to parse the requestBody"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", errors: " + errors
					+ ", requestBody: " + requestBody
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		catch(...)
		{
			string errorMessage = string("Parsing new LiveProxy playlist failed")
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", requestBody: " + requestBody
			;
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			return;
			// throw runtime_error(errorMessage);
		}

		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			shared_ptr<LiveProxyAndGrid>	selectedLiveProxy;

			#ifdef __VECTOR__
			for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			{
				if (liveProxy->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					selectedLiveProxy = liveProxy;

					break;
				}
			}
			#else	// __MAP__
			map<int64_t, shared_ptr<LiveProxyAndGrid>>::iterator it =
				_liveProxiesCapability->find(encodingJobKey);
			if (it != _liveProxiesCapability->end())
			{
				encodingFound = true;
				selectedLiveProxy = it->second;
			}
			#endif

			if (!encodingFound)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
					+ ", " + NoEncodingJobKeyFound().what();
            
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
				return;
			}

			{
				_logger->info(__FILEREF__ + "Replacing the LiveProxy playlist"
					+ ", ingestionJobKey: " + to_string(selectedLiveProxy->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
				);

				lock_guard<mutex> locker(selectedLiveProxy->_inputsRootMutex);

				selectedLiveProxy->_inputsRoot = newInputsRoot;
			}

			// 2022-10-21: abbiamo due opzioni:
			//	- apply the new playlist now
			//	- apply the new playlist at the end of current media
			if (switchBehaviour == "applyNewPlaylistNow")
			{
				try
				{
					// 2022-11-02: SIGQUIT is managed inside FFMpeg.cpp by liveProxy
					_logger->info(__FILEREF__ + "ProcessUtility::quitProcess"
						+ ", ingestionJobKey: " + to_string(selectedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", selectedLiveProxy->_childPid: " + to_string(selectedLiveProxy->_childPid)
					);
					ProcessUtility::quitProcess(selectedLiveProxy->_childPid);
				}
				catch(runtime_error e)
				{
					string errorMessage = string("ProcessUtility::kill (quit) Process failed")
						+ ", ingestionJobKey: " + to_string(selectedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(selectedLiveProxy->_encodingJobKey)
						+ ", _childPid: " + to_string(selectedLiveProxy->_childPid)
						+ ", e.what(): " + e.what()
					;
					_logger->error(__FILEREF__ + errorMessage);
				}
			}
		}

		string responseBody;
		{
			Json::Value responseBodyRoot;

			string field = "encodingJobKey";
			responseBodyRoot[field] = encodingJobKey;

			Json::StreamWriterBuilder wbuilder;
			responseBody = Json::writeString(wbuilder, responseBodyRoot);
		}

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, requestURI, requestMethod, 200, responseBody);
    }
    else if (method == "encodingProgress")
    {
		// 2020-10-13: The encodingProgress API is not called anymore
		// because it is the encodingStatus API returning the encodingProgress

        /*
        bool isAdminAPI = get<1>(workspaceAndFlags);
        if (!isAdminAPI)
        {
            string errorMessage = string("APIKey flags does not have the ADMIN permission"
                    ", isAdminAPI: " + to_string(isAdminAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        */
        
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		bool                    encodingCompleted = false;
		shared_ptr<EncodingCompleted>    selectedEncodingCompleted;

		shared_ptr<Encoding>    selectedEncoding;
		bool                    encodingFound = false;

		shared_ptr<LiveProxyAndGrid>	selectedLiveProxy;
		bool					liveProxyFound = false;

		{
			lock_guard<mutex> locker(*_encodingCompletedMutex);

			map<int64_t, shared_ptr<EncodingCompleted>>::iterator it =
				_encodingCompletedMap->find(encodingJobKey);
			if (it != _encodingCompletedMap->end())
			{
				encodingCompleted = true;
				selectedEncodingCompleted = it->second;
			}
		}

		/*
		if (!encodingCompleted)
		{
			// next \{ is to make the lock free as soon as the check is done
			{
				lock_guard<mutex> locker(*_encodingMutex);

				for (shared_ptr<Encoding> encoding: *_encodingsCapability)
				{
					if (encoding->_encodingJobKey == encodingJobKey)
					{
						encodingFound = true;
						selectedEncoding = encoding;
                
						break;
					}
				}
			}

			if (!encodingFound)
			{
				lock_guard<mutex> locker(*_liveProxyMutex);

				for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
				{
					if (liveProxy->_encodingJobKey == encodingJobKey)
					{
						liveProxyFound = true;
						selectedLiveProxy = liveProxy;

						break;
					}
				}
			}
		}

        if (!encodingCompleted && !encodingFound && !liveProxyFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingJobKeyFound().what();
            
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(errorMessage);
			return;
        }

        if (encodingFound)
		{
			int encodingProgress;
			try
			{
				encodingProgress = selectedEncoding->_ffmpeg->getEncodingProgress();
			}
			catch(FFMpegEncodingStatusNotAvailable e)
			{
				string errorMessage = string("_ffmpeg->getEncodingProgress failed")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
						;
				_logger->info(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				// throw e;
				return;
			}
			catch(exception e)
			{
				string errorMessage = string("_ffmpeg->getEncodingProgress failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
                    ;
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				// throw e;
				return;
			}
        
			string responseBody = string("{ ")
				+ "\"encodingJobKey\": " + to_string(encodingJobKey)
				+ ", \"pid\": " + to_string(selectedEncoding->_childPid)
				+ ", \"encodingProgress\": " + to_string(encodingProgress) + " "
				+ "}";

			sendSuccess(request, 200, responseBody);
		}
		else if (liveProxyFound)
		{
			int encodingProgress;
			try
			{
				// 2020-06-11: it's a live, it does not have sense the encoding progress
				// encodingProgress = selectedLiveProxy->_ffmpeg->getEncodingProgress();
				encodingProgress = -1;
			}
			catch(FFMpegEncodingStatusNotAvailable e)
			{
				string errorMessage = string("_ffmpeg->getEncodingProgress failed")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
						;
				_logger->info(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				// throw e;
				return;
			}
			catch(exception e)
			{
				string errorMessage = string("_ffmpeg->getEncodingProgress failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", e.what(): " + e.what()
                    ;
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				// throw e;
				return;
			}
        
			string responseBody = string("{ ")
				+ "\"encodingJobKey\": " + to_string(encodingJobKey)
				+ ", \"pid\": " + to_string(selectedLiveProxy->_childPid)
				+ ", \"encodingProgress\": " + to_string(encodingProgress) + " "
				+ "}";

			sendSuccess(request, 200, responseBody);
		}
		else if (encodingCompleted)
		{
			int encodingProgress = 100;
        
			string responseBody = string("{ ")
				+ "\"encodingJobKey\": " + to_string(encodingJobKey)
				+ ", \"encodingProgress\": " + to_string(encodingProgress) + " "
				+ "}";

			sendSuccess(request, 200, responseBody);
		}
		else // if (!encodingCompleted)
		{
			string errorMessage = method + ": " + FFMpegEncodingStatusNotAvailable().what()
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingCompleted: " + to_string(encodingCompleted)
					;
			_logger->info(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			// throw e;
			return;
		}
		*/
    }
    else
    {
        string errorMessage = string("No API is matched")
            + ", requestURI: " + requestURI
            + ", method: " + method
            + ", requestMethod: " + requestMethod;
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 400, errorMessage);

        throw runtime_error(errorMessage);
    }

	if (chrono::system_clock::now() - *_lastEncodingCompletedCheck >=
		chrono::seconds(_encodingCompletedRetentionInSeconds))
	{
		*_lastEncodingCompletedCheck = chrono::system_clock::now();
		encodingCompletedRetention();
	}

	/* this statistics information is already present in APICommon.cpp
	chrono::system_clock::time_point endManageRequestAndResponse = chrono::system_clock::now();
	_logger->info(__FILEREF__ + "manageRequestAndResponse"
		+ ", method: " + method
		+ ", @MMS statistics@ - duration manageRequestAndResponse (secs): @"
			+ to_string(chrono::duration_cast<chrono::seconds>(endManageRequestAndResponse - startManageRequestAndResponse).count()) + "@"
	);
	*/
}

void FFMPEGEncoder::encodeContentThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
    string api = "encodeContent";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", requestBody: " + requestBody
    );

	bool externalEncoder = false;
	string sourceAssetPathName;
	string encodedStagingAssetPathName;
	int64_t ingestionJobKey = 1;
    try
    {
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value encodingMedatada;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
				requestBody.c_str() + requestBody.size(), 
				&encodingMedatada, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        ingestionJobKey = JSONUtils::asInt64(encodingMedatada, "ingestionJobKey", -1);

        externalEncoder = JSONUtils::asBool(encodingMedatada, "externalEncoder", false);

        int videoTrackIndexToBeUsed = JSONUtils::asInt(encodingMedatada["ingestedParametersRoot"],
			"VideoTrackIndex", -1);
        int audioTrackIndexToBeUsed = JSONUtils::asInt(encodingMedatada["ingestedParametersRoot"],
			"AudioTrackIndex", -1);

		Json::Value sourcesToBeEncodedRoot = encodingMedatada["encodingParametersRoot"]["sourcesToBeEncodedRoot"];
		Json::Value sourceToBeEncodedRoot = sourcesToBeEncodedRoot[0];

        int64_t durationInMilliSeconds = JSONUtils::asInt64(sourceToBeEncodedRoot,
				"sourceDurationInMilliSecs", -1);
		Json::Value encodingProfileDetailsRoot = encodingMedatada["encodingParametersRoot"]
			["encodingProfileDetailsRoot"];
        MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(
				encodingMedatada["encodingParametersRoot"].get("contentType", "").asString());
        int64_t physicalPathKey = JSONUtils::asInt64(sourceToBeEncodedRoot, "sourcePhysicalPathKey", -1);
        // int64_t encodingJobKey = JSONUtils::asInt64(encodingMedatada, "encodingJobKey", -1);

		Json::Value videoTracksRoot;
		string field = "videoTracks";
        if (JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			videoTracksRoot = sourceToBeEncodedRoot[field];
		Json::Value audioTracksRoot;
		field = "audioTracks";
        if (JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			audioTracksRoot = sourceToBeEncodedRoot[field];

		if (externalEncoder)
		{
			bool isSourceStreaming = false;
			{
				field = "sourceFileExtension";
				if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string sourceFileExtension = sourceToBeEncodedRoot.get(field, "").asString();

				if (sourceFileExtension == ".m3u8")
					isSourceStreaming = true;
			}

			field = "sourceTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sourceTranscoderStagingAssetPathName = sourceToBeEncodedRoot.get(field, "").asString();

			{
				size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = sourceTranscoderStagingAssetPathName.substr(
						0, endOfDirectoryIndex);

					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(__FILEREF__ + "Creating directory"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", directoryPathName: " + directoryPathName
					);
					FileIO::createDirectory(directoryPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}
			}

			field = "sourcePhysicalDeliveryURL";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sourcePhysicalDeliveryURL = sourceToBeEncodedRoot.get(field, "").asString();

			field = "encodedTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = sourceToBeEncodedRoot.get(field, "").asString();

			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(
						0, endOfDirectoryIndex);

					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(__FILEREF__ + "Creating directory"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", directoryPathName: " + directoryPathName
					);
					FileIO::createDirectory(directoryPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}
			}

			_logger->info(__FILEREF__ + "downloading source content"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", externalEncoder: " + to_string(externalEncoder)
				+ ", sourcePhysicalDeliveryURL: " + sourcePhysicalDeliveryURL
				+ ", sourceTranscoderStagingAssetPathName: " + sourceTranscoderStagingAssetPathName
				+ ", isSourceStreaming: " + to_string(isSourceStreaming)
			);

			if (isSourceStreaming)
			{
				// regenerateTimestamps: see docs/TASK_01_Add_Content_JSON_Format.txt
				bool regenerateTimestamps = false;

				sourceAssetPathName = sourceTranscoderStagingAssetPathName + ".mp4";

				encoding->_ffmpeg->streamingToFile(
					ingestionJobKey,
					regenerateTimestamps,
					sourcePhysicalDeliveryURL,
					sourceAssetPathName);
			}
			else
			{
				sourceAssetPathName = sourceTranscoderStagingAssetPathName;

				MMSCURL::downloadFile(
					ingestionJobKey,
					sourcePhysicalDeliveryURL,
					sourceAssetPathName,
					_logger
				);
			}

			_logger->info(__FILEREF__ + "downloaded source content"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", externalEncoder: " + to_string(externalEncoder)
				+ ", sourcePhysicalDeliveryURL: " + sourcePhysicalDeliveryURL
				+ ", sourceTranscoderStagingAssetPathName: " + sourceTranscoderStagingAssetPathName
				+ ", sourceAssetPathName: " + sourceAssetPathName
				+ ", isSourceStreaming: " + to_string(isSourceStreaming)
			);
		}
		else
		{
			field = "mmsSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceAssetPathName = sourceToBeEncodedRoot.get(field, "").asString();

			field = "encodedNFSStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = sourceToBeEncodedRoot.get(field, "").asString();
		}

        _logger->info(__FILEREF__ + "encoding content..."
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", sourceAssetPathName: " + sourceAssetPathName
            + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
        );

		// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
        encoding->_ffmpeg->encodeContent(
			sourceAssetPathName,
			durationInMilliSeconds,
			encodedStagingAssetPathName,
			encodingProfileDetailsRoot,
			contentType == MMSEngineDBFacade::ContentType::Video,
			videoTracksRoot,
			audioTracksRoot,
			videoTrackIndexToBeUsed, audioTrackIndexToBeUsed,
			physicalPathKey,
			encodingJobKey,
			ingestionJobKey,
			&(encoding->_childPid)
		);
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "encoded content"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", sourceAssetPathName: " + sourceAssetPathName
            + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
        );

		if (externalEncoder)
		{
			field = "sourceMediaItemKey";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			int64_t sourceMediaItemKey = JSONUtils::asInt64(sourceToBeEncodedRoot, field, -1);

			field = "sourcePhysicalPathKey";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			int64_t sourcePhysicalPathKey = JSONUtils::asInt64(sourceToBeEncodedRoot, field, -1);

			field = "encodingProfileKey";
			if (!JSONUtils::isMetadataPresent(encodingMedatada["encodingParametersRoot"], field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			int64_t encodingProfileKey = JSONUtils::asInt64(encodingMedatada["encodingParametersRoot"], field, -1);

			field = "FileFormat";
			if (!JSONUtils::isMetadataPresent(encodingProfileDetailsRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string fileFormat = encodingProfileDetailsRoot.get(field, "").asString();

			int64_t userKey;
			string apiKey;
			{
				field = "internalMMS";
				if (JSONUtils::isMetadataPresent(encodingMedatada["ingestedParametersRoot"], field))
				{
					Json::Value internalMMSRoot = encodingMedatada["ingestedParametersRoot"][field];

					field = "credentials";
					if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
					{
						Json::Value credentialsRoot = internalMMSRoot[field];

						field = "userKey";
						userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

						field = "apiKey";
						string apiKeyEncrypted = credentialsRoot.get(field, "").asString();
						apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
					}
				}
			}

			field = "mmsWorkflowIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingMedatada["encodingParametersRoot"], field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string mmsWorkflowIngestionURL = encodingMedatada["encodingParametersRoot"].get(field, "").asString();

			field = "mmsBinaryIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingMedatada["encodingParametersRoot"], field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string mmsBinaryIngestionURL = encodingMedatada["encodingParametersRoot"].get(field, "").asString();

			// static unsigned long long getDirectorySizeInBytes (string directoryPathName);                             

			int64_t variantFileSizeInBytes = 0;
			if (fileFormat != "hls")
			{
				bool inCaseOfLinkHasItToBeRead = false;
				int64_t variantFileSizeInBytes = FileIO::getFileSizeInBytes (encodedStagingAssetPathName,
					inCaseOfLinkHasItToBeRead);                                                                          
			}

			ingestAVariant(
				ingestionJobKey,
				sourceMediaItemKey,
				sourcePhysicalPathKey,
				encodingProfileKey,
				fileFormat,
				encodedStagingAssetPathName,
				variantFileSizeInBytes,
				userKey,
				apiKey,
				mmsWorkflowIngestionURL,                                                                                
				mmsBinaryIngestionURL
			);

			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(sourceAssetPathName, exceptionInCaseOfError);
			}

			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
		}

        encoding->_running = false;
        encoding->_childPid = 0;
        
		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
			completedWithError, encoding->_errorMessage, killedByUser,
			urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

		if (externalEncoder)
		{
			if (sourceAssetPathName != "" && FileIO::fileExisting(sourceAssetPathName))
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(sourceAssetPathName, exceptionInCaseOfError);
			}

			if (encodedStagingAssetPathName != "")
			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
			completedWithError, encoding->_errorMessage, killedByUser,
			urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		if (externalEncoder)
		{
			if (sourceAssetPathName != "" && FileIO::fileExisting(sourceAssetPathName))
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(sourceAssetPathName, exceptionInCaseOfError);
			}

			if (encodedStagingAssetPathName != "")
			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);
		
		encoding->_errorMessage = e.what();

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		if (externalEncoder)
		{
			if (sourceAssetPathName != "" && FileIO::fileExisting(sourceAssetPathName))
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(sourceAssetPathName, exceptionInCaseOfError);
			}

			if (encodedStagingAssetPathName != "")
			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

string FFMPEGEncoder::buildVariantIngestionWorkflow(
	int64_t ingestionJobKey,
	int64_t sourceMediaItemKey,
	int64_t sourcePhysicalPathKey,
	int64_t encodingProfileKey,
	string fileFormat
)
{
	string workflowMetadata;
	try
	{
		string label = "Add Variant " + to_string(sourceMediaItemKey)
			+ " - " + to_string(sourcePhysicalPathKey)
			+ " - " + to_string(encodingProfileKey);

		Json::Value addContentRoot;

		string field = "Label";
		addContentRoot[field] = label;

		field = "Type";
		addContentRoot[field] = "Add-Content";

		Json::Value addContentParametersRoot;

		field = "FileFormat";
		addContentParametersRoot[field] = fileFormat;

		field = "Ingester";
		addContentParametersRoot[field] = "Encode Task";

		field = "variantOfMediaItemKey";
		addContentParametersRoot[field] = sourceMediaItemKey;

		field = "variantEncodingProfileKey";
		addContentParametersRoot[field] = encodingProfileKey;

		field = "Parameters";
		addContentRoot[field] = addContentParametersRoot;


		Json::Value workflowRoot;

		field = "Label";
		workflowRoot[field] = label;

		field = "Type";
		workflowRoot[field] = "Workflow";

		field = "Task";
		workflowRoot[field] = addContentRoot;

   		{
       		Json::StreamWriterBuilder wbuilder;
       		workflowMetadata = Json::writeString(wbuilder, workflowRoot);
   		}

		_logger->info(__FILEREF__ + "Variant workflow metadata generated"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", workflowMetadata: " + workflowMetadata
		);

		return workflowMetadata;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "buildVariantIngestionWorkflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "buildVariantIngestionWorkflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

void FFMPEGEncoder::ingestAVariant(
	int64_t ingestionJobKey,
	int64_t sourceMediaItemKey,
	int64_t sourcePhysicalPathKey,
	int64_t encodingProfileKey,
	string fileFormat,
	string variantPathFileName,
	int64_t variantFileSizeInBytes,
	int64_t userKey,
	string apiKey,
	string mmsWorkflowIngestionURL,
	string mmsBinaryIngestionURL
)
{
	_logger->info(__FILEREF__ + "Received ingestAVariant"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
		+ ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
		+ ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
		+ ", encodingProfileKey: " + to_string(encodingProfileKey)
		+ ", fileFormat: " + fileFormat
		+ ", variantPathFileName: " + variantPathFileName
		+ ", variantFileSizeInBytes: " + to_string(variantFileSizeInBytes)
		+ ", userKey: " + to_string(userKey)
		+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
		+ ", mmsBinaryIngestionURL: " + mmsBinaryIngestionURL
	);

	string workflowMetadata;
	int64_t addContentIngestionJobKey = -1;
	// create the workflow and ingest it
	try
	{
		string localFileFormat = fileFormat;
		if (fileFormat == "hls")
			localFileFormat = "m3u8-tar.gz";

		workflowMetadata = buildVariantIngestionWorkflow(
			ingestionJobKey, sourceMediaItemKey, sourcePhysicalPathKey,
			encodingProfileKey, localFileFormat);

		string sResponse = MMSCURL::httpPostPutString(
			ingestionJobKey,
			mmsWorkflowIngestionURL,
			"POST",	// requestType
			_mmsAPITimeoutInSeconds,
			to_string(userKey),
			apiKey,
			workflowMetadata,
			"application/json",	// contentType
			_logger
		);

		addContentIngestionJobKey = getAddContentIngestionJobKey(ingestionJobKey, sResponse);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}

	if (addContentIngestionJobKey == -1)
	{
		string errorMessage =
			string("Ingested URL failed, addContentIngestionJobKey is not valid")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	string mmsBinaryURL;
	// ingest binary
	try
	{
		string localVariantPathFileName = variantPathFileName;
		int64_t localVariantFileSizeInBytes = variantFileSizeInBytes;
		if (fileFormat == "hls")
		{
			// variantPathFileName is a dir like
			// /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/1_1607526_2022_11_09_09_11_04_0431/content
			// terminating with 'content' as built in MMSEngineProcessor.cpp

			{
				string executeCommand;
				try
				{
					localVariantPathFileName = variantPathFileName + ".tar.gz";

					size_t endOfPathIndex = localVariantPathFileName.find_last_of("/");
					if (endOfPathIndex == string::npos)
					{
						string errorMessage = string("No localVariantPathDirectory found")
							+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
							+ ", localVariantPathFileName: " + localVariantPathFileName 
						;
						_logger->error(__FILEREF__ + errorMessage);
          
						throw runtime_error(errorMessage);
					}
					string localVariantPathDirectory =
						localVariantPathFileName.substr(0, endOfPathIndex);

					executeCommand =
						"tar cfz " + localVariantPathFileName
						+ " -C " + localVariantPathDirectory
						+ " " + "content";
					_logger->info(__FILEREF__ + "Start tar command "
						+ ", executeCommand: " + executeCommand
					);
					chrono::system_clock::time_point startTar = chrono::system_clock::now();
					int executeCommandStatus = ProcessUtility::execute(executeCommand);
					chrono::system_clock::time_point endTar = chrono::system_clock::now();
					_logger->info(__FILEREF__ + "End tar command "
						+ ", executeCommand: " + executeCommand
						+ ", @MMS statistics@ - tarDuration (millisecs): @"
							+ to_string(chrono::duration_cast<chrono::milliseconds>(endTar - startTar).count()) + "@"
					);
					if (executeCommandStatus != 0)
					{
						string errorMessage = string("ProcessUtility::execute failed")
							+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
							+ ", executeCommandStatus: " + to_string(executeCommandStatus) 
							+ ", executeCommand: " + executeCommand 
						;
						_logger->error(__FILEREF__ + errorMessage);
          
						throw runtime_error(errorMessage);
					}

					bool inCaseOfLinkHasItToBeRead = false;
					localVariantFileSizeInBytes = FileIO::getFileSizeInBytes (localVariantPathFileName,
						inCaseOfLinkHasItToBeRead);                                                                          
				}
				catch(runtime_error e)
				{
					string errorMessage = string("tar command failed")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
						+ ", executeCommand: " + executeCommand 
					;
					_logger->error(__FILEREF__ + errorMessage);
         
					throw runtime_error(errorMessage);
				}
			}
		}

		mmsBinaryURL =
			mmsBinaryIngestionURL
			+ "/" + to_string(addContentIngestionJobKey)
		;

		string sResponse = MMSCURL::httpPostPutFile(
			ingestionJobKey,
			mmsBinaryURL,
			"POST",	// requestType
			_mmsBinaryTimeoutInSeconds,
			to_string(userKey),
			apiKey,
			localVariantPathFileName,
			localVariantFileSizeInBytes,
			_logger);

		if (fileFormat == "hls")
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", localVariantPathFileName: " + localVariantPathFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(localVariantPathFileName, exceptionInCaseOfError);
		}
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		if (fileFormat == "hls")
		{
			// it is useless to remove the generated tar.gz file because the parent staging directory
			// will be removed. Also here we should add a bool above to be sure the tar was successful
			/*
			_logger->info(__FILEREF__ + "remove"
				+ ", localVariantPathFileName: " + localVariantPathFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(localVariantPathFileName, exceptionInCaseOfError);
			*/
		}

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		if (fileFormat == "hls")
		{
			// it is useless to remove the generated tar.gz file because the parent staging directory
			// will be removed. Also here we should add a bool above to be sure the tar was successful
			/*
			_logger->info(__FILEREF__ + "remove"
				+ ", localVariantPathFileName: " + localVariantPathFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(localVariantPathFileName, exceptionInCaseOfError);
			*/
		}

		throw e;
	}
}


void FFMPEGEncoder::overlayImageOnVideoThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
    string api = "overlayImageOnVideo";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value overlayMedatada;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &overlayMedatada, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string mmsSourceVideoAssetPathName = overlayMedatada.get("mmsSourceVideoAssetPathName", "XXX").asString();
        int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(overlayMedatada, "videoDurationInMilliSeconds", -1);
        string mmsSourceImageAssetPathName = overlayMedatada.get("mmsSourceImageAssetPathName", "XXX").asString();
        string imagePosition_X_InPixel = overlayMedatada.get("imagePosition_X_InPixel", "XXX").asString();
        string imagePosition_Y_InPixel = overlayMedatada.get("imagePosition_Y_InPixel", "XXX").asString();

        // string encodedFileName = overlayMedatada.get("encodedFileName", "XXX").asString();
        string stagingEncodedAssetPathName = overlayMedatada.get("stagingEncodedAssetPathName", "XXX").asString();
        int64_t encodingJobKey = JSONUtils::asInt64(overlayMedatada, "encodingJobKey", -1);
        int64_t ingestionJobKey = JSONUtils::asInt64(overlayMedatada, "ingestionJobKey", -1);

		// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
        encoding->_ffmpeg->overlayImageOnVideo(
            mmsSourceVideoAssetPathName,
            videoDurationInMilliSeconds,
            mmsSourceImageAssetPathName,
            imagePosition_X_InPixel,
            imagePosition_Y_InPixel,
            // encodedFileName,
            stagingEncodedAssetPathName,
            encodingJobKey,
            ingestionJobKey,
			&(encoding->_childPid));
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

//        string responseBody = string("{ ")
//                + "\"ingestionJobKey\": " + to_string(ingestionJobKey) + " "
//                + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
//                + "}";

        // sendSuccess(request, 200, responseBody);
        
        encoding->_running = false;
        encoding->_childPid = 0;
        
        _logger->info(__FILEREF__ + "Encode content finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::overlayTextOnVideoThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
    string api = "overlayTextOnVideo";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value overlayTextMedatada;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &overlayTextMedatada, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        string mmsSourceVideoAssetPathName = overlayTextMedatada["encodingParametersRoot"].
			get("sourceAssetPathName", "").asString();
        int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(
			overlayTextMedatada["encodingParametersRoot"], "sourceDurationInMilliSeconds", -1);

		Json::Value drawTextDetailsRoot = overlayTextMedatada["ingestedParametersRoot"]["drawTextDetails"];
		string text = drawTextDetailsRoot.get("text", "").asString();
		string textPosition_X_InPixel = drawTextDetailsRoot.get("textPosition_X_InPixel", "").asString();
		string textPosition_Y_InPixel = drawTextDetailsRoot.get("textPosition_Y_InPixel", "").asString();
		string fontType = drawTextDetailsRoot.get("fontType", "").asString();
        int fontSize = JSONUtils::asInt(drawTextDetailsRoot, "fontSize", -1);
		string fontColor = drawTextDetailsRoot.get("fontColor", "").asString();
		int textPercentageOpacity = JSONUtils::asInt(drawTextDetailsRoot,
			"textPercentageOpacity", -1);
		int shadowX = JSONUtils::asInt(drawTextDetailsRoot, "shadowX", 0);
		int shadowY = JSONUtils::asInt(drawTextDetailsRoot, "shadowY", 0);
		bool boxEnable = JSONUtils::asBool(drawTextDetailsRoot, "boxEnable", false);
		string boxColor = drawTextDetailsRoot.get("boxColor", "").asString();
		int boxPercentageOpacity = JSONUtils::asInt(drawTextDetailsRoot, "boxPercentageOpacity", -1);

        string stagingEncodedAssetPathName = overlayTextMedatada.
			get("stagingEncodedAssetPathName", "").asString();
        int64_t encodingJobKey = JSONUtils::asInt64(overlayTextMedatada, "encodingJobKey", -1);
        int64_t ingestionJobKey = JSONUtils::asInt64(overlayTextMedatada, "ingestionJobKey", -1);

		// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
		encoding->_ffmpeg->overlayTextOnVideo(
			mmsSourceVideoAssetPathName,
			videoDurationInMilliSeconds,

			text,
			textPosition_X_InPixel,
			textPosition_Y_InPixel,
			fontType,
			fontSize,
			fontColor,
			textPercentageOpacity,
			shadowX, shadowY,
			boxEnable,
			boxColor,
			boxPercentageOpacity,

			// encodedFileName,
			stagingEncodedAssetPathName,
			encodingJobKey,
			ingestionJobKey,
			&(encoding->_childPid));
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

        
        encoding->_running = false;
        encoding->_childPid = 0;
        
        _logger->info(__FILEREF__ + "Encode content finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::generateFramesThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
    string api = "generateFrames";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", requestBody: " + requestBody
    );

	bool externalEncoder = false;
	string imagesDirectory;
    try
    {
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value generateFramesMedatada;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &generateFramesMedatada, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        bool externalEncoder = JSONUtils::asBool(generateFramesMedatada, "externalEncoder", false);
        int64_t ingestionJobKey = JSONUtils::asInt64(generateFramesMedatada, "ingestionJobKey", -1);
		Json::Value encodingParametersRoot = generateFramesMedatada["encodingParametersRoot"];
		Json::Value ingestedParametersRoot = generateFramesMedatada["ingestedParametersRoot"];

        double startTimeInSeconds = JSONUtils::asDouble(encodingParametersRoot, "startTimeInSeconds", 0);
        int maxFramesNumber = JSONUtils::asInt(encodingParametersRoot, "maxFramesNumber", -1);
        string videoFilter = encodingParametersRoot.get("videoFilter", "").asString();
        int periodInSeconds = JSONUtils::asInt(encodingParametersRoot, "periodInSeconds", -1);
        bool mjpeg = JSONUtils::asBool(encodingParametersRoot, "mjpeg", false);
        int imageWidth = JSONUtils::asInt(encodingParametersRoot, "imageWidth", -1);
        int imageHeight = JSONUtils::asInt(encodingParametersRoot, "imageHeight", -1);
        int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot, "videoDurationInMilliSeconds", -1);

		string sourceAssetPathName;

		if (externalEncoder)
		{
			string field = "transcoderStagingImagesDirectory";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			imagesDirectory = encodingParametersRoot.get(field, "").asString();

			bool isSourceStreaming = false;
			{
				field = "sourceFileName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string sourceFileName = encodingParametersRoot.get(field, "").asString();

				string sourceFileExtension;                                                               
				{                                                                                         
					size_t extensionIndex = sourceFileName.find_last_of(".");                             
					if (extensionIndex == string::npos)                                                   
					{                                                                                     
						string errorMessage = __FILEREF__ + "No extension find in the asset file name"    
							+ ", sourceFileName: " + sourceFileName;                
						_logger->error(errorMessage);                                                     

						throw runtime_error(errorMessage);                                                
					}                                                                                     
					sourceFileExtension = sourceFileName.substr(extensionIndex);                          
				}

				if (sourceFileExtension == ".m3u8")
					isSourceStreaming = true;
			}

			string sourcePhysicalDeliveryURL = encodingParametersRoot.
				get("sourcePhysicalDeliveryURL", "").asString();
			string sourceTranscoderStagingAssetPathName = encodingParametersRoot.
				get("sourceTranscoderStagingAssetPathName", "").asString();

			{
				string sourceTranscoderStagingAssetDirectory;
				{
					size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");                             
					if (endOfDirectoryIndex == string::npos)                                                   
					{                                                                                     
						string errorMessage = __FILEREF__ + "No directory find in the asset file name"
							+ ", sourceTranscoderStagingAssetPathName: " + sourceTranscoderStagingAssetPathName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);                                                
					}                                                                                     
					sourceTranscoderStagingAssetDirectory = sourceTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);                          
				}

				if (!FileIO::directoryExisting(sourceTranscoderStagingAssetDirectory))
				{
					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(__FILEREF__ + "Creating directory"
						+ ", sourceTranscoderStagingAssetDirectory: " + sourceTranscoderStagingAssetDirectory
					);
					FileIO::createDirectory(sourceTranscoderStagingAssetDirectory,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH,
						noErrorIfExists, recursive);
				}
			}

			_logger->info(__FILEREF__ + "downloading source content"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", externalEncoder: " + to_string(externalEncoder)
				+ ", sourcePhysicalDeliveryURL: " + sourcePhysicalDeliveryURL
				+ ", sourceTranscoderStagingAssetPathName: " + sourceTranscoderStagingAssetPathName
				+ ", isSourceStreaming: " + to_string(isSourceStreaming)
			);

			if (isSourceStreaming)
			{
				// regenerateTimestamps: see docs/TASK_01_Add_Content_JSON_Format.txt
				bool regenerateTimestamps = false;

				sourceAssetPathName = sourceTranscoderStagingAssetPathName + ".mp4";

				encoding->_ffmpeg->streamingToFile(
					ingestionJobKey,
					regenerateTimestamps,
					sourcePhysicalDeliveryURL,
					sourceAssetPathName);
			}
			else
			{
				sourceAssetPathName = sourceTranscoderStagingAssetPathName;

				MMSCURL::downloadFile(
					ingestionJobKey,
					sourcePhysicalDeliveryURL,
					sourceAssetPathName,
					_logger
				);
			}

			_logger->info(__FILEREF__ + "downloaded source content"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", externalEncoder: " + to_string(externalEncoder)
				+ ", sourcePhysicalDeliveryURL: " + sourcePhysicalDeliveryURL
				+ ", sourceTranscoderStagingAssetPathName: " + sourceTranscoderStagingAssetPathName
				+ ", sourceAssetPathName: " + sourceAssetPathName
				+ ", isSourceStreaming: " + to_string(isSourceStreaming)
			);
		}
		else
		{
			string field = "sourcePhysicalPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceAssetPathName = encodingParametersRoot.get(field, "").asString();

			field = "nfsImagesDirectory";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			imagesDirectory = encodingParametersRoot.get(field, "").asString();
		}

		string imageBaseFileName = to_string(ingestionJobKey);

		encoding->_ffmpeg->generateFramesToIngest(
			ingestionJobKey,
			encodingJobKey,
			imagesDirectory,
			imageBaseFileName,
			startTimeInSeconds,
			maxFramesNumber,
			videoFilter,
			periodInSeconds,
			mjpeg,
			imageWidth, 
			imageHeight,
			sourceAssetPathName,
			videoDurationInMilliSeconds,
			&(encoding->_childPid)
		);

        encoding->_running = false;
        encoding->_childPid = 0;

		bool completedWithError			= false;
		if (externalEncoder)
		{
			FileIO::DirectoryEntryType_t detDirectoryEntryType;
			shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (imagesDirectory + "/");

			vector<int64_t> addContentIngestionJobKeys;

			bool scanDirectoryFinished = false;
			int generatedFrameIndex = 0;
			while (!scanDirectoryFinished)
			{
				try
				{
					string generatedFrameFileName = FileIO::readDirectory (directory, &detDirectoryEntryType);

					if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
						continue;

					if (!(generatedFrameFileName.size() >= imageBaseFileName.size()
						&& 0 == generatedFrameFileName.compare(0, imageBaseFileName.size(), imageBaseFileName)))
						continue;

					string generateFrameTitle = ingestedParametersRoot.get("Title", "").asString();

					string ingestionJobLabel = generateFrameTitle + " (" + to_string(generatedFrameIndex) + ")";

					Json::Value userDataRoot;
					{
						if (JSONUtils::isMetadataPresent(ingestedParametersRoot, "UserData"))
							userDataRoot = ingestedParametersRoot["UserData"];

						Json::Value mmsDataRoot;
						mmsDataRoot["dataType"] = "generatedFrame";
						mmsDataRoot["ingestionJobLabel"] = ingestionJobLabel;
						mmsDataRoot["ingestionJobKey"] = ingestionJobKey;
						mmsDataRoot["generatedFrameIndex"] = generatedFrameIndex;

						userDataRoot["mmsData"] = mmsDataRoot;
					}

					// Title
					string addContentTitle = ingestionJobLabel;

					string outputFileFormat;                                                               
					try
					{
						{
							size_t extensionIndex = generatedFrameFileName.find_last_of(".");                             
							if (extensionIndex == string::npos)                                                   
							{                                                                                     
								string errorMessage = __FILEREF__ + "No extension find in the asset file name"
									+ ", generatedFrameFileName: " + generatedFrameFileName;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);                                                
							}                                                                                     
							outputFileFormat = generatedFrameFileName.substr(extensionIndex + 1);                          
						}

						_logger->info(__FILEREF__ + "ingest Frame"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", externalEncoder: " + to_string(externalEncoder)
							+ ", imagesDirectory: " + imagesDirectory
							+ ", generatedFrameFileName: " + generatedFrameFileName
							+ ", addContentTitle: " + addContentTitle
							+ ", outputFileFormat: " + outputFileFormat
						);

						addContentIngestionJobKeys.push_back(
							generateFrames_ingestFrame(
								ingestionJobKey, externalEncoder,
								imagesDirectory, generatedFrameFileName,
								addContentTitle, userDataRoot, outputFileFormat,
								ingestedParametersRoot, encodingParametersRoot)
						);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "generateFrames_ingestFrame failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", externalEncoder: " + to_string(externalEncoder)
							+ ", imagesDirectory: " + imagesDirectory
							+ ", generatedFrameFileName: " + generatedFrameFileName
							+ ", addContentTitle: " + addContentTitle
							+ ", outputFileFormat: " + outputFileFormat
							+ ", e.what(): " + e.what()
						);

						throw e;
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "generateFrames_ingestFrame failed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", externalEncoder: " + to_string(externalEncoder)
							+ ", imagesDirectory: " + imagesDirectory
							+ ", generatedFrameFileName: " + generatedFrameFileName
							+ ", addContentTitle: " + addContentTitle
							+ ", outputFileFormat: " + outputFileFormat
							+ ", e.what(): " + e.what()
						);

						throw e;
					}

					{
						_logger->info(__FILEREF__ + "remove"
							+ ", framePathName: " + imagesDirectory + "/" + generatedFrameFileName
						);
						bool exceptionInCaseOfError = false;
						FileIO::remove(imagesDirectory + "/" + generatedFrameFileName,
							exceptionInCaseOfError);
					}

					generatedFrameIndex++;
				}
				catch(DirectoryListFinished e)
				{
					scanDirectoryFinished = true;
				}
				catch(runtime_error e)
				{
					string errorMessage = __FILEREF__ + "listing directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					scanDirectoryFinished = true;

					completedWithError		= true;
					encoding->_errorMessage = errorMessage;

					// throw e;
				}
				catch(exception e)
				{
					string errorMessage = __FILEREF__ + "listing directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					scanDirectoryFinished = true;

					completedWithError		= true;
					encoding->_errorMessage = errorMessage;

					// throw e;
				}
			}

			FileIO::closeDirectory (directory);

			if (FileIO::directoryExisting(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", imagesDirectory: " + imagesDirectory);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
			}

			// wait the addContent to be executed
			try
			{
				string field = "mmsIngestionURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						// + ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string mmsIngestionURL = encodingParametersRoot.get(field, "").asString();

				int64_t userKey;
				string apiKey;
				{
					string field = "internalMMS";
					if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
					{
						Json::Value internalMMSRoot = ingestedParametersRoot[field];

						field = "credentials";
						if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
						{
							Json::Value credentialsRoot = internalMMSRoot[field];

							field = "userKey";
							userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

							field = "apiKey";
							string apiKeyEncrypted = credentialsRoot.get(field, "").asString();
							apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
						}
					}
				}

				chrono::system_clock::time_point startWaiting = chrono::system_clock::now();
				long maxSecondsWaiting = 2 * 60;

				while (addContentIngestionJobKeys.size() > 0
					&& chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startWaiting).count() < maxSecondsWaiting)
				{
					int64_t addContentIngestionJobKey = *(addContentIngestionJobKeys.begin());

					string mmsIngestionJobURL =
						mmsIngestionURL
						+ "/" + to_string(addContentIngestionJobKey)
						+ "?ingestionJobOutputs=false"
					;

					Json::Value ingestionRoot = MMSCURL::httpGetJson(
						ingestionJobKey,
						mmsIngestionJobURL,
						_mmsAPITimeoutInSeconds,
						to_string(userKey),
						apiKey,
						_logger);

					string field = "response";
					if (JSONUtils::isMetadataPresent(ingestionRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							// + ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					Json::Value responseRoot = ingestionRoot[field];

					field = "ingestionJobs";
					if (JSONUtils::isMetadataPresent(responseRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							// + ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					Json::Value ingestionJobsRoot = responseRoot[field];

					if (ingestionJobsRoot.size() != 1)
					{
						string errorMessage = __FILEREF__ + "Wrong ingestionJobs number"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							// + ", encodingJobKey: " + to_string(encodingJobKey)
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					Json::Value ingestionJobRoot = ingestionJobsRoot[0];

					field = "status";
					if (JSONUtils::isMetadataPresent(ingestionJobRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							// + ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string ingestionJobStatus = ingestionJobRoot.get(field, "").asString();

					string prefix = "End_";
					if (ingestionJobStatus.size() >= prefix.size()
						&& 0 == ingestionJobStatus.compare(0, prefix.size(), prefix))
					{
						_logger->info(__FILEREF__ + "addContentIngestionJobKey finished"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", addContentIngestionJobKey: " + to_string(addContentIngestionJobKey)
							+ ", ingestionJobStatus: " + ingestionJobStatus);

						addContentIngestionJobKeys.erase(addContentIngestionJobKeys.begin());
					}
					else
					{
						int secondsToSleep = 5;

						_logger->info(__FILEREF__ + "addContentIngestionJobKey not finished, sleeping..."
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", addContentIngestionJobKey: " + to_string(addContentIngestionJobKey)
							+ ", ingestionJobStatus: " + ingestionJobStatus
							+ ", secondsToSleep: " + to_string(secondsToSleep)
						);

						this_thread::sleep_for(chrono::seconds(secondsToSleep));
					}
				}
			}
			catch(runtime_error e)
			{
				string errorMessage = __FILEREF__ + "waiting addContent ingestion failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
				;
				_logger->error(errorMessage);
			}
		}

        _logger->info(__FILEREF__ + "generateFrames finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", completedWithError: " + to_string(completedWithError)
        );

		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

		if (externalEncoder)
		{
			if (imagesDirectory != "" && FileIO::directoryExisting(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", imagesDirectory: " + imagesDirectory);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		if (externalEncoder)
		{
			if (imagesDirectory != "" && FileIO::directoryExisting(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", imagesDirectory: " + imagesDirectory);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		if (externalEncoder)
		{
			if (imagesDirectory != "" && FileIO::directoryExisting(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", imagesDirectory: " + imagesDirectory);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
			}
		}

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
	}
}

int64_t FFMPEGEncoder::generateFrames_ingestFrame(
	int64_t ingestionJobKey,
	bool externalEncoder,
	string imagesDirectory, string generatedFrameFileName,
	string addContentTitle,
	Json::Value userDataRoot,
	string outputFileFormat,
	Json::Value ingestedParametersRoot,
	Json::Value encodingParametersRoot)
{
	string workflowMetadata;
	int64_t userKey;
	string apiKey;
	int64_t addContentIngestionJobKey = -1;
	string mmsWorkflowIngestionURL;
	// create the workflow and ingest it
	try
	{
		workflowMetadata = generateFrames_buildFrameIngestionWorkflow(
			ingestionJobKey,
			externalEncoder,
			generatedFrameFileName,
			imagesDirectory,
			addContentTitle,
			userDataRoot,
			outputFileFormat,
			ingestedParametersRoot,
			encodingParametersRoot
		);

		{
			string field = "internalMMS";
    		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				Json::Value internalMMSRoot = ingestedParametersRoot[field];

				field = "credentials";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					Json::Value credentialsRoot = internalMMSRoot[field];

					field = "userKey";
					userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

					field = "apiKey";
					string apiKeyEncrypted = credentialsRoot.get(field, "").asString();
					apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
				}
			}
		}

		{
			string field = "mmsWorkflowIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = encodingParametersRoot.get(field, "").asString();
		}

		string sResponse = MMSCURL::httpPostPutString(
			ingestionJobKey,
			mmsWorkflowIngestionURL,
			"POST",	// requestType
			_mmsAPITimeoutInSeconds,
			to_string(userKey),
			apiKey,
			workflowMetadata,
			"application/json",	// contentType
			_logger
		);

		addContentIngestionJobKey = getAddContentIngestionJobKey(ingestionJobKey, sResponse);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}

	if (addContentIngestionJobKey == -1)
	{
		string errorMessage =
			string("Ingested URL failed, addContentIngestionJobKey is not valid")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	string mmsBinaryURL;
	// ingest binary
	try
	{
		bool inCaseOfLinkHasItToBeRead = false;
		int64_t frameFileSize = FileIO::getFileSizeInBytes(
			imagesDirectory + "/" + generatedFrameFileName,
			inCaseOfLinkHasItToBeRead);

		string mmsBinaryIngestionURL;
		{
			string field = "mmsBinaryIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsBinaryIngestionURL = encodingParametersRoot.get(field, "").asString();
		}

		mmsBinaryURL =
			mmsBinaryIngestionURL
			+ "/" + to_string(addContentIngestionJobKey)
		;

		string sResponse = MMSCURL::httpPostPutFile(
			ingestionJobKey,
			mmsBinaryURL,
			"POST",	// requestType
			_mmsBinaryTimeoutInSeconds,
			to_string(userKey),
			apiKey,
			imagesDirectory + "/" + generatedFrameFileName,
			frameFileSize,
			_logger);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}

	return addContentIngestionJobKey;
}

string FFMPEGEncoder::generateFrames_buildFrameIngestionWorkflow(
	int64_t ingestionJobKey,
	bool externalEncoder,
	string generatedFrameFileName,
	string imagesDirectory,
	string addContentTitle,
	Json::Value userDataRoot,
	string outputFileFormat,
	Json::Value ingestedParametersRoot,
	Json::Value encodingParametersRoot
)
{
	string workflowMetadata;
	try
	{
		/*
		{
        	"Label": "<workflow label>",
        	"Type": "Workflow",
        	"Task": {
                "Label": "<task label 1>",
                "Type": "Add-Content"
                "Parameters": {
                        "FileFormat": "ts",
                        "Ingester": "Giuliano",
                        "SourceURL": "move:///abc...."
                },
        	}
		}
		*/
		// Json::Value mmsDataRoot = userDataRoot["mmsData"];
		// int64_t utcPreviousChunkStartTime = JSONUtils::asInt64(mmsDataRoot, "utcPreviousChunkStartTime", -1);
		// int64_t utcChunkStartTime = JSONUtils::asInt64(mmsDataRoot, "utcChunkStartTime", -1);
		// int64_t utcChunkEndTime = JSONUtils::asInt64(mmsDataRoot, "utcChunkEndTime", -1);

		Json::Value addContentRoot;

		string field = "Label";
		addContentRoot[field] = addContentTitle;

		field = "Type";
		addContentRoot[field] = "Add-Content";

		Json::Value addContentParametersRoot = ingestedParametersRoot;
		// if (internalMMSRootPresent)
		{
			Json::Value removed;
			field = "internalMMS";
			addContentParametersRoot.removeMember(field, &removed);
		}

		field = "FileFormat";
		addContentParametersRoot[field] = outputFileFormat;

		if (!externalEncoder)
		{
			string sourceURL = string("move") + "://" + imagesDirectory + "/" + generatedFrameFileName;
			field = "SourceURL";
			addContentParametersRoot[field] = sourceURL;
		}

		field = "Ingester";
		addContentParametersRoot[field] = "Generator Frames Task";

		field = "Title";
		addContentParametersRoot[field] = addContentTitle;

		field = "UserData";
		addContentParametersRoot[field] = userDataRoot;

		field = "Parameters";
		addContentRoot[field] = addContentParametersRoot;


		Json::Value workflowRoot;

		field = "Label";
		workflowRoot[field] = addContentTitle;

		field = "Type";
		workflowRoot[field] = "Workflow";

		/*
		{
			Json::Value variablesWorkflowRoot;

			{
				Json::Value variableWorkflowRoot;

				field = "Type";
				variableWorkflowRoot[field] = "integer";

				field = "Value";
				variableWorkflowRoot[field] = utcChunkStartTime;

				// name of the variable
				field = "CurrentUtcChunkStartTime";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			field = "Variables";
			workflowRoot[field] = variablesWorkflowRoot;
		}
		*/

		field = "Task";
		workflowRoot[field] = addContentRoot;

   		{
       		Json::StreamWriterBuilder wbuilder;
       		workflowMetadata = Json::writeString(wbuilder, workflowRoot);
   		}

		_logger->info(__FILEREF__ + "Frame Workflow metadata generated"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", " + addContentTitle + ", "
				+ generatedFrameFileName
		);

		return workflowMetadata;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "generateFrames_buildFrameIngestionWorkflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "generateFrames_buildFrameIngestionWorkflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

void FFMPEGEncoder::slideShowThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
    string api = "slideShow";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value slideShowMedatada;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &slideShowMedatada, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        
        int64_t ingestionJobKey = JSONUtils::asInt64(slideShowMedatada, "ingestionJobKey", -1);
        string videoSyncMethod = slideShowMedatada.get("videoSyncMethod", "vfr").asString();
        int outputFrameRate = JSONUtils::asInt(slideShowMedatada, "outputFrameRate", -1);
        string slideShowMediaPathName = slideShowMedatada.get("slideShowMediaPathName", "XXX").asString();

        vector<string> imagesSourcePhysicalPaths;
		{
			Json::Value sourcePhysicalPathsRoot(Json::arrayValue);
			sourcePhysicalPathsRoot = slideShowMedatada["imagesSourcePhysicalPaths"];
			for (int sourcePhysicalPathIndex = 0;
				sourcePhysicalPathIndex < sourcePhysicalPathsRoot.size();
				++sourcePhysicalPathIndex)
			{
				string sourcePhysicalPathName =
					sourcePhysicalPathsRoot.get(sourcePhysicalPathIndex, "").asString();

				imagesSourcePhysicalPaths.push_back(sourcePhysicalPathName);
			}
		}
        double durationOfEachSlideInSeconds = JSONUtils::asDouble(slideShowMedatada,
			"durationOfEachSlideInSeconds", 0);

        vector<string> audiosSourcePhysicalPaths;
		{
			Json::Value sourcePhysicalPathsRoot(Json::arrayValue);
			sourcePhysicalPathsRoot = slideShowMedatada["audiosSourcePhysicalPaths"];
			for (int sourcePhysicalPathIndex = 0;
				sourcePhysicalPathIndex < sourcePhysicalPathsRoot.size();
				++sourcePhysicalPathIndex)
			{
				string sourcePhysicalPathName =
					sourcePhysicalPathsRoot.get(sourcePhysicalPathIndex, "").asString();

				audiosSourcePhysicalPaths.push_back(sourcePhysicalPathName);
			}
		}
        double shortestAudioDurationInSeconds = JSONUtils::asDouble(slideShowMedatada,
			"shortestAudioDurationInSeconds", 0);

        encoding->_ffmpeg->generateSlideshowMediaToIngest(ingestionJobKey, encodingJobKey,
                imagesSourcePhysicalPaths, durationOfEachSlideInSeconds,
				audiosSourcePhysicalPaths, shortestAudioDurationInSeconds,
				videoSyncMethod, outputFrameRate, slideShowMediaPathName,
				&(encoding->_childPid));

        encoding->_running = false;
        encoding->_childPid = 0;
        
        _logger->info(__FILEREF__ + "slideShow finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
}

void FFMPEGEncoder::videoSpeedThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
    string api = "videoSpeed";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", requestBody: " + requestBody
	);

    try
    {
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value videoSpeedMetadata;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &videoSpeedMetadata, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string mmsSourceVideoAssetPathName = videoSpeedMetadata.get("mmsSourceVideoAssetPathName", "XXX").asString();
        int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(videoSpeedMetadata, "videoDurationInMilliSeconds", -1);

        string videoSpeedType = videoSpeedMetadata.get("videoSpeedType", "XXX").asString();
        int videoSpeedSize = JSONUtils::asInt(videoSpeedMetadata, "videoSpeedSize", 3);
        
        string stagingEncodedAssetPathName = videoSpeedMetadata.get("stagingEncodedAssetPathName", "XXX").asString();
        int64_t encodingJobKey = JSONUtils::asInt64(videoSpeedMetadata, "encodingJobKey", -1);
        int64_t ingestionJobKey = JSONUtils::asInt64(videoSpeedMetadata, "ingestionJobKey", -1);

		// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
        encoding->_ffmpeg->videoSpeed(
                mmsSourceVideoAssetPathName,
                videoDurationInMilliSeconds,

                videoSpeedType,
                videoSpeedSize,

                // encodedFileName,
                stagingEncodedAssetPathName,
                encodingJobKey,
                ingestionJobKey,
				&(encoding->_childPid));
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

//        string responseBody = string("{ ")
//                + "\"ingestionJobKey\": " + to_string(ingestionJobKey) + " "
//                + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
//                + "}";

        // sendSuccess(request, 200, responseBody);
        
        encoding->_running = false;
        encoding->_childPid = 0;
        
        _logger->info(__FILEREF__ + "Encode content finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::pictureInPictureThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
    string api = "pictureInPicture";

    _logger->info(__FILEREF__ + "Received " + api
                    + ", encodingJobKey: " + to_string(encodingJobKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value pictureInPictureMetadata;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &pictureInPictureMetadata, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string mmsMainVideoAssetPathName = pictureInPictureMetadata.get("mmsMainVideoAssetPathName", "XXX").asString();
        int64_t mainVideoDurationInMilliSeconds = JSONUtils::asInt64(pictureInPictureMetadata, "mainVideoDurationInMilliSeconds", -1);

        string mmsOverlayVideoAssetPathName = pictureInPictureMetadata.get("mmsOverlayVideoAssetPathName", "XXX").asString();
        int64_t overlayVideoDurationInMilliSeconds = JSONUtils::asInt64(pictureInPictureMetadata, "overlayVideoDurationInMilliSeconds", -1);

        bool soundOfMain = JSONUtils::asBool(pictureInPictureMetadata, "soundOfMain", false);

        string overlayPosition_X_InPixel = pictureInPictureMetadata.get("overlayPosition_X_InPixel", "XXX").asString();
        string overlayPosition_Y_InPixel = pictureInPictureMetadata.get("overlayPosition_Y_InPixel", "XXX").asString();
        string overlay_Width_InPixel = pictureInPictureMetadata.get("overlay_Width_InPixel", "XXX").asString();
        string overlay_Height_InPixel = pictureInPictureMetadata.get("overlay_Height_InPixel", "XXX").asString();
        
        string stagingEncodedAssetPathName = pictureInPictureMetadata.get("stagingEncodedAssetPathName", "XXX").asString();
        int64_t encodingJobKey = JSONUtils::asInt64(pictureInPictureMetadata, "encodingJobKey", -1);
        int64_t ingestionJobKey = JSONUtils::asInt64(pictureInPictureMetadata, "ingestionJobKey", -1);

		// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
        encoding->_ffmpeg->pictureInPicture(
                mmsMainVideoAssetPathName,
                mainVideoDurationInMilliSeconds,

                mmsOverlayVideoAssetPathName,
                overlayVideoDurationInMilliSeconds,

                soundOfMain,

                overlayPosition_X_InPixel,
                overlayPosition_Y_InPixel,
				overlay_Width_InPixel,
				overlay_Height_InPixel,

                // encodedFileName,
                stagingEncodedAssetPathName,
                encodingJobKey,
                ingestionJobKey,
				&(encoding->_childPid));
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

//        string responseBody = string("{ ")
//                + "\"ingestionJobKey\": " + to_string(ingestionJobKey) + " "
//                + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
//                + "}";

        // sendSuccess(request, 200, responseBody);
        
        encoding->_running = false;
        encoding->_childPid = 0;
        
        _logger->info(__FILEREF__ + "PictureInPicture encoding content finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::introOutroOverlayThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
    string api = "introOutroOverlay";

	_logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", requestBody: " + requestBody
	);

    try
    {
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value introOutroOverlayMetadata;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &introOutroOverlayMetadata, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

		string stagingEncodedAssetPathName =
			introOutroOverlayMetadata.get("stagingEncodedAssetPathName", "").asString();

        int64_t encodingJobKey = JSONUtils::asInt64(introOutroOverlayMetadata, "encodingJobKey", -1);
        int64_t ingestionJobKey = JSONUtils::asInt64(introOutroOverlayMetadata, "ingestionJobKey", -1);

		// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
		encoding->_ffmpeg->introOutroOverlay(
			introOutroOverlayMetadata["encodingParametersRoot"].get("introVideoAssetPathName", "").asString(),
			JSONUtils::asInt64(introOutroOverlayMetadata["encodingParametersRoot"], "introVideoDurationInMilliSeconds", -1),
			introOutroOverlayMetadata["encodingParametersRoot"].get("mainVideoAssetPathName", "").asString(),
			JSONUtils::asInt64(introOutroOverlayMetadata["encodingParametersRoot"], "mainVideoDurationInMilliSeconds", -1),
			introOutroOverlayMetadata["encodingParametersRoot"].get("outroVideoAssetPathName", "").asString(),
			JSONUtils::asInt64(introOutroOverlayMetadata["encodingParametersRoot"], "outroVideoDurationInMilliSeconds", -1),

			JSONUtils::asInt64(introOutroOverlayMetadata["ingestedParametersRoot"], "IntroOverlayDurationInSeconds", -1),
			JSONUtils::asInt64(introOutroOverlayMetadata["ingestedParametersRoot"], "OutroOverlayDurationInSeconds", -1),

			JSONUtils::asBool(introOutroOverlayMetadata["ingestedParametersRoot"], "MuteIntroOverlay", true),
			JSONUtils::asBool(introOutroOverlayMetadata["ingestedParametersRoot"], "MuteOutroOverlay", true),

			introOutroOverlayMetadata["encodingParametersRoot"]["encodingProfileDetailsRoot"],

			stagingEncodedAssetPathName,

			encodingJobKey,
			ingestionJobKey,
			&(encoding->_childPid));
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

        encoding->_running = false;
        encoding->_childPid = 0;

        _logger->info(__FILEREF__ + "introOutroOverlay encoding content finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::cutFrameAccurateThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
	string api = "cutFrameAccurate";

	_logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", requestBody: " + requestBody
	);

    try
    {
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

		Json::Value cutMetadata;
		try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &cutMetadata, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

		string stagingEncodedAssetPathName =
			cutMetadata.get("stagingEncodedAssetPathName", "").asString();

        int64_t encodingJobKey = JSONUtils::asInt64(cutMetadata, "encodingJobKey", -1);
        int64_t ingestionJobKey = JSONUtils::asInt64(cutMetadata, "ingestionJobKey", -1);

		encoding->_ffmpeg->cutFrameAccurateWithEncoding(
			ingestionJobKey,
			cutMetadata["encodingParametersRoot"].get("sourceVideoAssetPathName", "").asString(),
			encodingJobKey,
			cutMetadata["encodingParametersRoot"]["encodingProfileDetailsRoot"],
			JSONUtils::asDouble(cutMetadata["ingestedParametersRoot"], "StartTimeInSeconds", 0.0),
			JSONUtils::asDouble(cutMetadata["encodingParametersRoot"], "endTimeInSeconds", 0.0),
			JSONUtils::asInt(cutMetadata["ingestedParametersRoot"], "FramesNumber", -1),
			stagingEncodedAssetPathName,

			&(encoding->_childPid));
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

        encoding->_running = false;
        encoding->_childPid = 0;

        _logger->info(__FILEREF__ + "cut encoding content finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		encoding->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, encoding->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_encodingMutex);

			int erase = _encodingsCapability->erase(encoding->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_encodingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_encodingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::liveRecorderThread(
	// FCGX_Request& request,
	shared_ptr<LiveRecording> liveRecording,
	int64_t encodingJobKey,
	string requestBody)
{
	string api = "liveRecorder";

	_logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", requestBody: " + requestBody
	);

	string tvMulticastIP;
	string tvMulticastPort;
	string tvType;
	int64_t tvServiceId = -1;
	int64_t tvFrequency = -1;
	int64_t tvSymbolRate = -1;
	int64_t tvBandwidthInHz = -1;
	string tvModulation;
	int tvVideoPid = -1;
	int tvAudioItalianPid = -1;

    try
    {
        liveRecording->_killedBecauseOfNotWorking = false;
		liveRecording->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value liveRecorderMedatada;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &liveRecorderMedatada, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        liveRecording->_ingestionJobKey = JSONUtils::asInt64(liveRecorderMedatada,
			"ingestionJobKey", -1);

        liveRecording->_externalEncoder = JSONUtils::asBool(liveRecorderMedatada,
			"externalEncoder", false);

		// _transcoderStagingContentsPath is a transcoder LOCAL path,
		//		this is important because in case of high bitrate,
		//		nfs would not be enough fast and could create random file system error
        liveRecording->_transcoderStagingContentsPath =
			liveRecorderMedatada.get("transcoderStagingContentsPath", "").asString();
        string userAgent = liveRecorderMedatada.get("userAgent", "").asString();

		// this is the global shared path where the chunks would be moved for the ingestion
		// see the comments in EncoderVideoAudioProxy.cpp
        liveRecording->_stagingContentsPath =
			liveRecorderMedatada.get("stagingContentsPath", "").asString();
		// 2022-08-09: the stagingContentsPath directory was created by EncoderVideoAudioProxy.cpp
		// 		into the shared working area.
		// 		In case of an external encoder, the external working area does not have this directory
		// 		and the encoder will fail. For this reason, the directory is created if it does not exist
		// 2022-08-10: in case of an external encoder, the chunk has to be ingested
		//	as push, so the stagingContentsPath dir is not used at all
		//	For this reason the directory check is useless and it is commented
		/*
		if (!FileIO::directoryExisting(liveRecording->_stagingContentsPath))
		{
			bool noErrorIfExists = true;
			bool recursive = true;
			_logger->info(__FILEREF__ + "Creating directory"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
			);
			FileIO::createDirectory(liveRecording->_stagingContentsPath,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH,
				noErrorIfExists, recursive);
		}
		*/

        liveRecording->_segmentListFileName =
			liveRecorderMedatada.get("segmentListFileName", "").asString();
        liveRecording->_recordedFileNamePrefix =
			liveRecorderMedatada.get("recordedFileNamePrefix", "").asString();
		// see the comments in EncoderVideoAudioProxy.cpp
		if (liveRecording->_externalEncoder)
			liveRecording->_virtualVODStagingContentsPath =
				liveRecorderMedatada.get("virtualVODTranscoderStagingContentsPath", "").asString();
		else
			liveRecording->_virtualVODStagingContentsPath =
				liveRecorderMedatada.get("virtualVODStagingContentsPath", "").asString();
        liveRecording->_liveRecorderVirtualVODImageMediaItemKey =
			JSONUtils::asInt64(liveRecorderMedatada,
				"liveRecorderVirtualVODImageMediaItemKey", -1);

		// _encodingParametersRoot has to be the last field to be set because liveRecorderChunksIngestion()
		//		checks this field is set before to see if there are chunks to be ingested
		liveRecording->_encodingParametersRoot =
			liveRecorderMedatada["encodingParametersRoot"];
		liveRecording->_ingestedParametersRoot =
			liveRecorderMedatada["ingestedParametersRoot"];

        bool autoRenew = JSONUtils::asBool(liveRecording->_encodingParametersRoot,
			"autoRenew", false);

		liveRecording->_monitoringEnabled = JSONUtils::asBool(
			liveRecording->_ingestedParametersRoot, "monitoringEnabled", true);
		liveRecording->_monitoringFrameIncreasingEnabled = JSONUtils::asBool(
			liveRecording->_ingestedParametersRoot, "monitoringFrameIncreasingEnabled", true);

		liveRecording->_channelLabel = liveRecording->_ingestedParametersRoot.get(
			"ConfigurationLabel", "").asString();

		liveRecording->_lastRecordedAssetFileName			= "";
		liveRecording->_lastRecordedAssetDurationInSeconds	= 0.0;

        liveRecording->_streamSourceType = liveRecorderMedatada["encodingParametersRoot"].get(
			"streamSourceType", "IP_PULL").asString();
		int ipMMSAsServer_listenTimeoutInSeconds =
			liveRecorderMedatada["encodingParametersRoot"]
			.get("ActAsServerListenTimeout", 300).asInt();
		int pushListenTimeout = JSONUtils::asInt(
			liveRecorderMedatada["encodingParametersRoot"], "pushListenTimeout", -1);

		int captureLive_videoDeviceNumber = -1;
		string captureLive_videoInputFormat;
		int captureLive_frameRate = -1;
		int captureLive_width = -1;
		int captureLive_height = -1;
		int captureLive_audioDeviceNumber = -1;
		int captureLive_channelsNumber = -1;
		if (liveRecording->_streamSourceType == "CaptureLive")
		{
			captureLive_videoDeviceNumber = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"],
				"captureVideoDeviceNumber", -1);
			captureLive_videoInputFormat =
				liveRecorderMedatada["encodingParametersRoot"].
				get("captureVideoInputFormat", "").asString();
			captureLive_frameRate = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"], "captureFrameRate", -1);
			captureLive_width = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"], "captureWidth", -1);
			captureLive_height = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"], "captureHeight", -1);
			captureLive_audioDeviceNumber = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"],
				"captureAudioDeviceNumber", -1);
			captureLive_channelsNumber = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"],
				"captureChannelsNumber", -1);
		}

        string liveURL;

		if (liveRecording->_streamSourceType == "TV")
		{
			tvType = liveRecorderMedatada["encodingParametersRoot"].
				get("tvType", "").asString();
			tvServiceId = JSONUtils::asInt64(
				liveRecorderMedatada["encodingParametersRoot"],
				"tvServiceId", -1);
			tvFrequency = JSONUtils::asInt64(
				liveRecorderMedatada["encodingParametersRoot"],
				"tvFrequency", -1);
			tvSymbolRate = JSONUtils::asInt64(
				liveRecorderMedatada["encodingParametersRoot"],
				"tvSymbolRate", -1);
			tvBandwidthInHz = JSONUtils::asInt64(
				liveRecorderMedatada["encodingParametersRoot"],
				"tvBandwidthInHz", -1);
			tvModulation = liveRecorderMedatada["encodingParametersRoot"].
				get("tvModulation", "").asString();
			tvVideoPid = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"], "tvVideoPid", -1);
			tvAudioItalianPid = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"],
				"tvAudioItalianPid", -1);

			// In case ffmpeg crashes and is automatically restarted, it should use the same
			// IP-PORT it was using before because we already have a dbvlast sending the stream
			// to the specified IP-PORT.
			// For this reason, before to generate a new IP-PORT, let's look for the serviceId
			// inside the dvblast conf. file to see if it was already running before

			pair<string, string> tvMulticast =
				getTVMulticastFromDvblastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation);
			tie(tvMulticastIP, tvMulticastPort) = tvMulticast;

			if (tvMulticastIP == "")
			{
				lock_guard<mutex> locker(*_tvChannelsPortsMutex);

				tvMulticastIP = "239.255.1.1";
				tvMulticastPort = to_string(*_tvChannelPort_CurrentOffset
					+ _tvChannelPort_Start);

				*_tvChannelPort_CurrentOffset =
					(*_tvChannelPort_CurrentOffset + 1)
					% _tvChannelPort_MaxNumberOfOffsets;
			}

			// overrun_nonfatal=1 prevents ffmpeg from exiting,
			//		it can recover in most circumstances.
			// fifo_size=50000000 uses a 50MB udp input buffer (default 5MB)
			liveURL = string("udp://@") + tvMulticastIP
				+ ":" + tvMulticastPort
				+ "?overrun_nonfatal=1&fifo_size=50000000"
			;

			createOrUpdateTVDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				true);
		}
		else 
		{
			// in case of actAsServer
			//	true: it is set into the MMSEngineProcessor::manageLiveRecorder method
			//	false: it comes from the LiveRecorder json ingested
			liveURL = liveRecorderMedatada.get("liveURL", "").asString();
		}

        time_t utcRecordingPeriodStart = JSONUtils::asInt64(
			liveRecording->_encodingParametersRoot,
			"utcScheduleStart", -1);
        time_t utcRecordingPeriodEnd = JSONUtils::asInt64(
			liveRecording->_encodingParametersRoot,
			"utcScheduleEnd", -1);
        int segmentDurationInSeconds = JSONUtils::asInt(
			liveRecording->_encodingParametersRoot,
			"segmentDurationInSeconds", -1);
        string outputFileFormat = liveRecording->_encodingParametersRoot
			.get("outputFileFormat", "").asString();

		{
			bool monitorHLS = JSONUtils::asBool(liveRecording->_encodingParametersRoot,
				"monitorHLS", false);
			liveRecording->_virtualVOD = JSONUtils::asBool(
				liveRecording->_encodingParametersRoot,
				"liveRecorderVirtualVOD", false);

			if (monitorHLS || liveRecording->_virtualVOD)
			{
				// see the comments in EncoderVideoAudioProxy.cpp
				liveRecording->_monitorVirtualVODManifestDirectoryPath =
					liveRecording->_encodingParametersRoot
					.get("monitorManifestDirectoryPath", "").asString();
				liveRecording->_monitorVirtualVODManifestFileName =
					liveRecording->_encodingParametersRoot
					.get("monitorManifestFileName", "").asString();
			}
		}

		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath
			+ liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath
				+ liveRecording->_segmentListFileName,
				exceptionInCaseOfError);
		}

		// since the first chunk is discarded, we will start recording before the period of the chunk
		// In case of autorenew, when it is renewed, we will lose the first chunk
		// utcRecordingPeriodStart -= segmentDurationInSeconds;
		// 2019-12-19: the above problem is managed inside _ffmpeg->liveRecorder
		//		(see the secondsToStartEarly variable inside _ffmpeg->liveRecorder)
		//		For this reason the above decrement was commented

		// based on liveProxy->_proxyStart, the monitor thread starts the checkings
		// In case of IP_PUSH, the checks should be done after the ffmpeg server
		// receives the stream and we do not know what it happens.
		// For this reason, in this scenario, we have to set _proxyStart in the worst scenario
		if (liveRecording->_streamSourceType == "IP_PUSH")
		{
			if (chrono::system_clock::from_time_t(
					utcRecordingPeriodStart) < chrono::system_clock::now())
				liveRecording->_recordingStart = chrono::system_clock::now() +
					chrono::seconds(ipMMSAsServer_listenTimeoutInSeconds);
			else
				liveRecording->_recordingStart = chrono::system_clock::from_time_t(
					utcRecordingPeriodStart) +
					chrono::seconds(ipMMSAsServer_listenTimeoutInSeconds);
		}
		else
		{
			if (chrono::system_clock::from_time_t(utcRecordingPeriodStart)
					< chrono::system_clock::now())
				liveRecording->_recordingStart = chrono::system_clock::now();
			else
				liveRecording->_recordingStart = chrono::system_clock::from_time_t(
					utcRecordingPeriodStart);
		}

		Json::Value outputsRoot = liveRecording->_encodingParametersRoot["outputsRoot"];

		// liveRecording->_liveRecorderOutputRoots.clear();
		{
			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = outputRoot.get("outputType", "").asString();
				string manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "").
					asString();

				if (outputType == "HLS" || outputType == "DASH")
				{
					if (FileIO::directoryExisting(manifestDirectoryPath))
					{
						try
						{
							_logger->info(__FILEREF__ + "removeDirectory"
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
							);
							Boolean_t bRemoveRecursively = true;
							FileIO::removeDirectory(manifestDirectoryPath,
								bRemoveRecursively);
						}
						catch(runtime_error e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: "
									+ to_string(liveRecording->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
						catch(exception e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: "
									+ to_string(liveRecording->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
					}
				}
			}
		}

		Json::Value framesToBeDetectedRoot = liveRecording->_encodingParametersRoot[
			"framesToBeDetectedRoot"];

		string otherInputOptions = liveRecording->_ingestedParametersRoot.get(
			"otherInputOptions", "").asString();

		liveRecording->_segmenterType = "hlsSegmenter";
		// liveRecording->_segmenterType = "streamSegmenter";

		_logger->info(__FILEREF__ + "liveRecorder. _ffmpeg->liveRecorder"
			+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", streamSourceType: " + liveRecording->_streamSourceType
			+ ", liveURL: " + liveURL
		);
		liveRecording->_ffmpeg->liveRecorder(
			liveRecording->_ingestionJobKey,
			encodingJobKey,
			liveRecording->_externalEncoder,
			liveRecording->_transcoderStagingContentsPath
				+ liveRecording->_segmentListFileName,
			liveRecording->_recordedFileNamePrefix,

			otherInputOptions,

			liveRecording->_streamSourceType,
			StringUtils::trimTabToo(liveURL),
			pushListenTimeout,
			captureLive_videoDeviceNumber,
			captureLive_videoInputFormat,
			captureLive_frameRate,
			captureLive_width,
			captureLive_height,
			captureLive_audioDeviceNumber,
			captureLive_channelsNumber,

			userAgent,
			utcRecordingPeriodStart,
			utcRecordingPeriodEnd,

			segmentDurationInSeconds,
			outputFileFormat,
			liveRecording->_segmenterType,

			outputsRoot,

			framesToBeDetectedRoot,

			&(liveRecording->_childPid),
			&(liveRecording->_recordingStart)
		);

		// 2022-11-20: _running to false has to be set soon to avoid monitoring
        liveRecording->_running = false;
        liveRecording->_childPid = 0;

		if (liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

		if (!autoRenew)
		{
			// to wait the ingestion of the last chunk
			this_thread::sleep_for(chrono::seconds(
				2 * _liveRecorderChunksIngestionCheckInSeconds));
		}

		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_killedBecauseOfNotWorking = false;
        
        _logger->info(__FILEREF__ + "liveRecorded finished"
            + ", liveRecording->_ingestionJobKey: "
				+ to_string(liveRecording->_ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );

        liveRecording->_ingestionJobKey		= 0;
		liveRecording->_channelLabel		= "";
		// liveRecording->_liveRecorderOutputRoots.clear();

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
			completedWithError, liveRecording->_errorMessage, killedByUser,
			urlForbidden, urlNotFound);

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath
			+ liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath
				+ liveRecording->_segmentListFileName,
				exceptionInCaseOfError);
		}

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			int erase = _liveRecordingsCapability->erase(liveRecording->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveRecordingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__
					+ "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		if (liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		// liveRecording->_liveRecorderOutputRoots.clear();

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser;
		if (liveRecording->_killedBecauseOfNotWorking)
		{
			// it was killed just because it was not working and not because of user
			// In this case the process has to be restarted soon
			killedByUser				= false;
			completedWithError			= true;
			liveRecording->_killedBecauseOfNotWorking = false;
		}
		else
		{
			killedByUser                = true;
		}
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveRecording->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath
				+ liveRecording->_segmentListFileName, exceptionInCaseOfError);
		}

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			int erase = _liveRecordingsCapability->erase(liveRecording->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveRecordingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__
					+ "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(FFMpegURLForbidden e)
    {
		if (liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;
		// liveRecording->_liveRecorderOutputRoots.clear();

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLForbidden)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveRecording->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= true;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
			completedWithError, liveRecording->_errorMessage, killedByUser,
			urlForbidden, urlNotFound);

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath
			+ liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath
				+ liveRecording->_segmentListFileName,
				exceptionInCaseOfError);
		}

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			int erase = _liveRecordingsCapability->erase(liveRecording->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveRecordingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__
					+ "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(FFMpegURLNotFound e)
    {
		if (liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;
		// liveRecording->_liveRecorderOutputRoots.clear();

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLNotFound)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveRecording->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= true;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveRecording->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath
			+ liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath
				+ liveRecording->_segmentListFileName,
				exceptionInCaseOfError);
		}

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			int erase = _liveRecordingsCapability->erase(liveRecording->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveRecordingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__
					+ "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
		if (liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;
		// liveRecording->_liveRecorderOutputRoots.clear();

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveRecording->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveRecording->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath
			+ liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath
				+ liveRecording->_segmentListFileName,
				exceptionInCaseOfError);
		}

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			int erase = _liveRecordingsCapability->erase(liveRecording->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveRecordingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__
					+ "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(exception e)
    {
		if (liveRecording->_streamSourceType == "TV"
			&& tvServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateTVDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				tvMulticastIP, tvMulticastPort,
				tvType, tvServiceId, tvFrequency, tvSymbolRate,
				tvBandwidthInHz / 1000000,
				tvModulation, tvVideoPid, tvAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;
		// liveRecording->_liveRecorderOutputRoots.clear();

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveRecording->_errorMessage = errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveRecording->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		// here we have the deletion of the segments directory
		// The monitor directory was removed inside the ffmpeg method
		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath
			+ liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: "
					+ liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath
				+ liveRecording->_segmentListFileName,
				exceptionInCaseOfError);
		}

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			int erase = _liveRecordingsCapability->erase(liveRecording->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveRecordingsCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__
					+ "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
}

void FFMPEGEncoder::liveRecorderChunksIngestionThread()
{

	while(!_liveRecorderChunksIngestionThreadShutdown)
	{
		try
		{
			chrono::system_clock::time_point startAllChannelsIngestionChunks = chrono::system_clock::now();

			lock_guard<mutex> locker(*_liveRecordingMutex);

			#ifdef __VECTOR__
			for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
			#else	// __MAP__
			for(map<int64_t, shared_ptr<LiveRecording>>::iterator it = _liveRecordingsCapability->begin();
				it != _liveRecordingsCapability->end(); it++)
			#endif
			{
				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<LiveRecording> liveRecording = it->second;
				#endif

				if (liveRecording->_running)
				{
					_logger->info(__FILEREF__ + "liveRecorder_processSegmenterOutput ..."
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
						+ ", liveRecording->_segmenterType: " + liveRecording->_segmenterType
					);

					chrono::system_clock::time_point startSingleChannelIngestionChunks = chrono::system_clock::now();

					try
					{
						if (liveRecording->_encodingParametersRoot != Json::nullValue)
						{
							// bool highAvailability;
							// bool main;
							int segmentDurationInSeconds;
							string outputFileFormat;
							{
								// string field = "highAvailability";
								// highAvailability = JSONUtils::asBool(liveRecording->_encodingParametersRoot, field, false);

								// field = "main";
								// main = JSONUtils::asBool(liveRecording->_encodingParametersRoot, field, false);

								string field = "segmentDurationInSeconds";
								segmentDurationInSeconds = JSONUtils::asInt(liveRecording->_encodingParametersRoot, field, 0);

								field = "outputFileFormat";                                                                
								outputFileFormat = liveRecording->_encodingParametersRoot.get(field, "XXX").asString();                   
							}

							pair<string, int> lastRecordedAssetInfo;

							if (liveRecording->_segmenterType == "streamSegmenter")
							{
								lastRecordedAssetInfo = liveRecorder_processStreamSegmenterOutput(
									liveRecording->_ingestionJobKey,
									liveRecording->_encodingJobKey,
									liveRecording->_streamSourceType,
									liveRecording->_externalEncoder,
									segmentDurationInSeconds, outputFileFormat,                                                                              
									liveRecording->_encodingParametersRoot,
									liveRecording->_ingestedParametersRoot,

									liveRecording->_transcoderStagingContentsPath,
									liveRecording->_stagingContentsPath,
									liveRecording->_segmentListFileName,
									liveRecording->_recordedFileNamePrefix,
									liveRecording->_lastRecordedAssetFileName,
									liveRecording->_lastRecordedAssetDurationInSeconds);
							}
							else // if (liveRecording->_segmenterType == "hlsSegmenter")
							{
								lastRecordedAssetInfo = liveRecorder_processHLSSegmenterOutput(
									liveRecording->_ingestionJobKey,
									liveRecording->_encodingJobKey,
									liveRecording->_streamSourceType,
									liveRecording->_externalEncoder,
									segmentDurationInSeconds, outputFileFormat,                                                                              
									liveRecording->_encodingParametersRoot,
									liveRecording->_ingestedParametersRoot,

									liveRecording->_transcoderStagingContentsPath,
									liveRecording->_stagingContentsPath,
									liveRecording->_segmentListFileName,
									liveRecording->_recordedFileNamePrefix,
									liveRecording->_lastRecordedAssetFileName,
									liveRecording->_lastRecordedAssetDurationInSeconds);
							}

							liveRecording->_lastRecordedAssetFileName			= lastRecordedAssetInfo.first;
							liveRecording->_lastRecordedAssetDurationInSeconds	= lastRecordedAssetInfo.second;
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveRecorder_processSegmenterOutput failed")
							+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveRecorder_processSegmenterOutput failed")
							+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}

					_logger->info(__FILEREF__ + "Single Channel Ingestion Chunks"
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
						+ ", @MMS statistics@ - elapsed time: @" + to_string(
							chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
								- startSingleChannelIngestionChunks).count()
						) + "@"
					);
				}
			}

			_logger->info(__FILEREF__ + "All Channels Ingestion Chunks"
				+ ", @MMS statistics@ - elapsed time: @" + to_string(
					chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
						- startAllChannelsIngestionChunks).count()
				) + "@"
			);
		}
		catch(runtime_error e)
		{
			string errorMessage = string ("liveRecorderChunksIngestion failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("liveRecorderChunksIngestion failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}

		this_thread::sleep_for(chrono::seconds(_liveRecorderChunksIngestionCheckInSeconds));
	}
}

void FFMPEGEncoder::stopLiveRecorderChunksIngestionThread()
{
	_liveRecorderChunksIngestionThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_liveRecorderChunksIngestionCheckInSeconds));
}

void FFMPEGEncoder::liveRecorderVirtualVODIngestionThread()
{

	while(!_liveRecorderVirtualVODIngestionThreadShutdown)
	{
		int virtualVODsNumber = 0;

		try
		{
			// this is to have a copy of LiveRecording
			vector<shared_ptr<LiveRecording>> copiedRunningLiveRecordingCapability;

			// this is to have access to _running and _proxyStart
			//	to check if it is changed. In case the process is killed, it will access
			//	also to _killedBecauseOfNotWorking and _errorMessage
			vector<shared_ptr<LiveRecording>> sourceLiveRecordingCapability;

			chrono::system_clock::time_point startClone = chrono::system_clock::now();
			// to avoid to maintain the lock too much time
			// we will clone the proxies for monitoring check
			int liveRecordingVirtualVODCounter = 0;
			{
				lock_guard<mutex> locker(*_liveRecordingMutex);

				int liveRecordingNotVirtualVODCounter = 0;

				#ifdef __VECTOR__
				for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
				#else	// __MAP__
				for(map<int64_t, shared_ptr<LiveRecording>>::iterator it =
					_liveRecordingsCapability->begin(); it != _liveRecordingsCapability->end(); it++)
				#endif
				{
					#ifdef __VECTOR__
					#else	// __MAP__
					shared_ptr<LiveRecording> liveRecording = it->second;
					#endif

					if (liveRecording->_running && liveRecording->_virtualVOD
						&& startClone > liveRecording->_recordingStart)
					{
						liveRecordingVirtualVODCounter++;

						copiedRunningLiveRecordingCapability.push_back(
							liveRecording->cloneForMonitorAndVirtualVOD());
						sourceLiveRecordingCapability.push_back(
                            liveRecording);
					}
					else
					{
						liveRecordingNotVirtualVODCounter++;
					}
				}
				_logger->info(__FILEREF__ + "virtualVOD, numbers"
					+ ", total LiveRecording: " + to_string(liveRecordingVirtualVODCounter
						+ liveRecordingNotVirtualVODCounter)
					+ ", liveRecordingVirtualVODCounter: " + to_string(liveRecordingVirtualVODCounter)
					+ ", liveRecordingNotVirtualVODCounter: " + to_string(liveRecordingNotVirtualVODCounter)
				);
			}
			_logger->info(__FILEREF__ + "virtualVOD clone"
				+ ", copiedRunningLiveRecordingCapability.size: " + to_string(copiedRunningLiveRecordingCapability.size())
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - startClone).count())
			);

			chrono::system_clock::time_point startAllChannelsVirtualVOD = chrono::system_clock::now();

			for (int liveRecordingIndex = 0;
				liveRecordingIndex < copiedRunningLiveRecordingCapability.size();
				liveRecordingIndex++)
			{
				shared_ptr<LiveRecording> copiedLiveRecording
					= copiedRunningLiveRecordingCapability[liveRecordingIndex];
				shared_ptr<LiveRecording> sourceLiveRecording
					= sourceLiveRecordingCapability[liveRecordingIndex];

				_logger->info(__FILEREF__ + "virtualVOD"
					+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
					+ ", channelLabel: " + copiedLiveRecording->_channelLabel
				);

				if (!sourceLiveRecording->_running ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "virtualVOD. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_running: " + to_string(sourceLiveRecording->_running)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): "
							+ to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): "
							+ to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				{
					chrono::system_clock::time_point startSingleChannelVirtualVOD = chrono::system_clock::now();

					virtualVODsNumber++;

					_logger->info(__FILEREF__ + "liveRecorder_buildAndIngestVirtualVOD ..."
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", externalEncoder: " + to_string(copiedLiveRecording->_externalEncoder)
						+ ", running: " + to_string(copiedLiveRecording->_running)
						+ ", virtualVOD: " + to_string(copiedLiveRecording->_virtualVOD)
						+ ", virtualVODsNumber: " + to_string(virtualVODsNumber)
						+ ", monitorVirtualVODManifestDirectoryPath: "
							+ copiedLiveRecording->_monitorVirtualVODManifestDirectoryPath
						+ ", monitorVirtualVODManifestFileName: "
							+ copiedLiveRecording->_monitorVirtualVODManifestFileName
						+ ", virtualVODStagingContentsPath: "
							+ copiedLiveRecording->_virtualVODStagingContentsPath
					);

					long segmentsNumber = 0;

					try
					{
						int64_t deliveryCode = JSONUtils::asInt64(
							copiedLiveRecording->_ingestedParametersRoot, "DeliveryCode", 0);
						string ingestionJobLabel = copiedLiveRecording->_encodingParametersRoot
							.get("ingestionJobLabel", "").asString();
						string liveRecorderVirtualVODUniqueName = ingestionJobLabel + "("
							+ to_string(deliveryCode) + "_" + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ")";

						int64_t userKey;
						string apiKey;
						{
							string field = "internalMMS";
							if (JSONUtils::isMetadataPresent(copiedLiveRecording->_ingestedParametersRoot, field))
							{
								Json::Value internalMMSRoot = copiedLiveRecording->_ingestedParametersRoot[field];

								field = "credentials";
								if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
								{
									Json::Value credentialsRoot = internalMMSRoot[field];

									field = "userKey";
									userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

									field = "apiKey";
									string apiKeyEncrypted = credentialsRoot.get(field, "").asString();
									apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
								}
							}
						}

						string mmsWorkflowIngestionURL;
						string mmsBinaryIngestionURL;
						{
							string field = "mmsWorkflowIngestionURL";
							if (!JSONUtils::isMetadataPresent(copiedLiveRecording->_encodingParametersRoot,
								field))
							{
								string errorMessage = __FILEREF__ + "Field is not present or it is null"
									+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", Field: " + field;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
							mmsWorkflowIngestionURL = copiedLiveRecording->_encodingParametersRoot.get(field, "").asString();

							field = "mmsBinaryIngestionURL";
							if (!JSONUtils::isMetadataPresent(copiedLiveRecording->_encodingParametersRoot,
								field))
							{
								string errorMessage = __FILEREF__ + "Field is not present or it is null"
									+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", Field: " + field;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
							mmsBinaryIngestionURL = copiedLiveRecording->_encodingParametersRoot.get(field, "").asString();
						}

						segmentsNumber = liveRecorder_buildAndIngestVirtualVOD(
							copiedLiveRecording->_ingestionJobKey,
							copiedLiveRecording->_encodingJobKey,
							copiedLiveRecording->_externalEncoder,

							copiedLiveRecording->_monitorVirtualVODManifestDirectoryPath,
							copiedLiveRecording->_monitorVirtualVODManifestFileName,
							copiedLiveRecording->_virtualVODStagingContentsPath,

							deliveryCode,
							ingestionJobLabel,
							liveRecorderVirtualVODUniqueName,
							_liveRecorderVirtualVODRetention,
							copiedLiveRecording->_liveRecorderVirtualVODImageMediaItemKey,
							userKey,
							apiKey,
							mmsWorkflowIngestionURL,
							mmsBinaryIngestionURL);
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveRecorder_buildAndIngestVirtualVOD failed")
							+ ", copiedLiveRecording->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", copiedLiveRecording->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveRecorder_buildAndIngestVirtualVOD failed")
							+ ", copiedLiveRecording->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", copiedLiveRecording->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}

					_logger->info(__FILEREF__ + "Single Channel Virtual VOD"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", segmentsNumber: " + to_string(segmentsNumber)
						+ ", @MMS statistics@ - elapsed time (secs): @" + to_string(
							chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
								- startSingleChannelVirtualVOD).count()
						) + "@"
					);
				}
			}

			_logger->info(__FILEREF__ + "All Channels Virtual VOD"
				+ ", @MMS statistics@ - elapsed time: @" + to_string(
					chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
						- startAllChannelsVirtualVOD).count()
				) + "@"
			);
		}
		catch(runtime_error e)
		{
			string errorMessage = string ("liveRecorderVirtualVODIngestion failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("liveRecorderVirtualVODIngestion failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}

		if (virtualVODsNumber < 5)
			this_thread::sleep_for(chrono::seconds(_liveRecorderVirtualVODIngestionInSeconds * 2));
		else
			this_thread::sleep_for(chrono::seconds(_liveRecorderVirtualVODIngestionInSeconds));
	}
}

void FFMPEGEncoder::stopLiveRecorderVirtualVODIngestionThread()
{
	_liveRecorderVirtualVODIngestionThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_liveRecorderVirtualVODIngestionInSeconds));
}

pair<string, double> FFMPEGEncoder::liveRecorder_processStreamSegmenterOutput(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	string streamSourceType,
	bool externalEncoder,
	int segmentDurationInSeconds, string outputFileFormat,
	Json::Value encodingParametersRoot,
	Json::Value ingestedParametersRoot,
	string transcoderStagingContentsPath,
	string stagingContentsPath,
	string segmentListFileName,
	string recordedFileNamePrefix,
	string lastRecordedAssetFileName,
	double lastRecordedAssetDurationInSeconds)
{

	// it is assigned to lastRecordedAssetFileName because in case no new files are present,
	// the same lastRecordedAssetFileName has to be returned
	string newLastRecordedAssetFileName = lastRecordedAssetFileName;
	double newLastRecordedAssetDurationInSeconds = lastRecordedAssetDurationInSeconds;
    try
    {
		_logger->info(__FILEREF__ + "liveRecorder_processStreamSegmenterOutput"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			// + ", highAvailability: " + to_string(highAvailability)
			// + ", main: " + to_string(main)
			+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
			+ ", outputFileFormat: " + outputFileFormat
			+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
			+ ", stagingContentsPath: " + stagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
			+ ", recordedFileNamePrefix: " + recordedFileNamePrefix
			+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName
			+ ", lastRecordedAssetDurationInSeconds: " + to_string(lastRecordedAssetDurationInSeconds)
		);

		ifstream segmentList(transcoderStagingContentsPath + segmentListFileName);
		if (!segmentList)
        {
            string errorMessage = __FILEREF__ + "No segment list file found yet"
				+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
				+ ", segmentListFileName: " + segmentListFileName
				+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName;
            _logger->warn(errorMessage);

			return make_pair(lastRecordedAssetFileName, lastRecordedAssetDurationInSeconds);
            // throw runtime_error(errorMessage);
        }

		bool reachedNextFileToProcess = false;
		string currentRecordedAssetFileName;
		while(getline(segmentList, currentRecordedAssetFileName))
		{
			if (!reachedNextFileToProcess)
			{
				if (lastRecordedAssetFileName == "")
				{
					reachedNextFileToProcess = true;
				}
				else if (currentRecordedAssetFileName == lastRecordedAssetFileName)
				{
					reachedNextFileToProcess = true;

					continue;
				}
				else
				{
					continue;
				}
			}

			_logger->info(__FILEREF__ + "processing LiveRecorder file"
				+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName);

			if (!FileIO::fileExisting(transcoderStagingContentsPath + currentRecordedAssetFileName))
			{
				// it could be the scenario where mmsEngineService is restarted,
				// the segments list file still contains obsolete filenames
				_logger->error(__FILEREF__ + "file not existing"
						", currentRecordedAssetPathName: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				);

				continue;
			}

			bool isFirstChunk = (lastRecordedAssetFileName == "");

			time_t utcCurrentRecordedFileCreationTime = liveRecorder_getMediaLiveRecorderStartTime(
				ingestionJobKey, encodingJobKey, currentRecordedAssetFileName, segmentDurationInSeconds,
				isFirstChunk);

			/*
			time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
			if (utcNow - utcCurrentRecordedFileCreationTime < _secondsToWaitNFSBuffers)
			{
				long secondsToWait = _secondsToWaitNFSBuffers
					- (utcNow - utcCurrentRecordedFileCreationTime);

				_logger->info(__FILEREF__ + "processing LiveRecorder file too young"
					+ ", secondsToWait: " + to_string(secondsToWait));
				this_thread::sleep_for(chrono::seconds(secondsToWait));
			}
			*/

			bool ingestionRowToBeUpdatedAsSuccess = liveRecorder_isLastLiveRecorderFile(
					ingestionJobKey, encodingJobKey, utcCurrentRecordedFileCreationTime,
					transcoderStagingContentsPath, recordedFileNamePrefix,
					segmentDurationInSeconds, isFirstChunk);
			_logger->info(__FILEREF__ + "liveRecorder_isLastLiveRecorderFile"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
					+ ", recordedFileNamePrefix: " + recordedFileNamePrefix
					+ ", ingestionRowToBeUpdatedAsSuccess: " + to_string(ingestionRowToBeUpdatedAsSuccess));

			newLastRecordedAssetFileName = currentRecordedAssetFileName;
			newLastRecordedAssetDurationInSeconds = segmentDurationInSeconds;

			/*
			 * 2019-10-17: we just saw that, even if the real duration is 59 seconds,
			 * next utc time inside the chunk file name is still like +60 from the previuos chunk utc.
			 * For this reason next code was commented.
			try
			{
				int64_t durationInMilliSeconds;


				_logger->info(__FILEREF__ + "Calling ffmpeg.getMediaInfo"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", transcoderStagingContentsPath + currentRecordedAssetFileName: "
						+ (transcoderStagingContentsPath + currentRecordedAssetFileName)
				);
				FFMpeg ffmpeg (_configuration, _logger);
				tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> mediaInfo =
					ffmpeg.getMediaInfo(transcoderStagingContentsPath + currentRecordedAssetFileName);

				tie(durationInMilliSeconds, ignore,
					ignore, ignore, ignore, ignore, ignore, ignore,
					ignore, ignore, ignore, ignore) = mediaInfo;

				newLastRecordedAssetDurationInSeconds = durationInMilliSeconds / 1000;

				if (newLastRecordedAssetDurationInSeconds != segmentDurationInSeconds)
				{
					_logger->warn(__FILEREF__ + "segment duration is different from file duration"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", durationInMilliSeconds: " + to_string (durationInMilliSeconds)
						+ ", newLastRecordedAssetDurationInSeconds: "
							+ to_string (newLastRecordedAssetDurationInSeconds)
						+ ", segmentDurationInSeconds: " + to_string (segmentDurationInSeconds)
					);
				}
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "ffmpeg.getMediaInfo failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", transcoderStagingContentsPath + currentRecordedAssetFileName: "
						+ (transcoderStagingContentsPath + currentRecordedAssetFileName)
				);
			}
			*/

			time_t utcCurrentRecordedFileLastModificationTime =
				utcCurrentRecordedFileCreationTime + newLastRecordedAssetDurationInSeconds;
			/*
			time_t utcCurrentRecordedFileLastModificationTime = getMediaLiveRecorderEndTime(
				currentRecordedAssetPathName);
			*/

			string uniqueName;
			{
				int64_t deliveryCode = JSONUtils::asInt64(ingestedParametersRoot, "DeliveryCode", 0);

				uniqueName = to_string(deliveryCode);
				uniqueName += " - ";
				uniqueName += to_string(utcCurrentRecordedFileCreationTime);
			}

			string ingestionJobLabel = encodingParametersRoot.get("ingestionJobLabel", "").asString();

			// UserData
			Json::Value userDataRoot;
			{
				if (JSONUtils::isMetadataPresent(ingestedParametersRoot, "UserData"))
					userDataRoot = ingestedParametersRoot["UserData"];

				Json::Value mmsDataRoot;
				mmsDataRoot["dataType"] = "liveRecordingChunk";
				/*
				mmsDataRoot["streamSourceType"] = streamSourceType;
				if (streamSourceType == "IP_PULL")
					mmsDataRoot["ipConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
				else if (streamSourceType == "TV")
					mmsDataRoot["satConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
				else // if (streamSourceType == "IP_PUSH")
				*/
				{
					int64_t deliveryCode = JSONUtils::asInt64(ingestedParametersRoot,
						"DeliveryCode", 0);
					mmsDataRoot["deliveryCode"] = deliveryCode;
				}
				mmsDataRoot["ingestionJobLabel"] = ingestionJobLabel;
				// mmsDataRoot["main"] = main;
				mmsDataRoot["main"] = true;
				// if (!highAvailability)
				{
					bool validated = true;
					mmsDataRoot["validated"] = validated;
				}
				mmsDataRoot["ingestionJobKey"] = (int64_t) (ingestionJobKey);
				mmsDataRoot["utcPreviousChunkStartTime"] =
					(time_t) (utcCurrentRecordedFileCreationTime - lastRecordedAssetDurationInSeconds);
				mmsDataRoot["utcChunkStartTime"] = utcCurrentRecordedFileCreationTime;
				mmsDataRoot["utcChunkEndTime"] = utcCurrentRecordedFileLastModificationTime;

				mmsDataRoot["uniqueName"] = uniqueName;

				userDataRoot["mmsData"] = mmsDataRoot;
			}

			// Title
			string addContentTitle;
			{
				/*
				if (streamSourceType == "IP_PUSH")
				{
					int64_t deliveryCode = JSONUtils::asInt64(ingestedParametersRoot,
						"DeliveryCode", 0);
					addContentTitle = to_string(deliveryCode);
				}
				else
				{
					// 2021-02-03: in this case, we will use the 'ConfigurationLabel' that
					// it is much better that a code. Who will see the title of the chunks will recognize
					// easily the recording
					addContentTitle = ingestedParametersRoot.get("ConfigurationLabel", "").asString();
				}
				*/
				// string ingestionJobLabel = encodingParametersRoot.get("ingestionJobLabel", "").asString();
				if (ingestionJobLabel == "")
				{
					int64_t deliveryCode = JSONUtils::asInt64(ingestedParametersRoot,
						"DeliveryCode", 0);
					addContentTitle = to_string(deliveryCode);
				}
				else
					addContentTitle = ingestionJobLabel;

				addContentTitle += " - ";

				{
					tm		tmDateTime;
					char	strCurrentRecordedFileTime [64];

					// from utc to local time
					localtime_r (&utcCurrentRecordedFileCreationTime, &tmDateTime);

					/*
					sprintf (strCurrentRecordedFileTime,
						"%04d-%02d-%02d %02d:%02d:%02d",
						tmDateTime. tm_year + 1900,
						tmDateTime. tm_mon + 1,
						tmDateTime. tm_mday,
						tmDateTime. tm_hour,
						tmDateTime. tm_min,
						tmDateTime. tm_sec);
					*/

					sprintf (strCurrentRecordedFileTime,
						"%02d:%02d:%02d",
						tmDateTime. tm_hour,
						tmDateTime. tm_min,
						tmDateTime. tm_sec);

					addContentTitle += strCurrentRecordedFileTime;	// local time
				}

				addContentTitle += " - ";

				{
					tm		tmDateTime;
					char	strCurrentRecordedFileTime [64];

					// from utc to local time
					localtime_r (&utcCurrentRecordedFileLastModificationTime, &tmDateTime);

					/*
					sprintf (strCurrentRecordedFileTime,
						"%04d-%02d-%02d %02d:%02d:%02d",
						tmDateTime. tm_year + 1900,
						tmDateTime. tm_mon + 1,
						tmDateTime. tm_mday,
						tmDateTime. tm_hour,
						tmDateTime. tm_min,
						tmDateTime. tm_sec);
					*/
					sprintf (strCurrentRecordedFileTime,
						"%02d:%02d:%02d",
						tmDateTime. tm_hour,
						tmDateTime. tm_min,
						tmDateTime. tm_sec);

					addContentTitle += strCurrentRecordedFileTime;	// local time
				}

				// if (!main)
				// 	addContentTitle += " (BCK)";
			}

			if (isFirstChunk)
			{
				_logger->info(__FILEREF__ + "The first asset file name is not ingested because it does not contain the entire period and it will be removed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", currentRecordedAssetPathName: " + transcoderStagingContentsPath + currentRecordedAssetFileName
					+ ", title: " + addContentTitle
				);

				_logger->info(__FILEREF__ + "Remove"
					+ ", currentRecordedAssetPathName: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				);

                FileIO::remove(transcoderStagingContentsPath + currentRecordedAssetFileName);
			}
			else
			{
				try
				{
					_logger->info(__FILEREF__ + "ingest Recorded media"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
						+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
						+ ", stagingContentsPath: " + stagingContentsPath
						+ ", addContentTitle: " + addContentTitle
					);

					if (externalEncoder)
						liveRecorder_ingestRecordedMediaInCaseOfExternalTranscoder(ingestionJobKey,
							transcoderStagingContentsPath, currentRecordedAssetFileName,
							addContentTitle, uniqueName, /* highAvailability, */ userDataRoot, outputFileFormat,
							ingestedParametersRoot, encodingParametersRoot);
					else
						liveRecorder_ingestRecordedMediaInCaseOfInternalTranscoder(ingestionJobKey,
							transcoderStagingContentsPath, currentRecordedAssetFileName,
							stagingContentsPath,
							addContentTitle, uniqueName, /* highAvailability, */ userDataRoot, outputFileFormat,
							ingestedParametersRoot, encodingParametersRoot,
							false);
				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "liveRecorder_ingestRecordedMedia failed"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", externalEncoder: " + to_string(externalEncoder)
						+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
						+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
						+ ", stagingContentsPath: " + stagingContentsPath
						+ ", addContentTitle: " + addContentTitle
						+ ", outputFileFormat: " + outputFileFormat
						+ ", e.what(): " + e.what()
					);

					// throw e;
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__ + "liveRecorder_ingestRecordedMedia failed"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", externalEncoder: " + to_string(externalEncoder)
						+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
						+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
						+ ", stagingContentsPath: " + stagingContentsPath
						+ ", addContentTitle: " + addContentTitle
						+ ", outputFileFormat: " + outputFileFormat
					);
                
					// throw e;
				}
			}
		}

		if (reachedNextFileToProcess == false)
		{
			// this scenario should never happens, we have only one option when mmEngineService
			// is restarted, the new LiveRecorder is not started and the segments list file
			// contains still old files. So newLastRecordedAssetFileName is initialized
			// with the old file that will never be found once LiveRecorder starts and reset
			// the segment list file
			// In this scenario, we will reset newLastRecordedAssetFileName
			newLastRecordedAssetFileName	= "";
		}
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "liveRecorder_processStreamSegmenterOutput failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
			+ ", stagingContentsPath: " + stagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "liveRecorder_processStreamSegmenterOutput failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
			+ ", stagingContentsPath: " + stagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
        );
                
        throw e;
    }

	return make_pair(newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds);
}

pair<string, double> FFMPEGEncoder::liveRecorder_processHLSSegmenterOutput(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	string streamSourceType,
	bool externalEncoder,
	int segmentDurationInSeconds, string outputFileFormat,
	Json::Value encodingParametersRoot,
	Json::Value ingestedParametersRoot,
	string transcoderStagingContentsPath,
	string sharedStagingContentsPath,
	string segmentListFileName,
	string recordedFileNamePrefix,
	string lastRecordedAssetFileName,
	double lastRecordedAssetDurationInSeconds)
{

	string newLastRecordedAssetFileName = lastRecordedAssetFileName;
	double newLastRecordedAssetDurationInSeconds = lastRecordedAssetDurationInSeconds;

    try
    {
		_logger->info(__FILEREF__ + "liveRecorder_processHLSSegmenterOutput"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			// + ", highAvailability: " + to_string(highAvailability)
			// + ", main: " + to_string(main)
			+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
			+ ", outputFileFormat: " + outputFileFormat
			+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
			+ ", sharedStagingContentsPath: " + sharedStagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
			+ ", recordedFileNamePrefix: " + recordedFileNamePrefix
			+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName
			+ ", lastRecordedAssetDurationInSeconds: " + to_string(lastRecordedAssetDurationInSeconds)
		);

		double toBeIngestedSegmentDuration = -1.0;
		int64_t toBeIngestedSegmentUtcStartTimeInMillisecs = -1;
		string toBeIngestedSegmentFileName;
		{
			double currentSegmentDuration = -1.0;
			int64_t currentSegmentUtcStartTimeInMillisecs = -1;
			string currentSegmentFileName;

			bool toBeIngested = false;

			_logger->info(__FILEREF__ + "Reading manifest"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", transcoderStagingContentsPath + segmentListFileName: " + transcoderStagingContentsPath + segmentListFileName
			);

			ifstream segmentList;
			segmentList.open(transcoderStagingContentsPath + segmentListFileName, ifstream::in);
			if (!segmentList)
			{
				string errorMessage = __FILEREF__ + "No segment list file found yet"
					+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
					+ ", segmentListFileName: " + segmentListFileName
					+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName;
				_logger->warn(errorMessage);

				return make_pair(lastRecordedAssetFileName, lastRecordedAssetDurationInSeconds);
				// throw runtime_error(errorMessage);
			}

			int ingestionNumber = 0;
			string manifestLine;
			while(getline(segmentList, manifestLine))
			{
				_logger->info(__FILEREF__ + "Reading manifest line"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", manifestLine: " + manifestLine
					+ ", toBeIngested: " + to_string(toBeIngested)
					+ ", toBeIngestedSegmentDuration: " + to_string(toBeIngestedSegmentDuration)
					+ ", toBeIngestedSegmentUtcStartTimeInMillisecs: " + to_string(toBeIngestedSegmentUtcStartTimeInMillisecs)
					+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
					+ ", currentSegmentDuration: " + to_string(currentSegmentDuration)
					+ ", currentSegmentUtcStartTimeInMillisecs: " + to_string(currentSegmentUtcStartTimeInMillisecs)
					+ ", currentSegmentFileName: " + currentSegmentFileName
					+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName
					+ ", newLastRecordedAssetFileName: " + newLastRecordedAssetFileName
				);

				// #EXTINF:14.640000,
				// #EXT-X-PROGRAM-DATE-TIME:2021-02-26T15:41:15.477+0100
				// <segment file name>

				if (manifestLine.size() == 0)
					continue;

				string durationPrefix ("#EXTINF:");
				string dateTimePrefix = "#EXT-X-PROGRAM-DATE-TIME:";
				if (manifestLine.size() >= durationPrefix.size()
					&& 0 == manifestLine.compare(0, durationPrefix.size(), durationPrefix))
				{
					size_t endOfSegmentDuration = manifestLine.find(",");
					if (endOfSegmentDuration == string::npos)
					{
						string errorMessage = string("wrong manifest line format")
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", manifestLine: " + manifestLine
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}

					if (toBeIngested)
						toBeIngestedSegmentDuration = stod(manifestLine.substr(durationPrefix.size(),
							endOfSegmentDuration - durationPrefix.size()));
					else
						currentSegmentDuration = stod(manifestLine.substr(durationPrefix.size(),
							endOfSegmentDuration - durationPrefix.size()));
				}
				else if (manifestLine.size() >= dateTimePrefix.size() && 0 == manifestLine.compare(0, dateTimePrefix.size(),
					dateTimePrefix))
				{
					if (toBeIngested)
						toBeIngestedSegmentUtcStartTimeInMillisecs = DateTime::sDateMilliSecondsToUtc(
							manifestLine.substr(dateTimePrefix.size()));
					else
						currentSegmentUtcStartTimeInMillisecs = DateTime::sDateMilliSecondsToUtc(
							manifestLine.substr(dateTimePrefix.size()));
				}
				else if (manifestLine[0] != '#')
				{
					if (toBeIngested)
						toBeIngestedSegmentFileName = manifestLine;
					else
						currentSegmentFileName = manifestLine;
				}
				else
				{
					_logger->info(__FILEREF__ + "manifest line not used by our algorithm"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", manifestLine: " + manifestLine
					);

					continue;
				}

				if (
					// case 1: we are in the toBeIngested status (part of the playlist after the last ingested segment)
					//	and we are all the details of the new ingested segment
					(
						toBeIngested
						&& toBeIngestedSegmentDuration != -1.0
						&& toBeIngestedSegmentUtcStartTimeInMillisecs != -1
						&& toBeIngestedSegmentFileName != ""
					)
					||
					// case 2: we are NOT in the toBeIngested status
					//	but we just started to ingest (lastRecordedAssetFileName == "")
					//	and we have all the details of the ingested segment
					(
						!toBeIngested
						&& currentSegmentDuration != -1.0
						&& currentSegmentUtcStartTimeInMillisecs != -1
						&& currentSegmentFileName != ""
						&& lastRecordedAssetFileName == ""
					)
				)
				{
					// if we are in case 2, let's initialize variables like we are in case 1
					if (!toBeIngested)
					{
						toBeIngestedSegmentDuration = currentSegmentDuration;
						toBeIngestedSegmentUtcStartTimeInMillisecs = currentSegmentUtcStartTimeInMillisecs;
						toBeIngestedSegmentFileName = currentSegmentFileName;

						toBeIngested = true;
					}

					// ingest the asset and initilize
					// newLastRecordedAssetFileName and newLastRecordedAssetDurationInSeconds
					{
						int64_t toBeIngestedSegmentUtcEndTimeInMillisecs =
							toBeIngestedSegmentUtcStartTimeInMillisecs + (toBeIngestedSegmentDuration * 1000);

						_logger->info(__FILEREF__ + "processing LiveRecorder file"
							+ ", toBeIngestedSegmentDuration: " + to_string(toBeIngestedSegmentDuration)
							+ ", toBeIngestedSegmentUtcStartTimeInMillisecs: " + to_string(toBeIngestedSegmentUtcStartTimeInMillisecs)
							+ ", toBeIngestedSegmentUtcEndTimeInMillisecs: " + to_string(toBeIngestedSegmentUtcEndTimeInMillisecs)
							+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
						);

						if (!FileIO::fileExisting(transcoderStagingContentsPath + toBeIngestedSegmentFileName))
						{
							// it could be the scenario where mmsEngineService is restarted,
							// the segments list file still contains obsolete filenames
							_logger->error(__FILEREF__ + "file not existing"
								", currentRecordedAssetPathName: " + transcoderStagingContentsPath + toBeIngestedSegmentFileName
							);

							return make_pair(newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds);
						}

						// initialize metadata and ingest the asset
						{
							string uniqueName;
							{
								int64_t deliveryCode = JSONUtils::asInt64(ingestedParametersRoot, "DeliveryCode", 0);

								uniqueName = to_string(deliveryCode);
								uniqueName += " - ";
								uniqueName += to_string(toBeIngestedSegmentUtcStartTimeInMillisecs);
							}

							string ingestionJobLabel = encodingParametersRoot.get("ingestionJobLabel", "").asString();

							// UserData
							Json::Value userDataRoot;
							{
								if (JSONUtils::isMetadataPresent(ingestedParametersRoot, "UserData"))
									userDataRoot = ingestedParametersRoot["UserData"];

								Json::Value mmsDataRoot;
								mmsDataRoot["dataType"] = "liveRecordingChunk";
								/*
								mmsDataRoot["streamSourceType"] = streamSourceType;
								if (streamSourceType == "IP_PULL")
									mmsDataRoot["ipConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
								else if (streamSourceType == "TV")
									mmsDataRoot["satConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
								else // if (streamSourceType == "IP_PUSH")
								*/
								{
									int64_t deliveryCode = JSONUtils::asInt64(ingestedParametersRoot,
										"DeliveryCode", 0);
									mmsDataRoot["deliveryCode"] = deliveryCode;
								}
								mmsDataRoot["ingestionJobLabel"] = ingestionJobLabel;
								// mmsDataRoot["main"] = main;
								mmsDataRoot["main"] = true;
								// if (!highAvailability)
								{
									bool validated = true;
									mmsDataRoot["validated"] = validated;
								}
								mmsDataRoot["ingestionJobKey"] = (int64_t) (ingestionJobKey);
								/*
								mmsDataRoot["utcPreviousChunkStartTime"] =
									(time_t) (utcCurrentRecordedFileCreationTime - lastRecordedAssetDurationInSeconds);
								*/
								mmsDataRoot["utcChunkStartTime"] =
									(int64_t) (toBeIngestedSegmentUtcStartTimeInMillisecs / 1000);
								mmsDataRoot["utcStartTimeInMilliSecs"] =
									toBeIngestedSegmentUtcStartTimeInMillisecs;

								mmsDataRoot["utcChunkEndTime"] =
									(int64_t) (toBeIngestedSegmentUtcEndTimeInMillisecs / 1000);
								mmsDataRoot["utcEndTimeInMilliSecs"] =
									toBeIngestedSegmentUtcEndTimeInMillisecs;

								mmsDataRoot["uniqueName"] = uniqueName;

								userDataRoot["mmsData"] = mmsDataRoot;
							}

							// Title
							string addContentTitle;
							{
								if (ingestionJobLabel == "")
								{
									int64_t deliveryCode = JSONUtils::asInt64(ingestedParametersRoot,
										"DeliveryCode", 0);
									addContentTitle = to_string(deliveryCode);
								}
								else
									addContentTitle = ingestionJobLabel;

								addContentTitle += " - ";

								{
									tm		tmDateTime;
									char	strCurrentRecordedFileTime [64];

									time_t toBeIngestedSegmentUtcStartTimeInSeconds =
										toBeIngestedSegmentUtcStartTimeInMillisecs / 1000;
									int toBeIngestedSegmentMilliSecs = toBeIngestedSegmentUtcStartTimeInMillisecs % 1000;

									// from utc to local time
									localtime_r (&toBeIngestedSegmentUtcStartTimeInSeconds, &tmDateTime);

									/*
									sprintf (strCurrentRecordedFileTime,
										"%04d-%02d-%02d %02d:%02d:%02d",
										tmDateTime. tm_year + 1900,
										tmDateTime. tm_mon + 1,
										tmDateTime. tm_mday,
										tmDateTime. tm_hour,
										tmDateTime. tm_min,
										tmDateTime. tm_sec);
									*/

									sprintf (strCurrentRecordedFileTime,
										"%02d:%02d:%02d.%03d",
										tmDateTime. tm_hour,
										tmDateTime. tm_min,
										tmDateTime. tm_sec,
										toBeIngestedSegmentMilliSecs
										);

									addContentTitle += strCurrentRecordedFileTime;	// local time
								}

								addContentTitle += " - ";

								{
									tm		tmDateTime;
									char	strCurrentRecordedFileTime [64];

									time_t toBeIngestedSegmentUtcEndTimeInSeconds =
										toBeIngestedSegmentUtcEndTimeInMillisecs / 1000;
									int toBeIngestedSegmentMilliSecs = toBeIngestedSegmentUtcEndTimeInMillisecs % 1000;

									// from utc to local time
									localtime_r (&toBeIngestedSegmentUtcEndTimeInSeconds, &tmDateTime);

									/*
									sprintf (strCurrentRecordedFileTime,
										"%04d-%02d-%02d %02d:%02d:%02d",
										tmDateTime. tm_year + 1900,
										tmDateTime. tm_mon + 1,
										tmDateTime. tm_mday,
										tmDateTime. tm_hour,
										tmDateTime. tm_min,
										tmDateTime. tm_sec);
									*/
									sprintf (strCurrentRecordedFileTime,
										"%02d:%02d:%02d.%03d",
										tmDateTime. tm_hour,
										tmDateTime. tm_min,
										tmDateTime. tm_sec,
										toBeIngestedSegmentMilliSecs);

									addContentTitle += strCurrentRecordedFileTime;	// local time
								}

								// if (!main)
								// 	addContentTitle += " (BCK)";
							}

							{
								try
								{
									_logger->info(__FILEREF__ + "ingest Recorded media"
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
										+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
										+ ", sharedStagingContentsPath: " + sharedStagingContentsPath
										+ ", addContentTitle: " + addContentTitle
									);

									if (externalEncoder)
										liveRecorder_ingestRecordedMediaInCaseOfExternalTranscoder(ingestionJobKey,
											transcoderStagingContentsPath, toBeIngestedSegmentFileName,
											addContentTitle, uniqueName, userDataRoot, outputFileFormat,
											ingestedParametersRoot, encodingParametersRoot);
									else
										liveRecorder_ingestRecordedMediaInCaseOfInternalTranscoder(ingestionJobKey,
											transcoderStagingContentsPath, toBeIngestedSegmentFileName,
											sharedStagingContentsPath,
											addContentTitle, uniqueName, userDataRoot, outputFileFormat,
											ingestedParametersRoot, encodingParametersRoot,
											true);
								}
								catch(runtime_error e)
								{
									_logger->error(__FILEREF__ + "liveRecorder_ingestRecordedMedia failed"
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", externalEncoder: " + to_string(externalEncoder)
										+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
										+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
										+ ", sharedStagingContentsPath: " + sharedStagingContentsPath
										+ ", addContentTitle: " + addContentTitle
										+ ", outputFileFormat: " + outputFileFormat
										+ ", e.what(): " + e.what()
									);

									// throw e;
								}
								catch(exception e)
								{
									_logger->error(__FILEREF__ + "liveRecorder_ingestRecordedMedia failed"
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", externalEncoder: " + to_string(externalEncoder)
										+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
										+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
										+ ", sharedStagingContentsPath: " + sharedStagingContentsPath
										+ ", addContentTitle: " + addContentTitle
										+ ", outputFileFormat: " + outputFileFormat
									);
                
									// throw e;
								}
							}
						}

						newLastRecordedAssetFileName = toBeIngestedSegmentFileName;
						newLastRecordedAssetDurationInSeconds = toBeIngestedSegmentDuration;
					}

					ingestionNumber++;

					toBeIngestedSegmentDuration = -1.0;
					toBeIngestedSegmentUtcStartTimeInMillisecs = -1;
					toBeIngestedSegmentFileName = "";
				}
				else if (lastRecordedAssetFileName == currentSegmentFileName)
				{
					toBeIngested = true;
				}
			}

			// Scenario:
			//	we have lastRecordedAssetFileName with a filename that does not exist into the playlist
			// This is a scenario that should never happen but, in case it happens, we have to manage otherwise
			// no chunks will be ingested
			// lastRecordedAssetFileName has got from playlist in the previous liveRecorder_processHLSSegmenterOutput call
			if (lastRecordedAssetFileName != ""
				&& !toBeIngested					// file name does not exist into the playlist
			)
			{
				// 2022-08-12: this scenario happens when the 'monitorin' kills the recording process,
				//	so the playlist is reset and start from scratch.

				_logger->warn(__FILEREF__ + "Filename not found: probable the playlist was reset (may be because of a kill of the monitor process)"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", toBeIngested: " + to_string(toBeIngested)
					+ ", toBeIngestedSegmentDuration: " + to_string(toBeIngestedSegmentDuration)
					+ ", toBeIngestedSegmentUtcStartTimeInMillisecs: " + to_string(toBeIngestedSegmentUtcStartTimeInMillisecs)
					+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
					+ ", currentSegmentDuration: " + to_string(currentSegmentDuration)
					+ ", currentSegmentUtcStartTimeInMillisecs: " + to_string(currentSegmentUtcStartTimeInMillisecs)
					+ ", currentSegmentFileName: " + currentSegmentFileName
					+ ", lastRecordedAssetFileName: " + lastRecordedAssetFileName
					+ ", newLastRecordedAssetFileName: " + newLastRecordedAssetFileName
				);

				newLastRecordedAssetFileName = "";
				newLastRecordedAssetDurationInSeconds = 0.0;
			}
		}
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "liveRecorder_processHLSSegmenterOutput failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
			+ ", sharedStagingContentsPath: " + sharedStagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "liveRecorder_processHLSSegmenterOutput failed"
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
			+ ", sharedStagingContentsPath: " + sharedStagingContentsPath
			+ ", segmentListFileName: " + segmentListFileName
        );
                
        throw e;
    }

	return make_pair(newLastRecordedAssetFileName, newLastRecordedAssetDurationInSeconds);
}

void FFMPEGEncoder::liveRecorder_ingestRecordedMediaInCaseOfInternalTranscoder(
	int64_t ingestionJobKey,
	string transcoderStagingContentsPath, string currentRecordedAssetFileName,
	string sharedStagingContentsPath,
	string addContentTitle,
	string uniqueName,
	// bool highAvailability,
	Json::Value userDataRoot,
	string fileFormat,
	Json::Value ingestedParametersRoot,
	Json::Value encodingParametersRoot,
	bool copy)
{
	try
	{
		// moving chunk from transcoder staging path to shared staging path.
		// This is done because the AddContent task has a move://... url
		if (copy)
		{
			_logger->info(__FILEREF__ + "Chunk copying"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
				+ ", source: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + sharedStagingContentsPath
			);

			chrono::system_clock::time_point startCopying = chrono::system_clock::now();
			FileIO::copyFile(transcoderStagingContentsPath + currentRecordedAssetFileName, sharedStagingContentsPath);
			chrono::system_clock::time_point endCopying = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Chunk copied"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + sharedStagingContentsPath
				+ ", @MMS COPY statistics@ - copyingDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endCopying - startCopying).count()) + "@"
			);
		}
		else
		{
			_logger->info(__FILEREF__ + "Chunk moving"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", currentRecordedAssetFileName: " + currentRecordedAssetFileName
				+ ", source: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + sharedStagingContentsPath
			);

			chrono::system_clock::time_point startMoving = chrono::system_clock::now();
			FileIO::moveFile(transcoderStagingContentsPath + currentRecordedAssetFileName, sharedStagingContentsPath);
			chrono::system_clock::time_point endMoving = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Chunk moved"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + sharedStagingContentsPath
				+ ", @MMS MOVE statistics@ - movingDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endMoving - startMoving).count()) + "@"
			);
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = e.what();
		_logger->error(__FILEREF__ + "Coping/Moving of the chunk failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", exception: " + errorMessage
		);
		if (errorMessage.find(string("errno: 28")) != string::npos)
			_logger->error(__FILEREF__ + "No space left on storage"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", exception: " + errorMessage
			);


		_logger->info(__FILEREF__ + "remove"
			+ ", generated chunk: " + transcoderStagingContentsPath + currentRecordedAssetFileName 
		);
		bool exceptionInCaseOfError = false;
		FileIO::remove(transcoderStagingContentsPath + currentRecordedAssetFileName, exceptionInCaseOfError);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", exception: " + e.what()
		);

		_logger->info(__FILEREF__ + "remove"
			+ ", generated chunk: " + transcoderStagingContentsPath + currentRecordedAssetFileName 
		);
		bool exceptionInCaseOfError = false;
		FileIO::remove(transcoderStagingContentsPath + currentRecordedAssetFileName, exceptionInCaseOfError);

		throw e;
	}

	string mmsWorkflowIngestionURL;
	string workflowMetadata;
	try
	{
		workflowMetadata = liveRecorder_buildChunkIngestionWorkflow(
			ingestionJobKey,
			false,	// externalEncoder,
			currentRecordedAssetFileName,
			sharedStagingContentsPath,
			addContentTitle,
			uniqueName,
			userDataRoot,
			fileFormat,
			ingestedParametersRoot,
			encodingParametersRoot
		);

		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
    		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				Json::Value internalMMSRoot = ingestedParametersRoot[field];

				field = "credentials";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					Json::Value credentialsRoot = internalMMSRoot[field];

					field = "userKey";
					userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

					field = "apiKey";
					string apiKeyEncrypted = credentialsRoot.get(field, "").asString();
					apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
				}
			}
		}

		{
			string field = "mmsWorkflowIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = encodingParametersRoot.get(field, "").asString();
		}

		string sResponse = MMSCURL::httpPostPutString(
			ingestionJobKey,
			mmsWorkflowIngestionURL,
			"POST",	// requestType
			_mmsAPITimeoutInSeconds,
			to_string(userKey),
			apiKey,
			workflowMetadata,
			"application/json",	// contentType
			_logger
		);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

void FFMPEGEncoder::liveRecorder_ingestRecordedMediaInCaseOfExternalTranscoder(
	int64_t ingestionJobKey,
	string transcoderStagingContentsPath, string currentRecordedAssetFileName,
	string addContentTitle,
	string uniqueName,
	Json::Value userDataRoot,
	string fileFormat,
	Json::Value ingestedParametersRoot,
	Json::Value encodingParametersRoot)
{
	string workflowMetadata;
	int64_t userKey;
	string apiKey;
	int64_t addContentIngestionJobKey = -1;
	string mmsWorkflowIngestionURL;
	// create the workflow and ingest it
	try
	{
		workflowMetadata = liveRecorder_buildChunkIngestionWorkflow(
			ingestionJobKey,
			true,	// externalEncoder,
			"",	// currentRecordedAssetFileName,
			"",	// stagingContentsPath,
			addContentTitle,
			uniqueName,
			userDataRoot,
			fileFormat,
			ingestedParametersRoot,
			encodingParametersRoot
		);

		{
			string field = "internalMMS";
    		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				Json::Value internalMMSRoot = ingestedParametersRoot[field];

				field = "credentials";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					Json::Value credentialsRoot = internalMMSRoot[field];

					field = "userKey";
					userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

					field = "apiKey";
					string apiKeyEncrypted = credentialsRoot.get(field, "").asString();
					apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
				}
			}
		}

		{
			string field = "mmsWorkflowIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = encodingParametersRoot.get(field, "").asString();
		}

		string sResponse = MMSCURL::httpPostPutString(
			ingestionJobKey,
			mmsWorkflowIngestionURL,
			"POST",	// requestType
			_mmsAPITimeoutInSeconds,
			to_string(userKey),
			apiKey,
			workflowMetadata,
			"application/json",	// contentType
			_logger
		);

		addContentIngestionJobKey = getAddContentIngestionJobKey(ingestionJobKey, sResponse);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}

	if (addContentIngestionJobKey == -1)
	{
		string errorMessage =
			string("Ingested URL failed, addContentIngestionJobKey is not valid")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	string mmsBinaryURL;
	// ingest binary
	try
	{
		bool inCaseOfLinkHasItToBeRead = false;
		int64_t chunkFileSize = FileIO::getFileSizeInBytes(
			transcoderStagingContentsPath + currentRecordedAssetFileName,
			inCaseOfLinkHasItToBeRead);

		string mmsBinaryIngestionURL;
		{
			string field = "mmsBinaryIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsBinaryIngestionURL = encodingParametersRoot.get(field, "").asString();
		}

		mmsBinaryURL =
			mmsBinaryIngestionURL
			+ "/" + to_string(addContentIngestionJobKey)
		;

		string sResponse = MMSCURL::httpPostPutFile(
			ingestionJobKey,
			mmsBinaryURL,
			"POST",	// requestType
			_mmsBinaryTimeoutInSeconds,
			to_string(userKey),
			apiKey,
			transcoderStagingContentsPath + currentRecordedAssetFileName,
			chunkFileSize,
			_logger);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

string FFMPEGEncoder::liveRecorder_buildChunkIngestionWorkflow(
	int64_t ingestionJobKey,
	bool externalEncoder,
	string currentRecordedAssetFileName,
	string stagingContentsPath,
	string addContentTitle,
	string uniqueName,
	Json::Value userDataRoot,
	string fileFormat,
	Json::Value ingestedParametersRoot,
	Json::Value encodingParametersRoot
)
{
	string workflowMetadata;
	try
	{
		/*
		{
        	"Label": "<workflow label>",
        	"Type": "Workflow",
        	"Task": {
                "Label": "<task label 1>",
                "Type": "Add-Content"
                "Parameters": {
                        "FileFormat": "ts",
                        "Ingester": "Giuliano",
                        "SourceURL": "move:///abc...."
                },
        	}
		}
		*/
		Json::Value mmsDataRoot = userDataRoot["mmsData"];
		int64_t utcPreviousChunkStartTime = JSONUtils::asInt64(mmsDataRoot, "utcPreviousChunkStartTime", -1);
		int64_t utcChunkStartTime = JSONUtils::asInt64(mmsDataRoot, "utcChunkStartTime", -1);
		int64_t utcChunkEndTime = JSONUtils::asInt64(mmsDataRoot, "utcChunkEndTime", -1);

		Json::Value addContentRoot;

		string field = "Label";
		addContentRoot[field] = to_string(utcChunkStartTime);

		field = "Type";
		addContentRoot[field] = "Add-Content";

		{
			field = "internalMMS";
    		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				Json::Value internalMMSRoot = ingestedParametersRoot[field];

				field = "events";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					Json::Value eventsRoot = internalMMSRoot[field];

					field = "OnSuccess";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						addContentRoot[field] = eventsRoot[field];

					field = "OnError";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						addContentRoot[field] = eventsRoot[field];

					field = "OnComplete";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						addContentRoot[field] = eventsRoot[field];
				}
			}
		}

		Json::Value addContentParametersRoot = ingestedParametersRoot;
		// if (internalMMSRootPresent)
		{
			Json::Value removed;
			field = "internalMMS";
			addContentParametersRoot.removeMember(field, &removed);
		}

		field = "FileFormat";
		addContentParametersRoot[field] = fileFormat;

		if (!externalEncoder)
		{
			string sourceURL = string("move") + "://" + stagingContentsPath + currentRecordedAssetFileName;
			field = "SourceURL";
			addContentParametersRoot[field] = sourceURL;
		}

		field = "Ingester";
		addContentParametersRoot[field] = "Live Recorder Task";

		field = "Title";
		addContentParametersRoot[field] = addContentTitle;

		field = "UserData";
		addContentParametersRoot[field] = userDataRoot;

		// if (!highAvailability)
		{
			// in case of no high availability, we can set just now the UniqueName for this content
			// in case of high availability, the unique name will be set only of the selected content
			//		choosing between main and bqckup
			field = "UniqueName";
			addContentParametersRoot[field] = uniqueName;
		}

		field = "Parameters";
		addContentRoot[field] = addContentParametersRoot;


		Json::Value workflowRoot;

		field = "Label";
		workflowRoot[field] = addContentTitle;

		field = "Type";
		workflowRoot[field] = "Workflow";

		{
			Json::Value variablesWorkflowRoot;

			{
				Json::Value variableWorkflowRoot;

				field = "Type";
				variableWorkflowRoot[field] = "integer";

				field = "Value";
				variableWorkflowRoot[field] = utcChunkStartTime;

				// name of the variable
				field = "CurrentUtcChunkStartTime";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			char	currentUtcChunkStartTime_HHMISS [64];
			{
				tm		tmDateTime;

				// from utc to local time
				localtime_r (&utcChunkStartTime, &tmDateTime);

				sprintf (currentUtcChunkStartTime_HHMISS,
					"%02d:%02d:%02d",
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);

			}
			// field = "CurrentUtcChunkStartTime_HHMISS";
			// variablesWorkflowRoot[field] = string(currentUtcChunkStartTime_HHMISS);
			{
				Json::Value variableWorkflowRoot;

				field = "Type";
				variableWorkflowRoot[field] = "string";

				field = "Value";
				variableWorkflowRoot[field] = string(currentUtcChunkStartTime_HHMISS);

				// name of the variable
				field = "CurrentUtcChunkStartTime_HHMISS";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			// field = "PreviousUtcChunkStartTime";
			// variablesWorkflowRoot[field] = utcPreviousChunkStartTime;
			{
				Json::Value variableWorkflowRoot;

				field = "Type";
				variableWorkflowRoot[field] = "integer";

				field = "Value";
				variableWorkflowRoot[field] = utcPreviousChunkStartTime;

				// name of the variable
				field = "PreviousUtcChunkStartTime";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			int64_t deliveryCode = JSONUtils::asInt64(ingestedParametersRoot, "DeliveryCode", 0);
			{
				Json::Value variableWorkflowRoot;

				field = "Type";
				variableWorkflowRoot[field] = "integer";

				field = "Value";
				variableWorkflowRoot[field] = deliveryCode;

				// name of the variable
				field = "DeliveryCode";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			string ingestionJobLabel = encodingParametersRoot.get("ingestionJobLabel", "").asString();
			{
				Json::Value variableWorkflowRoot;

				field = "Type";
				variableWorkflowRoot[field] = "string";

				field = "Value";
				variableWorkflowRoot[field] = ingestionJobLabel;

				// name of the variable
				field = "IngestionJobLabel";
				variablesWorkflowRoot[field] = variableWorkflowRoot;
			}

			field = "Variables";
			workflowRoot[field] = variablesWorkflowRoot;
		}

		field = "Task";
		workflowRoot[field] = addContentRoot;

   		{
       		Json::StreamWriterBuilder wbuilder;
       		workflowMetadata = Json::writeString(wbuilder, workflowRoot);
   		}

		_logger->info(__FILEREF__ + "Recording Workflow metadata generated"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", " + addContentTitle + ", "
				+ currentRecordedAssetFileName
				+ ", prev: " + to_string(utcPreviousChunkStartTime)
				+ ", from: " + to_string(utcChunkStartTime)
				+ ", to: " + to_string(utcChunkEndTime)
		);

		return workflowMetadata;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "buildRecordedMediaWorkflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "buildRecordedMediaWorkflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

bool FFMPEGEncoder::liveRecorder_isLastLiveRecorderFile(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	time_t utcCurrentRecordedFileCreationTime, string transcoderStagingContentsPath,
	string recordedFileNamePrefix, int segmentDurationInSeconds, bool isFirstChunk)
{
	bool isLastLiveRecorderFile = true;

    try
    {
		_logger->info(__FILEREF__ + "liveRecorder_isLastLiveRecorderFile"
			+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
			+ ", recordedFileNamePrefix: " + recordedFileNamePrefix
			+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
		);

        FileIO::DirectoryEntryType_t detDirectoryEntryType;
        shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (transcoderStagingContentsPath);

        bool scanDirectoryFinished = false;
        while (!scanDirectoryFinished)
        {
            string directoryEntry;
            try
            {
                string directoryEntry = FileIO::readDirectory (directory,
                    &detDirectoryEntryType);

				_logger->info(__FILEREF__ + "FileIO::readDirectory"
					+ ", directoryEntry: " + directoryEntry
					+ ", detDirectoryEntryType: " + to_string(static_cast<int>(detDirectoryEntryType))
				);

				// next statement is endWith and .lck is used during the move of a file
				string suffix(".lck");
				if (directoryEntry.size() >= suffix.size()
					&& 0 == directoryEntry.compare(directoryEntry.size()-suffix.size(),
						suffix.size(), suffix))
					continue;

                if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
                    continue;

                if (directoryEntry.size() >= recordedFileNamePrefix.size()
						&& directoryEntry.compare(0, recordedFileNamePrefix.size(),
							recordedFileNamePrefix) == 0)
                {
					time_t utcFileCreationTime = liveRecorder_getMediaLiveRecorderStartTime(
							ingestionJobKey, encodingJobKey, directoryEntry, segmentDurationInSeconds,
							isFirstChunk);

					if (utcFileCreationTime > utcCurrentRecordedFileCreationTime)
					{
						isLastLiveRecorderFile = false;

						break;
					}
				}
            }
            catch(DirectoryListFinished e)
            {
                scanDirectoryFinished = true;
            }
            catch(runtime_error e)
            {
                string errorMessage = __FILEREF__ + "listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
            catch(exception e)
            {
                string errorMessage = __FILEREF__ + "listing directory failed"
                       + ", e.what(): " + e.what()
                ;
                _logger->error(errorMessage);

                throw e;
            }
        }

        FileIO::closeDirectory (directory);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "liveRecorder_isLastLiveRecorderFile failed"
            + ", e.what(): " + e.what()
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "liveRecorder_isLastLiveRecorderFile failed");
    }

	return isLastLiveRecorderFile;
}

time_t FFMPEGEncoder::liveRecorder_getMediaLiveRecorderStartTime(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	string mediaLiveRecorderFileName, int segmentDurationInSeconds,
	bool isFirstChunk)
{
	// liveRecorder_6405_48749_2019-02-02_22-11-00_1100374273.ts
	// liveRecorder_<ingestionJobKey>_<encodingJobKey>_YYYY-MM-DD_HH-MI-SS_<utc>.ts

	_logger->info(__FILEREF__ + "Received liveRecorder_getMediaLiveRecorderStartTime"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", encodingJobKey: " + to_string(encodingJobKey)
		+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
		+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
		+ ", isFirstChunk: " + to_string(isFirstChunk)
	);

	size_t endIndex = mediaLiveRecorderFileName.find_last_of(".");
	if (mediaLiveRecorderFileName.length() < 20 ||
		   endIndex == string::npos)
	{
		string errorMessage = __FILEREF__ + "wrong media live recorder format"
			+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
			;
			_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	size_t beginUTCIndex = mediaLiveRecorderFileName.find_last_of("_");
	if (mediaLiveRecorderFileName.length() < 20 ||
		   beginUTCIndex == string::npos)
	{
		string errorMessage = __FILEREF__ + "wrong media live recorder format"
			+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
			;
			_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	time_t utcMediaLiveRecorderStartTime = stol(mediaLiveRecorderFileName.substr(beginUTCIndex + 1,
				endIndex - beginUTCIndex + 1));

	{
		// in case of high bit rate (huge files) and server with high cpu usage, sometime I saw seconds 1 instead of 0
		// For this reason, utcMediaLiveRecorderStartTime is fixed.
		// From the other side the first generated file is the only one where we can have seconds
		// different from 0, anyway here this is not possible because we discard the first chunk
		// 2019-10-16: I saw as well seconds == 59, in this case we would not do utcMediaLiveRecorderStartTime -= seconds
		//	as it is done below in the code but we should do utcMediaLiveRecorderStartTime += 1.
		int seconds = stoi(mediaLiveRecorderFileName.substr(beginUTCIndex - 2, 2));
		if (!isFirstChunk && seconds % segmentDurationInSeconds != 0)
		{
			int halfSegmentDurationInSeconds = segmentDurationInSeconds / 2;

			// scenario: segmentDurationInSeconds is 10 and seconds = 29
			//	Before to compare seconds with halfSegmentDurationInSeconds
			//	(the check compare the seconds between 0 and 10)
			//	we have to redure 29 to 9
			if (seconds > segmentDurationInSeconds)
			{
				int factorToBeReduced = seconds / segmentDurationInSeconds;
				seconds -= (factorToBeReduced * segmentDurationInSeconds);
			}

			if (seconds <= halfSegmentDurationInSeconds)
			{
				_logger->warn(__FILEREF__ + "Wrong seconds (start time), force it to 0"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
					+ ", seconds: " + to_string(seconds)
				);
				utcMediaLiveRecorderStartTime -= seconds;
			}
			else if (seconds > halfSegmentDurationInSeconds && seconds < segmentDurationInSeconds)
			{
				_logger->warn(__FILEREF__ + "Wrong seconds (start time), increase it"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
					+ ", seconds: " + to_string(seconds)
				);
				utcMediaLiveRecorderStartTime += (segmentDurationInSeconds - seconds);
			}
		}
	}

	return utcMediaLiveRecorderStartTime;
	/*
	tm                      tmDateTime;


	// liveRecorder_6405_2019-02-02_22-11-00.ts

	_logger->info(__FILEREF__ + "getMediaLiveRecorderStartTime"
		", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
	);

	size_t index = mediaLiveRecorderFileName.find_last_of(".");
	if (mediaLiveRecorderFileName.length() < 20 ||
		   index == string::npos)
	{
		string errorMessage = __FILEREF__ + "wrong media live recorder format"
			+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
			;
			_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	time_t utcMediaLiveRecorderStartTime;
	time (&utcMediaLiveRecorderStartTime);
	gmtime_r(&utcMediaLiveRecorderStartTime, &tmDateTime);

	tmDateTime.tm_year		= stoi(mediaLiveRecorderFileName.substr(index - 19, 4))
		- 1900;
	tmDateTime.tm_mon		= stoi(mediaLiveRecorderFileName.substr(index - 14, 2))
		- 1;
	tmDateTime.tm_mday		= stoi(mediaLiveRecorderFileName.substr(index - 11, 2));
	tmDateTime.tm_hour		= stoi(mediaLiveRecorderFileName.substr(index - 8, 2));
	tmDateTime.tm_min      = stoi(mediaLiveRecorderFileName.substr(index - 5, 2));

	// in case of high bit rate (huge files) and server with high cpu usage, sometime I saw seconds 1 instead of 0
	// For this reason, 0 is set.
	// From the other side the first generated file is the only one where we can have seconds
	// different from 0, anyway here this is not possible because we discard the first chunk
	int seconds = stoi(mediaLiveRecorderFileName.substr(index - 2, 2));
	if (seconds != 0)
	{
		_logger->warn(__FILEREF__ + "Wrong seconds (start time), force it to 0"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
				+ ", seconds: " + to_string(seconds)
				);
		seconds = 0;
	}
	tmDateTime.tm_sec      = seconds;

	utcMediaLiveRecorderStartTime = timegm (&tmDateTime);

	return utcMediaLiveRecorderStartTime;
	*/
}

time_t FFMPEGEncoder::liveRecorder_getMediaLiveRecorderEndTime(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	string mediaLiveRecorderFileName)
{
	tm                      tmDateTime;

	time_t utcCurrentRecordedFileLastModificationTime;

	FileIO::getFileTime (mediaLiveRecorderFileName.c_str(),
		&utcCurrentRecordedFileLastModificationTime);

	localtime_r(&utcCurrentRecordedFileLastModificationTime, &tmDateTime);

	// in case of high bit rate (huge files) and server with high cpu usage, sometime I saw seconds 1 instead of 0
	// For this reason, 0 is set
	if (tmDateTime.tm_sec != 0)
	{
		_logger->warn(__FILEREF__ + "Wrong seconds (end time), force it to 0"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", mediaLiveRecorderFileName: " + mediaLiveRecorderFileName
				+ ", seconds: " + to_string(tmDateTime.tm_sec)
				);
		tmDateTime.tm_sec = 0;
	}

	utcCurrentRecordedFileLastModificationTime = mktime(&tmDateTime);

	return utcCurrentRecordedFileLastModificationTime;
}

long FFMPEGEncoder::liveRecorder_buildAndIngestVirtualVOD(
	int64_t liveRecorderIngestionJobKey,
	int64_t liveRecorderEncodingJobKey,
	bool externalEncoder,

	string sourceSegmentsDirectoryPathName,
	string sourceManifestFileName,
	// /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/.../content
	string stagingLiveRecorderVirtualVODPathName,

	int64_t deliveryCode,
	string liveRecorderIngestionJobLabel,
	string liveRecorderVirtualVODUniqueName,
	string liveRecorderVirtualVODRetention,
	int64_t liveRecorderVirtualVODImageMediaItemKey,
	int64_t liveRecorderUserKey,
	string liveRecorderApiKey,
	string mmsWorkflowIngestionURL,
	string mmsBinaryIngestionURL
)
{

    _logger->info(__FILEREF__ + "Received liveRecorder_buildAndIngestVirtualVOD"
		+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
		+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
		+ ", externalEncoder: " + to_string(externalEncoder)

        + ", sourceSegmentsDirectoryPathName: " + sourceSegmentsDirectoryPathName
        + ", sourceManifestFileName: " + sourceManifestFileName
        + ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName

		+ ", deliveryCode: " + to_string(deliveryCode)
        + ", liveRecorderIngestionJobLabel: " + liveRecorderIngestionJobLabel
        + ", liveRecorderVirtualVODUniqueName: " + liveRecorderVirtualVODUniqueName
        + ", liveRecorderVirtualVODRetention: " + liveRecorderVirtualVODRetention
		+ ", liveRecorderVirtualVODImageMediaItemKey: " + to_string(liveRecorderVirtualVODImageMediaItemKey)
		+ ", liveRecorderUserKey: " + to_string(liveRecorderUserKey)
        + ", liveRecorderApiKey: " + liveRecorderApiKey
        + ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
        + ", mmsBinaryIngestionURL: " + mmsBinaryIngestionURL
    );

	long segmentsNumber = 0;

	// let's build the live recorder virtual VOD
	// - copy current manifest and TS files
	// - calculate start end time of the virtual VOD
	// - add end line to manifest
	// - create tar gz
	// - remove directory
	int64_t utcStartTimeInMilliSecs = -1;
	int64_t utcEndTimeInMilliSecs = -1;

	string virtualVODM3u8DirectoryName;
	string tarGzStagingLiveRecorderVirtualVODPathName;
	try
	{
		{
			// virtualVODM3u8DirectoryName = to_string(liveRecorderIngestionJobKey)
			// 	+ "_liveRecorderVirtualVOD"
			// ;
			{
				size_t endOfPathIndex = stagingLiveRecorderVirtualVODPathName.find_last_of("/");
				if (endOfPathIndex == string::npos)
				{
					string errorMessage = string("No stagingLiveRecorderVirtualVODPathName found")
						+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
						+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
						+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName 
					;
					_logger->error(__FILEREF__ + errorMessage);
          
					throw runtime_error(errorMessage);
				}
				// stagingLiveRecorderVirtualVODPathName is initialized in EncoderVideoAudioProxy.cpp
				// and virtualVODM3u8DirectoryName will be the name of the directory of the m3u8 of the Virtual VOD
				// In case of externalEncoder, since PUSH is used, virtualVODM3u8DirectoryName has to be 'content'
				// (see the Add-Content Task documentation). For this reason, in EncoderVideoAudioProxy.cpp,
				// 'content' is used
				virtualVODM3u8DirectoryName =
					stagingLiveRecorderVirtualVODPathName.substr(endOfPathIndex + 1);
			}


			if (stagingLiveRecorderVirtualVODPathName != ""
				&& FileIO::directoryExisting(stagingLiveRecorderVirtualVODPathName))
			{
				_logger->info(__FILEREF__ + "Remove directory"
					+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
				);
				bool removeRecursively = true;
				FileIO::removeDirectory(stagingLiveRecorderVirtualVODPathName, removeRecursively);
			}
		}

		string sourceManifestPathFileName = sourceSegmentsDirectoryPathName + "/" +
			sourceManifestFileName;
		if (!FileIO::isFileExisting (sourceManifestPathFileName.c_str()))
		{
			string errorMessage = string("manifest file not existing")
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", sourceManifestPathFileName: " + sourceManifestPathFileName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		// 2021-05-30: it is not a good idea to copy all the directory (manifest and ts files) because
		//	ffmpeg is not accurate to remove the obsolete ts files, so we will have the manifest files
		//	having for example 300 ts references but the directory contains thousands of ts files.
		//	So we will copy only the manifest file and ONLY the ts files referenced into the manifest file

		/*
		// copy manifest and TS files into the stagingLiveRecorderVirtualVODPathName
		{
			_logger->info(__FILEREF__ + "Coping directory"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", sourceSegmentsDirectoryPathName: " + sourceSegmentsDirectoryPathName
				+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
			);

			chrono::system_clock::time_point startCoping = chrono::system_clock::now();
			FileIO::copyDirectory(sourceSegmentsDirectoryPathName, stagingLiveRecorderVirtualVODPathName,
					S_IRUSR | S_IWUSR | S_IXUSR |                                                                 
                  S_IRGRP | S_IXGRP |                                                                           
                  S_IROTH | S_IXOTH);
			chrono::system_clock::time_point endCoping = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Copied directory"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", @MMS COPY statistics@ - copingDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endCoping - startCoping).count()) + "@"
			);
		}
		*/

		// 2022-05-26: non dovrebbe accadere ma, a volte, capita che il file ts non esiste, perchè eliminato
		//	da ffmpeg, ma risiede ancora nel manifest. Per evitare quindi che la generazione del virtualVOD
		//	si blocchi, consideriamo come se il manifest avesse solamente i segmenti successivi
		//	La copia quindi del manifest originale viene fatta su un file temporaneo e gestiamo
		//	noi il manifest "definitivo"
		// 2022-05-27: Probabilmente era il crontab che rimuoveva i segmenti e causava il problema
		//	descritto sopra. Per cui, fissato il retention del crontab, mantenere la playlist originale
		//	probabilmente va bene. Ormai lasciamo cosi visto che funziona ed è piu robusto nel caso in cui
		//	un segmento venisse eliminato

		string tmpManifestPathFileName = stagingLiveRecorderVirtualVODPathName + "/" +
			sourceManifestFileName + ".tmp";
		string destManifestPathFileName = stagingLiveRecorderVirtualVODPathName + "/" +
			sourceManifestFileName;

		// create the destination directory and copy the manifest file
		{
			if (!FileIO::directoryExisting(stagingLiveRecorderVirtualVODPathName))
			{
				bool noErrorIfExists = true;
				bool recursive = true;
				_logger->info(__FILEREF__ + "Creating directory"
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
				);
				FileIO::createDirectory(stagingLiveRecorderVirtualVODPathName,
					S_IRUSR | S_IWUSR | S_IXUSR |
					S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
			}

			_logger->info(__FILEREF__ + "Coping"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", sourceManifestPathFileName: " + sourceManifestPathFileName
				+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
			);
			FileIO::copyFile(sourceManifestPathFileName, tmpManifestPathFileName);
		}

		if (!FileIO::isFileExisting (tmpManifestPathFileName.c_str()))
		{
			string errorMessage = string("manifest file not existing")
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		ofstream ofManifestFile(destManifestPathFileName, ofstream::trunc);

		// read start time of the first segment
		// read start time and duration of the last segment
		// copy ts file into the directory
		double firstSegmentDuration = -1.0;
		int64_t firstSegmentUtcStartTimeInMillisecs = -1;
		double lastSegmentDuration = -1.0;
		int64_t lastSegmentUtcStartTimeInMillisecs = -1;
		{
			_logger->info(__FILEREF__ + "Reading copied manifest file"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
			);

			ifstream ifManifestFile(tmpManifestPathFileName);
			if (!ifManifestFile.is_open())
			{
				string errorMessage = string("Not authorized: manifest file not opened")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			string firstPartOfManifest;
			string manifestLine;
			{
				while(getline(ifManifestFile, manifestLine))
				{
					// #EXTM3U
					// #EXT-X-VERSION:3
					// #EXT-X-TARGETDURATION:19
					// #EXT-X-MEDIA-SEQUENCE:0
					// #EXTINF:10.000000,
					// #EXT-X-PROGRAM-DATE-TIME:2021-02-26T15:41:15.477+0100
					// liveRecorder_760504_1653579715.ts
					// ...

					string extInfPrefix ("#EXTINF:");
					string programDatePrefix = "#EXT-X-PROGRAM-DATE-TIME:";
					if (manifestLine.size() >= extInfPrefix.size()
						&& 0 == manifestLine.compare(0, extInfPrefix.size(), extInfPrefix))
					{
						break;
					}
					else if (manifestLine.size() >= programDatePrefix.size()
						&& 0 == manifestLine.compare(0, programDatePrefix.size(), programDatePrefix))
						break;
					else if (manifestLine[0] != '#')
					{
						break;
					}
					else
					{
						firstPartOfManifest += (manifestLine + "\n");
					}
				}
			}

			ofManifestFile << firstPartOfManifest;

			segmentsNumber = 0;
			do
			{
				// #EXTM3U
				// #EXT-X-VERSION:3
				// #EXT-X-TARGETDURATION:19
				// #EXT-X-MEDIA-SEQUENCE:0
				// #EXTINF:10.000000,
				// #EXT-X-PROGRAM-DATE-TIME:2021-02-26T15:41:15.477+0100
				// liveRecorder_760504_1653579715.ts
				// ...

				string extInfPrefix ("#EXTINF:");
				string programDatePrefix = "#EXT-X-PROGRAM-DATE-TIME:";
				if (manifestLine.size() >= extInfPrefix.size()
					&& 0 == manifestLine.compare(0, extInfPrefix.size(), extInfPrefix))
				{
					size_t endOfSegmentDuration = manifestLine.find(",");
					if (endOfSegmentDuration == string::npos)
					{
						string errorMessage = string("wrong manifest line format")
							+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
							+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
							+ ", manifestLine: " + manifestLine
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}

					lastSegmentDuration = stod(manifestLine.substr(extInfPrefix.size(),
						endOfSegmentDuration - extInfPrefix.size()));
				}
				else if (manifestLine.size() >= programDatePrefix.size()
					&& 0 == manifestLine.compare(0, programDatePrefix.size(), programDatePrefix))
					lastSegmentUtcStartTimeInMillisecs = DateTime::sDateMilliSecondsToUtc(manifestLine.substr(programDatePrefix.size()));
				else if (manifestLine != "" && manifestLine[0] != '#')
				{
					string sourceTSPathFileName = sourceSegmentsDirectoryPathName + "/" +
						manifestLine;
					string copiedTSPathFileName = stagingLiveRecorderVirtualVODPathName + "/" +
						manifestLine;

					try
					{
						_logger->info(__FILEREF__ + "Coping"
							+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
							+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
							+ ", sourceTSPathFileName: " + sourceTSPathFileName
							+ ", copiedTSPathFileName: " + copiedTSPathFileName
						);
						FileIO::copyFile(sourceTSPathFileName, copiedTSPathFileName);
					}
					catch(runtime_error e)
					{
						string errorMessage =
							string("copyFile failed, previous segments of the manifest will be omitted")
							+ ", sourceTSPathFileName: " + sourceTSPathFileName
							+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
							+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
							+ ", e.what: " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);

						ofManifestFile.close();

						ofManifestFile.open(destManifestPathFileName, ofstream::trunc);
						ofManifestFile << firstPartOfManifest;

						firstSegmentDuration = -1.0;
						firstSegmentUtcStartTimeInMillisecs = -1;
						lastSegmentDuration = -1.0;
						lastSegmentUtcStartTimeInMillisecs = -1;

						segmentsNumber = 0;

						continue;
					}

					segmentsNumber++;
				}

				if (firstSegmentDuration == -1.0 && firstSegmentUtcStartTimeInMillisecs == -1
					&& lastSegmentDuration != -1.0 && lastSegmentUtcStartTimeInMillisecs != -1)
				{
					firstSegmentDuration = lastSegmentDuration;
					firstSegmentUtcStartTimeInMillisecs = lastSegmentUtcStartTimeInMillisecs;
				}

				ofManifestFile << manifestLine << endl;
			}
			while(getline(ifManifestFile, manifestLine));
		}
		utcStartTimeInMilliSecs = firstSegmentUtcStartTimeInMillisecs;
		utcEndTimeInMilliSecs = lastSegmentUtcStartTimeInMillisecs + (lastSegmentDuration * 1000);

		// add end list to manifest file
		{
			_logger->info(__FILEREF__ + "Add end manifest line to copied manifest file"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", tmpManifestPathFileName: " + tmpManifestPathFileName
			);

			// string endLine = "\n";
			ofManifestFile << endl << "#EXT-X-ENDLIST" << endl;
			ofManifestFile.close();
		}

		if (segmentsNumber == 0)
		{
			string errorMessage = string("No segments found")
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", sourceManifestPathFileName: " + sourceManifestPathFileName 
				+ ", destManifestPathFileName: " + destManifestPathFileName 
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			string executeCommand;
			try
			{
				tarGzStagingLiveRecorderVirtualVODPathName = stagingLiveRecorderVirtualVODPathName + ".tar.gz";

				size_t endOfPathIndex = stagingLiveRecorderVirtualVODPathName.find_last_of("/");
				if (endOfPathIndex == string::npos)
				{
					string errorMessage = string("No stagingLiveRecorderVirtualVODDirectory found")
						+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
						+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
						+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName 
					;
					_logger->error(__FILEREF__ + errorMessage);
          
					throw runtime_error(errorMessage);
				}
				string stagingLiveRecorderVirtualVODDirectory =
					stagingLiveRecorderVirtualVODPathName.substr(0, endOfPathIndex);

				executeCommand =
					"tar cfz " + tarGzStagingLiveRecorderVirtualVODPathName
					+ " -C " + stagingLiveRecorderVirtualVODDirectory
					+ " " + virtualVODM3u8DirectoryName;
				_logger->info(__FILEREF__ + "Start tar command "
					+ ", executeCommand: " + executeCommand
				);
				chrono::system_clock::time_point startTar = chrono::system_clock::now();
				int executeCommandStatus = ProcessUtility::execute(executeCommand);
				chrono::system_clock::time_point endTar = chrono::system_clock::now();
				_logger->info(__FILEREF__ + "End tar command "
					+ ", executeCommand: " + executeCommand
					+ ", @MMS statistics@ - tarDuration (millisecs): @"
						+ to_string(chrono::duration_cast<chrono::milliseconds>(endTar - startTar).count()) + "@"
				);
				if (executeCommandStatus != 0)
				{
					string errorMessage = string("ProcessUtility::execute failed")
						+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
						+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
						+ ", executeCommandStatus: " + to_string(executeCommandStatus) 
						+ ", executeCommand: " + executeCommand 
					;
					_logger->error(__FILEREF__ + errorMessage);
          
					throw runtime_error(errorMessage);
				}

				{
					_logger->info(__FILEREF__ + "Remove directory"
						+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
					);
					bool removeRecursively = true;
					FileIO::removeDirectory(stagingLiveRecorderVirtualVODPathName, removeRecursively);
				}
			}
			catch(runtime_error e)
			{
				string errorMessage = string("tar command failed")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", executeCommand: " + executeCommand 
				;
				_logger->error(__FILEREF__ + errorMessage);
         
				throw runtime_error(errorMessage);
			}
		}
	}
	catch(runtime_error e)
	{
		string errorMessage = string("build the live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& FileIO::fileExisting(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			FileIO::remove(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		if (stagingLiveRecorderVirtualVODPathName != ""
			&& FileIO::directoryExisting(stagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove directory"
				+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
			);
			bool removeRecursively = true;
			FileIO::removeDirectory(stagingLiveRecorderVirtualVODPathName, removeRecursively);
		}

		throw runtime_error(errorMessage);
	}
	catch(exception e)
	{
		string errorMessage = string("build the live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& FileIO::fileExisting(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			FileIO::remove(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		if (stagingLiveRecorderVirtualVODPathName != ""
			&& FileIO::directoryExisting(stagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove directory"
				+ ", stagingLiveRecorderVirtualVODPathName: " + stagingLiveRecorderVirtualVODPathName
			);
			bool removeRecursively = true;
			FileIO::removeDirectory(stagingLiveRecorderVirtualVODPathName, removeRecursively);
		}

		throw runtime_error(errorMessage);
	}


	// build workflow
	string workflowMetadata;
	try
	{
		workflowMetadata = liveRecorder_buildVirtualVODIngestionWorkflow(
			liveRecorderIngestionJobKey,
			liveRecorderEncodingJobKey,
			externalEncoder,

			utcStartTimeInMilliSecs,
			utcEndTimeInMilliSecs,
			deliveryCode,
			liveRecorderIngestionJobLabel,
			tarGzStagingLiveRecorderVirtualVODPathName,
			liveRecorderVirtualVODUniqueName,
			liveRecorderVirtualVODRetention,
			liveRecorderVirtualVODImageMediaItemKey);
	}
	catch (runtime_error e)
	{
		string errorMessage = string("build workflowMetadata live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& FileIO::fileExisting(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			FileIO::remove(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string("build workflowMetadata live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& FileIO::fileExisting(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			FileIO::remove(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}

	// ingest the Live Recorder VOD
	int64_t addContentIngestionJobKey = -1;
	try
	{
		string sResponse = MMSCURL::httpPostPutString(
			liveRecorderIngestionJobKey,
			mmsWorkflowIngestionURL,
			"POST",	// requestType
			_mmsAPITimeoutInSeconds,
			to_string(liveRecorderUserKey),
			liveRecorderApiKey,
			workflowMetadata,
			"application/json",	// contentType
			_logger
		);

		if (externalEncoder)
		{
			addContentIngestionJobKey = getAddContentIngestionJobKey(
				liveRecorderIngestionJobKey, sResponse);
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = string("ingest live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& FileIO::fileExisting(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			FileIO::remove(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string("ingest live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
		;
		_logger->error(__FILEREF__ + errorMessage);

		if (tarGzStagingLiveRecorderVirtualVODPathName != ""
			&& FileIO::fileExisting(tarGzStagingLiveRecorderVirtualVODPathName))
		{
			_logger->info(__FILEREF__ + "Remove"
				+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
			);
			FileIO::remove(tarGzStagingLiveRecorderVirtualVODPathName);
		}

		throw runtime_error(errorMessage);
	}

	if (externalEncoder)
	{
		string mmsBinaryURL;
		// ingest binary
		try
		{
			if (addContentIngestionJobKey == -1)
			{
				string errorMessage =
					string("Ingested URL failed, addContentIngestionJobKey is not valid")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey) 
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			bool inCaseOfLinkHasItToBeRead = false;
			int64_t chunkFileSize = FileIO::getFileSizeInBytes(
				tarGzStagingLiveRecorderVirtualVODPathName,
				inCaseOfLinkHasItToBeRead);

			mmsBinaryURL =
				mmsBinaryIngestionURL
				+ "/" + to_string(addContentIngestionJobKey)
			;

			string sResponse = MMSCURL::httpPostPutFile(
				liveRecorderIngestionJobKey,
				mmsBinaryURL,
				"POST",	// requestType
				_mmsBinaryTimeoutInSeconds,
				to_string(liveRecorderUserKey),
				liveRecorderApiKey,
				tarGzStagingLiveRecorderVirtualVODPathName,
				chunkFileSize,
				_logger);

			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
				);
				FileIO::remove(tarGzStagingLiveRecorderVirtualVODPathName);
			}
		}
		catch (runtime_error e)
		{
			_logger->error(__FILEREF__ + "Ingestion binary failed"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey) 
				+ ", mmsBinaryURL: " + mmsBinaryURL
				+ ", workflowMetadata: " + workflowMetadata
				+ ", exception: " + e.what()
			);

			if (tarGzStagingLiveRecorderVirtualVODPathName != ""
				&& FileIO::fileExisting(tarGzStagingLiveRecorderVirtualVODPathName))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
				);
				FileIO::remove(tarGzStagingLiveRecorderVirtualVODPathName);
			}

			throw e;
		}
		catch (exception e)
		{
			_logger->error(__FILEREF__ + "Ingestion binary failed"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey) 
				+ ", mmsBinaryURL: " + mmsBinaryURL
				+ ", workflowMetadata: " + workflowMetadata
				+ ", exception: " + e.what()
			);

			if (tarGzStagingLiveRecorderVirtualVODPathName != ""
				&& FileIO::fileExisting(tarGzStagingLiveRecorderVirtualVODPathName))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", tarGzStagingLiveRecorderVirtualVODPathName: " + tarGzStagingLiveRecorderVirtualVODPathName
				);
				FileIO::remove(tarGzStagingLiveRecorderVirtualVODPathName);
			}

			throw e;
		}
	}

	return segmentsNumber;
}

long FFMPEGEncoder::getAddContentIngestionJobKey(
	int64_t ingestionJobKey,
	string ingestionResponse
)
{
	try
	{
		int64_t addContentIngestionJobKey;

		/*
		{
			"tasks" :
			[
				{
					"ingestionJobKey" : 10793,
					"label" : "Add Content test",
					"type" : "Add-Content"
				},
				{
					"ingestionJobKey" : 10794,
					"label" : "Frame Containing Face: test",
					"type" : "Face-Recognition"
				},
				...
			],
			"workflow" :
			{
				"ingestionRootKey" : 831,
				"label" : "ingestContent test"
			}
		}
		*/
		Json::Value ingestionResponseRoot;

		Json::CharReaderBuilder builder;
		Json::CharReader* reader = builder.newCharReader();
		string errors;

		bool parsingSuccessful = reader->parse(ingestionResponse.c_str(),
			ingestionResponse.c_str() + ingestionResponse.size(), 
			&ingestionResponseRoot, &errors);
		delete reader;

		if (!parsingSuccessful)
		{
			string errorMessage = __FILEREF__
				+ "ingestion workflow. Failed to parse the response body"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", errors: " + errors
				+ ", ingestionResponse: " + ingestionResponse
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		string field = "tasks";
		if (!JSONUtils::isMetadataPresent(ingestionResponseRoot, field))
		{
			string errorMessage = __FILEREF__
				"ingestion workflow. Response Body json is not well format"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", errors: " + errors
				+ ", ingestionResponse: " + ingestionResponse
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		Json::Value tasksRoot = ingestionResponseRoot[field];

		for(int taskIndex = 0; taskIndex < tasksRoot.size(); taskIndex++)
		{
			Json::Value ingestionJobRoot = tasksRoot[taskIndex];

			field = "type";
			if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				string errorMessage = __FILEREF__
					"ingestion workflow. Response Body json is not well format"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", errors: " + errors
					+ ", ingestionResponse: " + ingestionResponse
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string type = ingestionJobRoot.get(field, "").asString();

			if (type == "Add-Content")
			{
				field = "ingestionJobKey";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage = __FILEREF__
						"ingestion workflow. Response Body json is not well format"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", errors: " + errors
						+ ", ingestionResponse: " + ingestionResponse
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				addContentIngestionJobKey = JSONUtils::asInt64(ingestionJobRoot, field, -1);

				break;
			}
		}

		return addContentIngestionJobKey;
	}
	catch(...)
	{
		string errorMessage =
			string("ingestion workflow. Response Body json is not well format")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ingestionResponse: " + ingestionResponse
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

string FFMPEGEncoder::liveRecorder_buildVirtualVODIngestionWorkflow(
	int64_t liveRecorderIngestionJobKey,
	int64_t liveRecorderEncodingJobKey,
	bool externalEncoder,

	int64_t utcStartTimeInMilliSecs,
	int64_t utcEndTimeInMilliSecs,
	int64_t deliveryCode,
	string liveRecorderIngestionJobLabel,
	string tarGzStagingLiveRecorderVirtualVODPathName,
	string liveRecorderVirtualVODUniqueName,
	string liveRecorderVirtualVODRetention,
	int64_t liveRecorderVirtualVODImageMediaItemKey
)
{
	string workflowMetadata;

	try
	{
		// {
        // 	"Label": "<workflow label>",
        // 	"Type": "Workflow",
        //	"Task": {
        //        "Label": "<task label 1>",
        //        "Type": "Add-Content"
        //        "Parameters": {
        //                "FileFormat": "m3u8",
        //                "Ingester": "Giuliano",
        //                "SourceURL": "move:///abc...."
        //        },
        //	}
		// }
		Json::Value mmsDataRoot;

		// 2020-04-28: set it to liveRecordingChunk to avoid to be visible into the GUI (view MediaItems).
		//	This is because this MediaItem is not completed yet
		string field = "dataType";
		mmsDataRoot[field] = "liveRecordingVOD";

		field = "utcStartTimeInMilliSecs";
		mmsDataRoot[field] = utcStartTimeInMilliSecs;

		field = "utcEndTimeInMilliSecs";
		mmsDataRoot[field] = utcEndTimeInMilliSecs;

		string sUtcEndTimeForContentTitle;
		{
			char    utcEndTime_str [64];
			tm      tmDateTime;


			time_t utcEndTimeInSeconds = utcEndTimeInMilliSecs / 1000;

			// from utc to local time
			localtime_r (&utcEndTimeInSeconds, &tmDateTime);

			{
				sprintf (utcEndTime_str,
					"%04d-%02d-%02d %02d:%02d:%02d",
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1,
					tmDateTime. tm_mday,
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);

				string sUtcEndTime = utcEndTime_str;

				field = "utcEndTime_str";
				mmsDataRoot[field] = sUtcEndTime;
			}

			{
				sprintf (utcEndTime_str,
					"%02d:%02d:%02d",
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);

				sUtcEndTimeForContentTitle = utcEndTime_str;
			}
		}

		field = "deliveryCode";
		mmsDataRoot[field] = deliveryCode;

		Json::Value userDataRoot;

		field = "mmsData";
		userDataRoot[field] = mmsDataRoot;

		Json::Value addContentRoot;

		string addContentLabel = liveRecorderIngestionJobLabel;
			// + " V-VOD (up to " + sUtcEndTimeForContentTitle + ")";

		field = "Label";
		addContentRoot[field] = addContentLabel;

		field = "Type";
		addContentRoot[field] = "Add-Content";

		Json::Value addContentParametersRoot;

		field = "FileFormat";
		addContentParametersRoot[field] = "m3u8-tar.gz";

		if (!externalEncoder)
		{
			// 2021-05-30: changed from copy to move with the idea to have better performance
			string sourceURL = string("move") + "://" + tarGzStagingLiveRecorderVirtualVODPathName;
			field = "SourceURL";
			addContentParametersRoot[field] = sourceURL;
		}

		field = "Ingester";
		addContentParametersRoot[field] = "Live Recorder Task";

		field = "Title";
		addContentParametersRoot[field] = addContentLabel;

		field = "UniqueName";
		addContentParametersRoot[field] = liveRecorderVirtualVODUniqueName;

		field = "AllowUniqueNameOverride";
		addContentParametersRoot[field] = true;

		field = "Retention";
		addContentParametersRoot[field] = liveRecorderVirtualVODRetention;

		field = "UserData";
		addContentParametersRoot[field] = userDataRoot;

		if (liveRecorderVirtualVODImageMediaItemKey != -1)
		{
			try
			{
				/*
				bool warningIfMissing = true;
				pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemDetails =
					_mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
					workspace->_workspaceKey, liveRecorderVirtualVODImageLabel, warningIfMissing);

				int64_t liveRecorderVirtualVODImageMediaItemKey;
				tie(liveRecorderVirtualVODImageMediaItemKey, ignore) = mediaItemDetails;
				*/

				Json::Value crossReferenceRoot;

				field = "Type";
				crossReferenceRoot[field] = "VideoOfImage";

				field = "MediaItemKey";
				crossReferenceRoot[field] = liveRecorderVirtualVODImageMediaItemKey;

				field = "CrossReference";
				addContentParametersRoot[field] = crossReferenceRoot;
			}
			catch (MediaItemKeyNotFound e)
			{
				string errorMessage = string("getMediaItemKeyDetailsByUniqueName failed")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", liveRecorderVirtualVODImageMediaItemKey: " + to_string(liveRecorderVirtualVODImageMediaItemKey)
					+ ", e.what: " + e.what()
				;
				_logger->error(__FILEREF__ + errorMessage);
			}
			catch (runtime_error e)
			{
				string errorMessage = string("getMediaItemKeyDetailsByUniqueName failed")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", liveRecorderVirtualVODImageMediaItemKey: " + to_string(liveRecorderVirtualVODImageMediaItemKey)
					+ ", e.what: " + e.what()
				;
				_logger->error(__FILEREF__ + errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = string("getMediaItemKeyDetailsByUniqueName failed")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", liveRecorderVirtualVODImageMediaItemKey: " + to_string(liveRecorderVirtualVODImageMediaItemKey)
				;
				_logger->error(__FILEREF__ + errorMessage);
			}
		}

		field = "Parameters";
		addContentRoot[field] = addContentParametersRoot;


		Json::Value workflowRoot;

		field = "Label";
		workflowRoot[field] = addContentLabel + " (virtual VOD)";

		field = "Type";
		workflowRoot[field] = "Workflow";

		field = "Task";
		workflowRoot[field] = addContentRoot;

   		{
       		Json::StreamWriterBuilder wbuilder;
       		workflowMetadata = Json::writeString(wbuilder, workflowRoot);
   		}

		_logger->info(__FILEREF__ + "Live Recorder VOD Workflow metadata generated"
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", " + addContentLabel + ", "
		);

		return workflowMetadata;
	}
	catch (runtime_error e)
	{
		string errorMessage = string("build workflowMetadata live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string("build workflowMetadata live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", e.what: " + e.what()
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::liveProxyThread(
	// FCGX_Request& request,
	shared_ptr<LiveProxyAndGrid> liveProxy,
	int64_t encodingJobKey,
	string requestBody)
{
    string api = "liveProxy";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", requestBody: " + requestBody
    );

	// string tvMulticastIP;
	// string tvMulticastPort;
	// string tvType;
	// int64_t tvServiceId = -1;
	// int64_t tvFrequency = -1;
	// int64_t tvSymbolRate = -1;
	// int64_t tvBandwidthInMhz = -1;
	// string tvModulation;
	// int tvVideoPid = -1;
	// int tvAudioItalianPid = -1;
    try
    {
		liveProxy->_killedBecauseOfNotWorking = false;
		liveProxy->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value liveProxyMetadata;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &liveProxyMetadata, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

		liveProxy->_ingestionJobKey = JSONUtils::asInt64(liveProxyMetadata, "ingestionJobKey", -1);
		bool externalEncoder = JSONUtils::asBool(liveProxyMetadata, "externalEncoder", false);

		liveProxy->_outputsRoot = liveProxyMetadata["encodingParametersRoot"]["outputsRoot"];
		// liveProxy->_liveProxyOutputRoots.clear();
		{
			for(int outputIndex = 0; outputIndex < liveProxy->_outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = liveProxy->_outputsRoot[outputIndex];

				string outputType = outputRoot.get("outputType", "").asString();

				if (outputType == "HLS" || outputType == "DASH")
				{
					string manifestDirectoryPath
						= outputRoot.get("manifestDirectoryPath", "").asString();

					if (FileIO::directoryExisting(manifestDirectoryPath))
					{
						try
						{
							_logger->info(__FILEREF__ + "removeDirectory"
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
							);
							Boolean_t bRemoveRecursively = true;
							FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
						}
						catch(runtime_error e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
						catch(exception e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", manifestDirectoryPath: " + manifestDirectoryPath
								+ ", e.what(): " + e.what()
							;
							_logger->error(errorMessage);

							// throw e;
						}
					}
				}
			}
		}

		liveProxy->_ingestedParametersRoot = liveProxyMetadata["ingestedParametersRoot"];

		// non serve
		// liveProxy->_channelLabel = "";

		// liveProxy->_streamSourceType = liveProxyMetadata["encodingParametersRoot"].
		// 	get("streamSourceType", "IP_PULL").asString();

		liveProxy->_inputsRoot = liveProxyMetadata["encodingParametersRoot"]["inputsRoot"];

		for (int inputIndex = 0; inputIndex < liveProxy->_inputsRoot.size(); inputIndex++)
		{
			Json::Value inputRoot = liveProxy->_inputsRoot[inputIndex];

			if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
				continue;
			Json::Value streamInputRoot = inputRoot["streamInput"];

			string streamSourceType = streamInputRoot.get("streamSourceType", "").asString();
			if (streamSourceType == "TV")
			{
				string tvType = streamInputRoot.get("tvType", "").asString();
				int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
				int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
				int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
				int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
				string tvModulation = streamInputRoot.get("tvModulation", "").asString();
				int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
				int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
					"tvAudioItalianPid", -1);

				// In case ffmpeg crashes and is automatically restarted, it should use the same
				// IP-PORT it was using before because we already have a dbvlast sending the stream
				// to the specified IP-PORT.
				// For this reason, before to generate a new IP-PORT, let's look for the serviceId
				// inside the dvblast conf. file to see if it was already running before

				string tvMulticastIP;
				string tvMulticastPort;

				// in case there is already a serviceId running, we will use the same multicastIP-Port
				pair<string, string> tvMulticast = getTVMulticastFromDvblastConfigurationFile(
					liveProxy->_ingestionJobKey, encodingJobKey,
					tvType, tvServiceId, tvFrequency, tvSymbolRate,
					tvBandwidthInHz / 1000000,
					tvModulation);
				tie(tvMulticastIP, tvMulticastPort) = tvMulticast;

				if (tvMulticastIP == "")
				{
					lock_guard<mutex> locker(*_tvChannelsPortsMutex);

					tvMulticastIP = "239.255.1.1";
					tvMulticastPort = to_string(*_tvChannelPort_CurrentOffset
						+ _tvChannelPort_Start);

					*_tvChannelPort_CurrentOffset = (*_tvChannelPort_CurrentOffset + 1)
						% _tvChannelPort_MaxNumberOfOffsets;
				}

				// overrun_nonfatal=1 prevents ffmpeg from exiting,
				//		it can recover in most circumstances.
				// fifo_size=50000000 uses a 50MB udp input buffer (default 5MB)
				string newURL = string("udp://@") + tvMulticastIP
					+ ":" + tvMulticastPort
					+ "?overrun_nonfatal=1&fifo_size=50000000"
				;

				streamInputRoot["url"] = newURL;
				streamInputRoot["tvMulticastIP"] = tvMulticastIP;
				streamInputRoot["tvMulticastPort"] = tvMulticastPort;
				inputRoot["streamInput"] = streamInputRoot;
				liveProxy->_inputsRoot[inputIndex] = inputRoot;

				createOrUpdateTVDvbLastConfigurationFile(
					liveProxy->_ingestionJobKey, encodingJobKey,
					tvMulticastIP, tvMulticastPort,
					tvType, tvServiceId, tvFrequency, tvSymbolRate,
					tvBandwidthInHz / 1000000,
					tvModulation, tvVideoPid, tvAudioItalianPid,
					true);
			}
		}

		{
			// setting of liveProxy->_proxyStart
			// Based on liveProxy->_proxyStart, the monitor thread starts the checkings
			// In case of IP_PUSH, the checks should be done after the ffmpeg server
			// receives the stream and we do not know what it happens.
			// For this reason, in this scenario, we have to set _proxyStart in the worst scenario
			if (liveProxy->_inputsRoot.size() > 0)	// it has to be > 0
			{
				Json::Value inputRoot = liveProxy->_inputsRoot[0];

				int64_t utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, "utcScheduleStart", -1);
				// if (utcProxyPeriodStart == -1)
				// 	utcProxyPeriodStart = JSONUtils::asInt64(inputRoot, "utcProxyPeriodStart", -1);

				if (JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
				{
					Json::Value streamInputRoot = inputRoot["streamInput"];

					string streamSourceType =
						streamInputRoot.get("streamSourceType", "").asString();

					if (streamSourceType == "IP_PUSH")
					{
						int pushListenTimeout = JSONUtils::asInt(
							streamInputRoot, "pushListenTimeout", -1);

						if (utcProxyPeriodStart != -1)
						{
							if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
								chrono::system_clock::now())
								liveProxy->_proxyStart = chrono::system_clock::now() +
									chrono::seconds(pushListenTimeout);
							else
								liveProxy->_proxyStart = chrono::system_clock::from_time_t(
									utcProxyPeriodStart) +
									chrono::seconds(pushListenTimeout);
						}
						else
							liveProxy->_proxyStart = chrono::system_clock::now() +
								chrono::seconds(pushListenTimeout);
					}
					else
					{
						if (utcProxyPeriodStart != -1)
						{
							if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
									chrono::system_clock::now())
								liveProxy->_proxyStart = chrono::system_clock::now();
							else
								liveProxy->_proxyStart = chrono::system_clock::from_time_t(
									utcProxyPeriodStart);
						}
						else
							liveProxy->_proxyStart = chrono::system_clock::now();
					}
				}
				else
				{
					if (utcProxyPeriodStart != -1)
					{
						if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
								chrono::system_clock::now())
							liveProxy->_proxyStart = chrono::system_clock::now();
						else
							liveProxy->_proxyStart = chrono::system_clock::from_time_t(
								utcProxyPeriodStart);
					}
					else
						liveProxy->_proxyStart = chrono::system_clock::now();
				}
			}

			liveProxy->_ffmpeg->liveProxy2(
				liveProxy->_ingestionJobKey,
				encodingJobKey,
				externalEncoder,
				&(liveProxy->_inputsRootMutex),
				&(liveProxy->_inputsRoot),
				liveProxy->_outputsRoot,
				&(liveProxy->_childPid),
				&(liveProxy->_proxyStart)
			);
		}

		for (int inputIndex = 0; inputIndex < liveProxy->_inputsRoot.size(); inputIndex++)
		{
			Json::Value inputRoot = liveProxy->_inputsRoot[inputIndex];

			if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
				continue;
			Json::Value streamInputRoot = inputRoot["streamInput"];

			string streamSourceType = streamInputRoot.get("streamSourceType", "").asString();
			if (streamSourceType == "TV")
			{
				string tvMulticastIP = streamInputRoot.get("tvMulticastIP", "").asString();
				string tvMulticastPort = streamInputRoot.get("tvMulticastPort", "").asString();

				string tvType = streamInputRoot.get("tvType", "").asString();
				int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
				int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
				int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
				int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
				string tvModulation = streamInputRoot.get("tvModulation", "").asString();
				int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
				int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
					"tvAudioItalianPid", -1);

				if (tvServiceId != -1) // this is just to be sure variables are initialized
				{
					// remove configuration from dvblast configuration file
					createOrUpdateTVDvbLastConfigurationFile(
						liveProxy->_ingestionJobKey, encodingJobKey,
						tvMulticastIP, tvMulticastPort,
						tvType, tvServiceId, tvFrequency, tvSymbolRate,
						tvBandwidthInHz / 1000000,
						tvModulation, tvVideoPid, tvAudioItalianPid,
						false);
				}
			}
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
        liveProxy->_childPid = 0;
		liveProxy->_killedBecauseOfNotWorking = false;
        
        _logger->info(__FILEREF__ + "_ffmpeg->liveProxy finished"
			+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            // + ", liveProxy->_channelLabel: " + liveProxy->_channelLabel
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
			completedWithError, liveProxy->_errorMessage, killedByUser,
			urlForbidden, urlNotFound);

		liveProxy->_ingestionJobKey = 0;
		// liveProxy->_channelLabel = "";
		// liveProxy->_liveProxyOutputRoots.clear();

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__
					+ "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		if (liveProxy->_inputsRoot != Json::nullValue)
		{
			for (int inputIndex = 0; inputIndex < liveProxy->_inputsRoot.size(); inputIndex++)
			{
				Json::Value inputRoot = liveProxy->_inputsRoot[inputIndex];

				if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
					continue;
				Json::Value streamInputRoot = inputRoot["streamInput"];

				string streamSourceType = streamInputRoot.get("streamSourceType", "").asString();
				if (streamSourceType == "TV")
				{
					string tvMulticastIP = streamInputRoot.get("tvMulticastIP", "").asString();
					string tvMulticastPort = streamInputRoot.get("tvMulticastPort", "").asString();

					string tvType = streamInputRoot.get("tvType", "").asString();
					int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
					int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
					int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
					int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
					string tvModulation = streamInputRoot.get("tvModulation", "").asString();
					int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							liveProxy->_ingestionJobKey, encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		// liveProxy->_channelLabel = "";
		// liveProxy->_liveProxyOutputRoots.clear();

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser;
		if (liveProxy->_killedBecauseOfNotWorking)
		{
			// it was killed just because it was not working and not because of user
			// In this case the process has to be restarted soon
			killedByUser				= false;
			completedWithError			= true;
			liveProxy->_killedBecauseOfNotWorking = false;
		}
		else
		{
			killedByUser				= true;
		}
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(liveProxy->_encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(FFMpegURLForbidden e)
    {
		if (liveProxy->_inputsRoot != Json::nullValue)
		{
			for (int inputIndex = 0; inputIndex < liveProxy->_inputsRoot.size(); inputIndex++)
			{
				Json::Value inputRoot = liveProxy->_inputsRoot[inputIndex];

				if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
					continue;
				Json::Value streamInputRoot = inputRoot["streamInput"];

				string streamSourceType = streamInputRoot.get("streamSourceType", "").asString();
				if (streamSourceType == "TV")
				{
					string tvMulticastIP = streamInputRoot.get("tvMulticastIP", "").asString();
					string tvMulticastPort = streamInputRoot.get("tvMulticastPort", "").asString();

					string tvType = streamInputRoot.get("tvType", "").asString();
					int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
					int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
					int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
					int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
					string tvModulation = streamInputRoot.get("tvModulation", "").asString();
					int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							liveProxy->_ingestionJobKey, encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		// liveProxy->_channelLabel = "";
		// liveProxy->_liveProxyOutputRoots.clear();
		liveProxy->_killedBecauseOfNotWorking = false;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLForbidden)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= true;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(FFMpegURLNotFound e)
    {
		if (liveProxy->_inputsRoot != Json::nullValue)
		{
			for (int inputIndex = 0; inputIndex < liveProxy->_inputsRoot.size(); inputIndex++)
			{
				Json::Value inputRoot = liveProxy->_inputsRoot[inputIndex];

				if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
					continue;
				Json::Value streamInputRoot = inputRoot["streamInput"];

				string streamSourceType = streamInputRoot.get("streamSourceType", "").asString();
				if (streamSourceType == "TV")
				{
					string tvMulticastIP = streamInputRoot.get("tvMulticastIP", "").asString();
					string tvMulticastPort = streamInputRoot.get("tvMulticastPort", "").asString();

					string tvType = streamInputRoot.get("tvType", "").asString();
					int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
					int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
					int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
					int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
					string tvModulation = streamInputRoot.get("tvModulation", "").asString();
					int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							liveProxy->_ingestionJobKey, encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		// liveProxy->_channelLabel = "";
		// liveProxy->_liveProxyOutputRoots.clear();
		liveProxy->_killedBecauseOfNotWorking = false;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLNotFound)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= true;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
		if (liveProxy->_inputsRoot != Json::nullValue)
		{
			for (int inputIndex = 0; inputIndex < liveProxy->_inputsRoot.size(); inputIndex++)
			{
				Json::Value inputRoot = liveProxy->_inputsRoot[inputIndex];

				if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
					continue;
				Json::Value streamInputRoot = inputRoot["streamInput"];

				string streamSourceType = streamInputRoot.get("streamSourceType", "").asString();
				if (streamSourceType == "TV")
				{
					string tvMulticastIP = streamInputRoot.get("tvMulticastIP", "").asString();
					string tvMulticastPort = streamInputRoot.get("tvMulticastPort", "").asString();

					string tvType = streamInputRoot.get("tvType", "").asString();
					int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
					int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
					int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
					int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
					string tvModulation = streamInputRoot.get("tvModulation", "").asString();
					int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							liveProxy->_ingestionJobKey, encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		// liveProxy->_channelLabel = "";
		// liveProxy->_liveProxyOutputRoots.clear();
		liveProxy->_killedBecauseOfNotWorking = false;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		if (liveProxy->_inputsRoot != Json::nullValue)
		{
			for (int inputIndex = 0; inputIndex < liveProxy->_inputsRoot.size(); inputIndex++)
			{
				Json::Value inputRoot = liveProxy->_inputsRoot[inputIndex];

				if (!JSONUtils::isMetadataPresent(inputRoot, "streamInput"))
					continue;
				Json::Value streamInputRoot = inputRoot["streamInput"];

				string streamSourceType = streamInputRoot.get("streamSourceType", "").asString();
				if (streamSourceType == "TV")
				{
					string tvMulticastIP = streamInputRoot.get("tvMulticastIP", "").asString();
					string tvMulticastPort = streamInputRoot.get("tvMulticastPort", "").asString();

					string tvType = streamInputRoot.get("tvType", "").asString();
					int64_t tvServiceId = JSONUtils::asInt64(streamInputRoot, "tvServiceId", -1);
					int64_t tvFrequency = JSONUtils::asInt64(streamInputRoot, "tvFrequency", -1);
					int64_t tvSymbolRate = JSONUtils::asInt64(streamInputRoot, "tvSymbolRate", -1);
					int64_t tvBandwidthInHz = JSONUtils::asInt64(streamInputRoot, "tvBandwidthInHz", -1);
					string tvModulation = streamInputRoot.get("tvModulation", "").asString();
					int tvVideoPid = JSONUtils::asInt(streamInputRoot, "tvVideoPid", -1);
					int tvAudioItalianPid = JSONUtils::asInt(streamInputRoot,
						"tvAudioItalianPid", -1);

					if (tvServiceId != -1) // this is just to be sure variables are initialized
					{
						// remove configuration from dvblast configuration file
						createOrUpdateTVDvbLastConfigurationFile(
							liveProxy->_ingestionJobKey, encodingJobKey,
							tvMulticastIP, tvMulticastPort,
							tvType, tvServiceId, tvFrequency, tvSymbolRate,
							tvBandwidthInHz / 1000000,
							tvModulation, tvVideoPid, tvAudioItalianPid,
							false);
					}
				}
			}
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		// liveProxy->_channelLabel = "";
		// liveProxy->_liveProxyOutputRoots.clear();
		liveProxy->_killedBecauseOfNotWorking = false;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::liveGridThread(
	// FCGX_Request& request,
	shared_ptr<LiveProxyAndGrid> liveProxy,
	int64_t encodingJobKey,
	string requestBody)
{
    string api = "liveGrid";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		liveProxy->_killedBecauseOfNotWorking = false;
		liveProxy->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value liveGridMetadata;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &liveGridMetadata, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", errors: " + errors
                        + ", requestBody: " + requestBody
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(...)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

		liveProxy->_ingestionJobKey = JSONUtils::asInt64(liveGridMetadata, "ingestionJobKey", -1);

		Json::Value inputChannelsRoot = liveGridMetadata["inputChannels"];

		Json::Value encodingParametersRoot = liveGridMetadata["encodingParametersRoot"];
        Json::Value ingestedParametersRoot = liveGridMetadata["ingestedParametersRoot"];

		string userAgent;
		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, "UserAgent"))
            userAgent = ingestedParametersRoot.get("UserAgent", "").asString();
		Json::Value encodingProfileDetailsRoot = liveGridMetadata["encodingProfileDetails"];

		int gridColumns = JSONUtils::asInt(ingestedParametersRoot, "Columns", 0);
		int gridWidth = JSONUtils::asInt(ingestedParametersRoot, "GridWidth", 0);
		int gridHeight = JSONUtils::asInt(ingestedParametersRoot, "GridHeight", 0);
		liveProxy->_liveGridOutputType = encodingParametersRoot.get("outputType", "").asString();
		string srtURL = ingestedParametersRoot.get("SRT_URL", "").asString();
		int segmentDurationInSeconds = JSONUtils::asInt(encodingParametersRoot, "segmentDurationInSeconds", 10);
		int playlistEntriesNumber = JSONUtils::asInt(encodingParametersRoot, "playlistEntriesNumber", 6);
		string manifestDirectoryPath = encodingParametersRoot.get("manifestDirectoryPath", "").asString();
		string manifestFileName = encodingParametersRoot.get("manifestFileName", "").asString();
		// liveProxy->_channelLabel = manifestFileName;

		/*
		liveProxy->_manifestFilePathNames.clear();
		for(int inputChannelIndex = 0; inputChannelIndex < inputChannelsRoot.size(); inputChannelIndex++)
		{
			string audioTrackDirectoryName = to_string(inputChannelIndex) + "_audio";
			
			string audioPathName = manifestDirectoryPath + "/"
				+ audioTrackDirectoryName + "/" + manifestFileName;

			liveProxy->_manifestFilePathNames.push_back(audioPathName);
		}
		{
			string videoTrackDirectoryName = "0_video";

			string videoPathName = manifestDirectoryPath + "/"
				+ videoTrackDirectoryName + "/" + manifestFileName;

			liveProxy->_manifestFilePathNames.push_back(videoPathName);
		}
		*/

		// if (liveProxy->_outputType == "HLS") // || liveProxy->_outputType == "DASH")
		{
			if (liveProxy->_liveGridOutputType == "HLS"
				&& FileIO::directoryExisting(manifestDirectoryPath))
			{
				try
				{
					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", manifestDirectoryPath: " + manifestDirectoryPath
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
				}
				catch(runtime_error e)
				{
					string errorMessage = __FILEREF__ + "remove directory failed"
						+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", manifestDirectoryPath: " + manifestDirectoryPath
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					// throw e;
				}
				catch(exception e)
				{
					string errorMessage = __FILEREF__ + "remove directory failed"
						+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", manifestDirectoryPath: " + manifestDirectoryPath
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					// throw e;
				}
			}

			liveProxy->_proxyStart = chrono::system_clock::now();

			liveProxy->_ffmpeg->liveGrid(
				liveProxy->_ingestionJobKey,
				encodingJobKey,
				encodingProfileDetailsRoot,
				userAgent,
				inputChannelsRoot,
				gridColumns,
				gridWidth,
				gridHeight,
				liveProxy->_liveGridOutputType,
				segmentDurationInSeconds,
				playlistEntriesNumber,
				manifestDirectoryPath,
				manifestFileName,
				srtURL,
				&(liveProxy->_childPid));
		}
		/*
		else
		{
			liveProxy->_proxyStart = chrono::system_clock::now();

			liveProxy->_ffmpeg->liveGridByCDN(
				liveProxy->_ingestionJobKey,
				encodingJobKey,
				liveURL, userAgent, inputTimeOffset,
				otherOutputOptions,
				cdnURL,
				&(liveProxy->_childPid));
		}
		*/

        liveProxy->_running = false;
		liveProxy->_method = "";
        liveProxy->_childPid = 0;
		liveProxy->_killedBecauseOfNotWorking = false;
        
        _logger->info(__FILEREF__ + "_ffmpeg->liveGridBy... finished"
			+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            // + ", liveProxy->_channelLabel: " + liveProxy->_channelLabel
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		liveProxy->_ingestionJobKey = 0;
		liveProxy->_liveGridOutputType = "";
		// liveProxy->_channelLabel = "";

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_liveGridOutputType = "";
		// liveProxy->_channelLabel = "";

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser;
		if (liveProxy->_killedBecauseOfNotWorking)
		{
			// it was killed just because it was not working and not because of user
			// In this case the process has to be restarted soon
			killedByUser				= false;
			completedWithError			= true;
			liveProxy->_killedBecauseOfNotWorking = false;
		}
		else
		{
			killedByUser				= true;
		}
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(liveProxy->_encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(FFMpegURLForbidden e)
    {
        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_liveGridOutputType = "";
		// liveProxy->_channelLabel = "";
		liveProxy->_killedBecauseOfNotWorking = false;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLForbidden)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= true;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(FFMpegURLNotFound e)
    {
        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_liveGridOutputType = "";
		// liveProxy->_channelLabel = "";
		liveProxy->_killedBecauseOfNotWorking = false;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLNotFound)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= true;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_liveGridOutputType = "";
		// liveProxy->_channelLabel = "";
		liveProxy->_killedBecauseOfNotWorking = false;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_liveGridOutputType = "";
		// liveProxy->_channelLabel = "";
		liveProxy->_killedBecauseOfNotWorking = false;

		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		liveProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(liveProxy->_encodingJobKey);
			if (erase)
				_logger->info(__FILEREF__ + "_liveProxiesCapability->erase"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
			else
				_logger->error(__FILEREF__ + "_liveProxiesCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::monitorThread()
{

	while(!_monitorThreadShutdown)
	{
		// proxy
		try
		{
			// this is to have a copy of LiveProxyAndGrid
			vector<shared_ptr<LiveProxyAndGrid>> copiedRunningLiveProxiesCapability;

			// this is to have access to _running and _proxyStart
			//	to check if it is changed. In case the process is killed, it will access
			//	also to _killedBecauseOfNotWorking and _errorMessage
			vector<shared_ptr<LiveProxyAndGrid>> sourceLiveProxiesCapability;

			chrono::system_clock::time_point startClone = chrono::system_clock::now();
			// to avoid to maintain the lock too much time
			// we will clone the proxies for monitoring check
			int liveProxyAndGridRunningCounter = 0;
			{
				lock_guard<mutex> locker(*_liveProxyMutex);

				int liveProxyAndGridNotRunningCounter = 0;

				#ifdef __VECTOR__
				for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
				#else	// __MAP__
				for(map<int64_t, shared_ptr<LiveProxyAndGrid>>::iterator it =
					_liveProxiesCapability->begin(); it != _liveProxiesCapability->end(); it++)
				#endif
				{
					#ifdef __VECTOR__
					#else	// __MAP__
					shared_ptr<LiveProxyAndGrid> liveProxy = it->second;
					#endif

					if (liveProxy->_running)
					{
						liveProxyAndGridRunningCounter++;

						copiedRunningLiveProxiesCapability.push_back(
							liveProxy->cloneForMonitor());
						sourceLiveProxiesCapability.push_back(
                            liveProxy);
					}
					else
					{
						liveProxyAndGridNotRunningCounter++;
					}
				}
				_logger->info(__FILEREF__ + "liveProxyMonitor, numbers"
					+ ", total LiveProxyAndGrid: " + to_string(liveProxyAndGridRunningCounter + liveProxyAndGridNotRunningCounter)
					+ ", liveProxyAndGridRunningCounter: " + to_string(liveProxyAndGridRunningCounter)
					+ ", liveProxyAndGridNotRunningCounter: " + to_string(liveProxyAndGridNotRunningCounter)
				);
			}
			_logger->info(__FILEREF__ + "liveProxyMonitor clone"
				+ ", copiedRunningLiveProxiesCapability.size: " + to_string(copiedRunningLiveProxiesCapability.size())
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - startClone).count())
			);

			chrono::system_clock::time_point monitorStart = chrono::system_clock::now();

			for (int liveProxyIndex = 0;
				liveProxyIndex < copiedRunningLiveProxiesCapability.size();
				liveProxyIndex++)
			{
				shared_ptr<LiveProxyAndGrid> copiedLiveProxy
					= copiedRunningLiveProxiesCapability[liveProxyIndex];
				shared_ptr<LiveProxyAndGrid> sourceLiveProxy
					= sourceLiveProxiesCapability[liveProxyIndex];

				// this is just for logging
				string configurationLabel;
				if (copiedLiveProxy->_inputsRoot.size() > 0)
				{
					Json::Value inputRoot = copiedLiveProxy->_inputsRoot[0];
					string field = "streamInput";
					if (JSONUtils::isMetadataPresent(inputRoot, field))
					{
						Json::Value streamInputRoot = inputRoot[field];
						field = "configurationLabel";
						if (JSONUtils::isMetadataPresent(streamInputRoot, field))
							configurationLabel = streamInputRoot.
								get(field, "").asString();
					}
				}

				_logger->info(__FILEREF__ + "liveProxyMonitor start"
					+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
					+ ", configurationLabel: " + configurationLabel
					+ ", sourceLiveProxy->_running: " + to_string(sourceLiveProxy->_running)
					+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): "
						+ to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
					+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): "
						+ to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
				);

				chrono::system_clock::time_point now = chrono::system_clock::now();

				bool liveProxyWorking = true;
				string localErrorMessage;

				if (!sourceLiveProxy->_running ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_running: " + to_string(sourceLiveProxy->_running)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				{
					// copiedLiveProxy->_proxyStart could be a bit in the future
					int64_t liveProxyLiveTimeInMinutes;
					if (now > copiedLiveProxy->_proxyStart)
						liveProxyLiveTimeInMinutes = chrono::duration_cast<
							chrono::minutes>(now - copiedLiveProxy->_proxyStart).count();
					else	// it will be negative
						liveProxyLiveTimeInMinutes = chrono::duration_cast<
							chrono::minutes>(now - copiedLiveProxy->_proxyStart).count();

					// checks are done after 3 minutes LiveProxy started,
					// in order to be sure the manifest file was already created
					if (liveProxyLiveTimeInMinutes <= 3)
					{
						_logger->info(__FILEREF__
							+ "liveProxyMonitor. Checks are not done because too early"
							+ ", ingestionJobKey: "
								+ to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", encodingJobKey: "
								+ to_string(copiedLiveProxy->_encodingJobKey)
							+ ", liveProxyLiveTimeInMinutes: "
								+ to_string(liveProxyLiveTimeInMinutes)
						);

						continue;
					}
				}

				if (!sourceLiveProxy->_running ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_running: " + to_string(sourceLiveProxy->_running)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				// First health check
				//		HLS/DASH:	kill if manifest file does not exist or was not updated in the last 30 seconds
				//		rtmp(Proxy)/SRT(Grid):	kill if it was found 'Non-monotonous DTS in output stream' and 'incorrect timestamps'
				bool rtmpOutputFound = false;
				if (liveProxyWorking)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor manifest check"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
					);

					for(int outputIndex = 0; outputIndex < copiedLiveProxy->_outputsRoot.size();
						outputIndex++)
					{
						Json::Value outputRoot = copiedLiveProxy->_outputsRoot[outputIndex];

						string outputType = outputRoot.get("outputType", "").asString();

						if (!liveProxyWorking)
							break;

						if (outputType == "HLS" || outputType == "DASH")
						{
							string manifestDirectoryPath = outputRoot
								.get("manifestDirectoryPath", "").asString();
							string manifestFileName = outputRoot
								.get("manifestFileName", "").asString();

							try
							{
								// First health check (HLS/DASH) looking the manifests path name timestamp
								{
									string manifestFilePathName =
										manifestDirectoryPath + "/" + manifestFileName;
									{
										if(!FileIO::fileExisting(manifestFilePathName))
										{
											liveProxyWorking = false;

											_logger->error(__FILEREF__ + "liveProxyMonitor. Manifest file does not exist"
												+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestFilePathName: " + manifestFilePathName
											);

											localErrorMessage = " restarted because of 'manifest file is missing'";

											break;
										}
										else
										{
											time_t utcManifestFileLastModificationTime = 0;

											FileIO::getFileTime (manifestFilePathName.c_str(),
												&utcManifestFileLastModificationTime);

											unsigned long long	ullNow = 0;
											unsigned long		ulAdditionalMilliSecs;
											long				lTimeZoneDifferenceInHours;

											DateTime:: nowUTCInMilliSecs (&ullNow, &ulAdditionalMilliSecs,
												&lTimeZoneDifferenceInHours);

											long maxLastManifestFileUpdateInSeconds = 30;

											unsigned long long lastManifestFileUpdateInSeconds = ullNow - utcManifestFileLastModificationTime;
											if (lastManifestFileUpdateInSeconds > maxLastManifestFileUpdateInSeconds)
											{
												liveProxyWorking = false;

												_logger->error(__FILEREF__ + "liveProxyMonitor. Manifest file was not updated "
													+ "in the last " + to_string(maxLastManifestFileUpdateInSeconds) + " seconds"
													+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
													+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
													+ ", manifestFilePathName: " + manifestFilePathName
													+ ", lastManifestFileUpdateInSeconds: " + to_string(lastManifestFileUpdateInSeconds) + " seconds ago"
												);

												localErrorMessage = " restarted because of 'manifest file was not updated'";

												break;
											}
										}
									}
								}
							}
							catch(runtime_error e)
							{
								string errorMessage = string ("liveProxyMonitor (HLS) on manifest path name failed")
									+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
									+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
							catch(exception e)
							{
								string errorMessage = string ("liveProxyMonitor (HLS) on manifest path name failed")
									+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
									+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
						}
						else	// rtmp (Proxy) or SRT (Grid)
						{
							rtmpOutputFound = true;
						}
					}
				}

				if (!sourceLiveProxy->_running ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_running: " + to_string(sourceLiveProxy->_running)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveProxyWorking && rtmpOutputFound)
				{
					try
					{
						_logger->info(__FILEREF__ + "liveProxyMonitor nonMonotonousDTSInOutputLog check"
							+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", configurationLabel: " + configurationLabel
						);

						// First health check (rtmp), looks the log and check there is no message like
						//	[flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to 95383372. This may result in incorrect timestamps in the output file.
						//	This message causes proxy not working
						if (sourceLiveProxy->_ffmpeg->nonMonotonousDTSInOutputLog())
						{
							liveProxyWorking = false;

							_logger->error(__FILEREF__ + "liveProxyMonitor (rtmp). Live Proxy is logging 'Non-monotonous DTS in output stream/incorrect timestamps'. LiveProxy (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
								// + ", channelLabel: " + copiedLiveProxy->_channelLabel
								+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
							);

							localErrorMessage = " restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps'";
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) Non-monotonous DTS failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) Non-monotonous DTS failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (!sourceLiveProxy->_running ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_running: " + to_string(sourceLiveProxy->_running)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				// Second health 
				//		HLS/DASH:	kill if segments were not generated
				//					frame increasing check
				//					it is also implemented the retention of segments too old (10 minutes)
				//						This is already implemented by the HLS parameters (into the ffmpeg command)
				//						We do it for the DASH option and in case ffmpeg does not work
				//		rtmp(Proxy)/SRT(Grid):		frame increasing check
				if (liveProxyWorking)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor segments check"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
					);

					for(int outputIndex = 0; outputIndex < copiedLiveProxy->_outputsRoot.size();
						outputIndex++)
					{
						Json::Value outputRoot = copiedLiveProxy->_outputsRoot[outputIndex];

						string outputType = outputRoot.get("outputType", "").asString();

						if (!liveProxyWorking)
							break;

						if (outputType == "HLS" || outputType == "DASH")
						{
							string manifestDirectoryPath = outputRoot
								.get("manifestDirectoryPath", "").asString();
							string manifestFileName = outputRoot
								.get("manifestFileName", "").asString();

							try
							{
								/*
								int64_t liveProxyLiveTimeInMinutes =
									chrono::duration_cast<chrono::minutes>(now - copiedLiveProxy->_proxyStart).count();

								// check id done after 3 minutes LiveProxy started, in order to be sure
								// segments were already created
								// 1. get the timestamp of the last generated file
								// 2. fill the vector with the chunks (pathname) to be removed because too old
								//		(10 minutes after the "capacity" of the playlist)
								// 3. kill ffmpeg in case no segments were generated
								if (liveProxyLiveTimeInMinutes > 3)
								*/
								{
									string manifestFilePathName =
										manifestDirectoryPath + "/" + manifestFileName;
									{
										vector<string>	chunksTooOldToBeRemoved;
										bool chunksWereNotGenerated = false;

										string manifestDirectoryPathName;
										{
											size_t manifestFilePathIndex = manifestFilePathName.find_last_of("/");
											if (manifestFilePathIndex == string::npos)
											{
												string errorMessage = __FILEREF__ + "liveProxyMonitor. No manifestDirectoryPath find in the m3u8/mpd file path name"
													+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
													+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
													+ ", manifestFilePathName: " + manifestFilePathName;
												_logger->error(errorMessage);

												throw runtime_error(errorMessage);
											}
											manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
										}

										chrono::system_clock::time_point lastChunkTimestamp = copiedLiveProxy->_proxyStart;
										bool firstChunkRead = false;

										try
										{
											if (FileIO::directoryExisting(manifestDirectoryPathName))
											{
												FileIO::DirectoryEntryType_t detDirectoryEntryType;
												shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (
													manifestDirectoryPathName + "/");

												// chunks will be removed 10 minutes after the "capacity" of the playlist
												// long liveProxyChunkRetentionInSeconds =
												// 	(segmentDurationInSeconds * playlistEntriesNumber)
												// 	+ 10 * 60;	// 10 minutes
												long liveProxyChunkRetentionInSeconds = 10 * 60;	// 10 minutes

												bool scanDirectoryFinished = false;
												while (!scanDirectoryFinished)
												{
													string directoryEntry;
													try
													{
														string directoryEntry = FileIO::readDirectory (directory,
														&detDirectoryEntryType);

														if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
															continue;

														string dashPrefixInitFiles ("init-stream");
														if (outputType == "DASH" &&
															directoryEntry.size() >= dashPrefixInitFiles.size()
																&& 0 == directoryEntry.compare(0, dashPrefixInitFiles.size(), dashPrefixInitFiles)
														)
															continue;

														{
															string segmentPathNameToBeRemoved =
																manifestDirectoryPathName + "/" + directoryEntry;

															chrono::system_clock::time_point fileLastModification =
																FileIO::getFileTime (segmentPathNameToBeRemoved);
															chrono::system_clock::time_point now = chrono::system_clock::now();

															if (chrono::duration_cast<chrono::seconds>(now - fileLastModification).count()
																> liveProxyChunkRetentionInSeconds)
															{
																chunksTooOldToBeRemoved.push_back(segmentPathNameToBeRemoved);
															}

															if (!firstChunkRead
																|| fileLastModification > lastChunkTimestamp)
																lastChunkTimestamp = fileLastModification;

															firstChunkRead = true;
														}
													}
													catch(DirectoryListFinished e)
													{
														scanDirectoryFinished = true;
													}
													catch(runtime_error e)
													{
														string errorMessage = __FILEREF__ + "liveProxyMonitor. listing directory failed"
															+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
															+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
															+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
															+ ", e.what(): " + e.what()
														;
														_logger->error(errorMessage);

														// throw e;
													}
													catch(exception e)
													{
														string errorMessage = __FILEREF__ + "liveProxyMonitor. listing directory failed"
															+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
															+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
															+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
															+ ", e.what(): " + e.what()
														;
														_logger->error(errorMessage);

														// throw e;
													}
												}

												FileIO::closeDirectory (directory);
											}
										}
										catch(runtime_error e)
										{
											_logger->error(__FILEREF__ + "liveProxyMonitor. scan LiveProxy files failed"
												+ ", _ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", _encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
												+ ", e.what(): " + e.what()
											);
										}
										catch(...)
										{
											_logger->error(__FILEREF__ + "liveProxyMonitor. scan LiveProxy files failed"
												+ ", _ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", _encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
											);
										}
				
										if (!firstChunkRead
											|| lastChunkTimestamp < chrono::system_clock::now() - chrono::minutes(1))
										{
											// if we are here, it means the ffmpeg command is not generating the ts files

											_logger->error(__FILEREF__ + "liveProxyMonitor. Chunks were not generated"
												+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
												+ ", firstChunkRead: " + to_string(firstChunkRead)
											);

											chunksWereNotGenerated = true;

											liveProxyWorking = false;
											localErrorMessage = " restarted because of 'no segments were generated'";

											_logger->error(__FILEREF__ + "liveProxyMonitor. ProcessUtility::kill/quitProcess. liveProxyMonitor. Live Proxy is not working (no segments were generated). LiveProxy (ffmpeg) is killed in order to be started again"
												+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
												+ ", manifestFilePathName: " + manifestFilePathName
												// + ", channelLabel: " + copiedLiveProxy->_channelLabel
												+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
											);


											// we killed the process, we do not care to remove the too old segments
											// since we will remove the entore directory
											break;
										}

										{
											bool exceptionInCaseOfError = false;

											for (string segmentPathNameToBeRemoved: chunksTooOldToBeRemoved)
											{
												try
												{
													_logger->info(__FILEREF__ + "liveProxyMonitor. Remove chunk because too old"
														+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
														+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
														+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
													FileIO::remove(segmentPathNameToBeRemoved, exceptionInCaseOfError);
												}
												catch(runtime_error e)
												{
													_logger->error(__FILEREF__ + "liveProxyMonitor. remove failed"
														+ ", _ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
														+ ", _encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
														+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved
														+ ", e.what(): " + e.what()
													);
												}
											}
										}
									}
								}
							}
							catch(runtime_error e)
							{
								string errorMessage = string ("liveProxyMonitor (HLS) on segments (and retention) failed")
									+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
									+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
										+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
							catch(exception e)
							{
								string errorMessage = string ("liveProxyMonitor (HLS) on segments (and retention) failed")
									+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
									+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
						}
					}
				}

				if (!sourceLiveProxy->_running ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_running: " + to_string(sourceLiveProxy->_running)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveProxyWorking) // && rtmpOutputFound)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor isFrameIncreasing check"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
					);

					try
					{
						// Second health check, rtmp(Proxy)/SRT(Grid), looks if the frame is increasing
						int maxMilliSecondsToWait = 3000;
						if (!sourceLiveProxy->_ffmpeg->isFrameIncreasing(maxMilliSecondsToWait))
						{
							_logger->error(__FILEREF__ + "liveProxyMonitor. ProcessUtility::kill/quitProcess. liveProxyMonitor (rtmp). Live Proxy frame is not increasing'. LiveProxy (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
								+ ", configurationLabel: " + configurationLabel
								+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
							);

							liveProxyWorking = false;

							localErrorMessage = " restarted because of 'frame is not increasing'";
						}
					}
					catch(FFMpegEncodingStatusNotAvailable e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) frame increasing check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->warn(__FILEREF__ + errorMessage);
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) frame increasing check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) frame increasing check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (!sourceLiveProxy->_running ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_running: " + to_string(sourceLiveProxy->_running)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveProxyWorking) // && rtmpOutputFound)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor forbiddenErrorInOutputLog check"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
					);

					try
					{
						if (sourceLiveProxy->_ffmpeg->forbiddenErrorInOutputLog())
						{
							_logger->error(__FILEREF__ + "liveProxyMonitor. ProcessUtility::kill/quitProcess. liveProxyMonitor (rtmp). Live Proxy is returning 'HTTP error 403 Forbidden'. LiveProxy (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
								// + ", channelLabel: " + copiedLiveProxy->_channelLabel
								+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
							);

							liveProxyWorking = false;
							localErrorMessage = " restarted because of 'HTTP error 403 Forbidden'";
						}
					}
					catch(FFMpegEncodingStatusNotAvailable e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) HTTP error 403 Forbidden check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->warn(__FILEREF__ + errorMessage);
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) HTTP error 403 Forbidden check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", configurationLabel: " + configurationLabel
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveProxyMonitor (rtmp) HTTP error 403 Forbidden check failed")
							+ ", copiedLiveProxy->_ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", copiedLiveProxy->_encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", configurationLabel: " + configurationLabel
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (!sourceLiveProxy->_running ||
					copiedLiveProxy->_proxyStart != sourceLiveProxy->_proxyStart)
				{
					_logger->info(__FILEREF__ + "liveProxyMonitor. LiveProxy changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", sourceLiveProxy->_running: " + to_string(sourceLiveProxy->_running)
						+ ", copiedLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(copiedLiveProxy->_proxyStart.time_since_epoch().count())
						+ ", sourceLiveProxy->_proxyStart.time_since_epoch().count(): " + to_string(sourceLiveProxy->_proxyStart.time_since_epoch().count())
					);

					continue;
				}

				if (!liveProxyWorking)
				{
					_logger->error(__FILEREF__ + "liveProxyMonitor. ProcessUtility::kill/quitProcess. liveProxyMonitor. LiveProxy (ffmpeg) is killed/quit in order to be started again"
						+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", localErrorMessage: " + localErrorMessage
						// + ", channelLabel: " + copiedLiveProxy->_channelLabel
						+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
					);

					try
					{
						// 2021-12-14: switched from quit to kill because it seems
						//		ffmpeg didn't terminate (in case of quit) when he was
						//		failing. May be because it could not finish his sample/frame
						//		to process. The result is that the channels were not restarted.
						//		This is an ipothesys, not 100% sure
						// 2022-11-02: SIGQUIT is managed inside FFMpeg.cpp by liveProxy
						// ProcessUtility::killProcess(sourceLiveProxy->_childPid);
						// sourceLiveProxy->_killedBecauseOfNotWorking = true;
						ProcessUtility::quitProcess(sourceLiveProxy->_childPid);
						{
							char strDateTime [64];
							{
								time_t utcTime = chrono::system_clock::to_time_t(
									chrono::system_clock::now());
								tm tmDateTime;
								localtime_r (&utcTime, &tmDateTime);
								sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
									tmDateTime. tm_year + 1900,
									tmDateTime. tm_mon + 1,
									tmDateTime. tm_mday,
									tmDateTime. tm_hour,
									tmDateTime. tm_min,
									tmDateTime. tm_sec);
							}
							sourceLiveProxy->_errorMessage = string(strDateTime) + " "
								// + liveProxy->_channelLabel
								+ localErrorMessage;
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = string("liveProxyMonitor. ProcessUtility::kill/quit Process failed")
							+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
							+ ", configurationLabel: " + configurationLabel
							+ ", copiedLiveProxy->_childPid: " + to_string(copiedLiveProxy->_childPid)
							+ ", e.what(): " + e.what()
								;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				_logger->info(__FILEREF__ + "liveProxyMonitor "
					+ to_string(liveProxyIndex) + "/" + to_string(liveProxyAndGridRunningCounter)
					+ ", ingestionJobKey: " + to_string(copiedLiveProxy->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveProxy->_encodingJobKey)
					+ ", configurationLabel: " + configurationLabel
					+ ", @MMS statistics@ - elapsed time: @" + to_string(
						chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - now).count()) + "@"
				);
			}
			_logger->info(__FILEREF__ + "liveProxyMonitor"
				+ ", liveProxyAndGridRunningCounter: " + to_string(liveProxyAndGridRunningCounter)
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - monitorStart).count())
			);
		}
		catch(runtime_error e)
		{
			string errorMessage = string ("liveProxyMonitor failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("liveProxyMonitor failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}

		// recording
		try
		{
			// this is to have a copy of LiveRecording
			vector<shared_ptr<LiveRecording>> copiedRunningLiveRecordingCapability;

			// this is to have access to _running and _proxyStart
			//	to check if it is changed. In case the process is killed, it will access
			//	also to _killedBecauseOfNotWorking and _errorMessage
			vector<shared_ptr<LiveRecording>> sourceLiveRecordingCapability;

			chrono::system_clock::time_point startClone = chrono::system_clock::now();
			// to avoid to maintain the lock too much time
			// we will clone the proxies for monitoring check
			int liveRecordingRunningCounter = 0;
			{
				lock_guard<mutex> locker(*_liveRecordingMutex);

				int liveRecordingNotRunningCounter = 0;

				#ifdef __VECTOR__
				for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
				#else	// __MAP__
				for(map<int64_t, shared_ptr<LiveRecording>>::iterator it =
					_liveRecordingsCapability->begin(); it != _liveRecordingsCapability->end(); it++)
				#endif
				{
					#ifdef __VECTOR__
					#else	// __MAP__
					shared_ptr<LiveRecording> liveRecording = it->second;
					#endif

					if (liveRecording->_running && liveRecording->_monitoringEnabled)
					{
						liveRecordingRunningCounter++;

						copiedRunningLiveRecordingCapability.push_back(
							liveRecording->cloneForMonitorAndVirtualVOD());
						sourceLiveRecordingCapability.push_back(
                            liveRecording);
					}
					else
					{
						liveRecordingNotRunningCounter++;
					}
				}
				_logger->info(__FILEREF__ + "liveRecordingMonitor, numbers"
					+ ", total LiveRecording: " + to_string(liveRecordingRunningCounter
						+ liveRecordingNotRunningCounter)
					+ ", liveRecordingRunningCounter: " + to_string(liveRecordingRunningCounter)
					+ ", liveRecordingNotRunningCounter: " + to_string(liveRecordingNotRunningCounter)
				);
			}
			_logger->info(__FILEREF__ + "liveRecordingMonitor clone"
				+ ", copiedRunningLiveRecordingCapability.size: " + to_string(copiedRunningLiveRecordingCapability.size())
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - startClone).count())
			);

			chrono::system_clock::time_point monitorStart = chrono::system_clock::now();

			for (int liveRecordingIndex = 0;
				liveRecordingIndex < copiedRunningLiveRecordingCapability.size();
				liveRecordingIndex++)
			{
				shared_ptr<LiveRecording> copiedLiveRecording
					= copiedRunningLiveRecordingCapability[liveRecordingIndex];
				shared_ptr<LiveRecording> sourceLiveRecording
					= sourceLiveRecordingCapability[liveRecordingIndex];

				_logger->info(__FILEREF__ + "liveRecordingMonitor"
					+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
					+ ", channelLabel: " + copiedLiveRecording->_channelLabel
				);

				chrono::system_clock::time_point now = chrono::system_clock::now();

				bool liveRecorderWorking = true;
				string localErrorMessage;

				if (!sourceLiveRecording->_running ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_running: " + to_string(sourceLiveRecording->_running)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				// copiedLiveRecording->_recordingStart could be a bit in the future
				int64_t liveRecordingLiveTimeInMinutes;
				if (now > copiedLiveRecording->_recordingStart)
					liveRecordingLiveTimeInMinutes = chrono::duration_cast<chrono::minutes>(
						now - copiedLiveRecording->_recordingStart).count();
				else
					liveRecordingLiveTimeInMinutes = 0;

				int segmentDurationInSeconds;
				string field = "segmentDurationInSeconds";
				segmentDurationInSeconds = JSONUtils::asInt(copiedLiveRecording->_encodingParametersRoot, field, 0);

				// check is done after 5 minutes + segmentDurationInSeconds LiveRecording started,
				// in order to be sure the file was already created
				if (liveRecordingLiveTimeInMinutes <= (segmentDurationInSeconds / 60) + 5)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. Checks are not done because too early"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", liveRecordingLiveTimeInMinutes: "
							+ to_string(liveRecordingLiveTimeInMinutes)
						+ ", (segmentDurationInSeconds / 60) + 5: "
							+ to_string((segmentDurationInSeconds / 60) + 5)
					);

					continue;
				}

				if (!sourceLiveRecording->_running ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_running: " + to_string(sourceLiveRecording->_running)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				// First health check
				//		kill if 1840699_408620.liveRecorder.list file does not exist or was not updated in the last (2 * segment duration in secs) seconds
				if (liveRecorderWorking)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. liveRecorder.list check"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					);

					try
					{
						// looking the manifests path name timestamp

						string segmentListPathName = copiedLiveRecording->_transcoderStagingContentsPath
							+ copiedLiveRecording->_segmentListFileName;

						{
							// 2022-05-26: in case the file does not exist, try again to make sure
							//	it really does not exist
							bool segmentListFileExistence = FileIO::fileExisting(segmentListPathName);

							if (!segmentListFileExistence)
							{
								int sleepTimeInSeconds = 5;

								_logger->warn(__FILEREF__
									+ "liveRecordingMonitor. Segment list file does not exist, let's check again"
									+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
									+ ", segmentListPathName: " + segmentListPathName
									+ ", sleepTimeInSeconds: " + to_string(sleepTimeInSeconds)
								);

								this_thread::sleep_for(chrono::seconds(sleepTimeInSeconds));

								segmentListFileExistence = FileIO::fileExisting(segmentListPathName);
							}

							if(!segmentListFileExistence)
							{
								liveRecorderWorking = false;

								_logger->error(__FILEREF__ + "liveRecordingMonitor. Segment list file does not exist"
									+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
									+ ", segmentListPathName: " + segmentListPathName
								);

								localErrorMessage = " restarted because of 'segment list file is missing or was not updated'";
							}
							else
							{
								time_t utcSegmentListFileLastModificationTime;

								FileIO::getFileTime (segmentListPathName.c_str(),
									&utcSegmentListFileLastModificationTime);

								unsigned long long	ullNow = 0;
								unsigned long		ulAdditionalMilliSecs;
								long				lTimeZoneDifferenceInHours;

								DateTime:: nowUTCInMilliSecs (&ullNow, &ulAdditionalMilliSecs,
									&lTimeZoneDifferenceInHours);

								long maxLastSegmentListFileUpdateInSeconds
									= segmentDurationInSeconds * 2;

								unsigned long long lastSegmentListFileUpdateInSeconds
									= ullNow - utcSegmentListFileLastModificationTime;
								if (lastSegmentListFileUpdateInSeconds
									> maxLastSegmentListFileUpdateInSeconds)
								{
									liveRecorderWorking = false;

									_logger->error(__FILEREF__ + "liveRecordingMonitor. Segment list file was not updated "
										+ "in the last " + to_string(maxLastSegmentListFileUpdateInSeconds) + " seconds"
										+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
										+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
										+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
										+ ", segmentListPathName: " + segmentListPathName
										+ ", lastSegmentListFileUpdateInSeconds: " + to_string(lastSegmentListFileUpdateInSeconds) + " seconds ago"
									);

									localErrorMessage = " restarted because of 'segment list file is missing or was not updated'";
								}
							}
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveRecordingMonitor on path name failed")
							+ ", copiedLiveRecording->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", copiedLiveRecording->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveRecordingMonitor on path name failed")
							+ ", copiedLiveRecording->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", copiedLiveRecording->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (!sourceLiveRecording->_running ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_running: " + to_string(sourceLiveRecording->_running)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				// Second health check
				//		HLS/DASH:	kill if manifest file does not exist or was not updated in the last 30 seconds
				//		rtmp(Proxy):	kill if it was found 'Non-monotonous DTS in output stream' and 'incorrect timestamps'
				//			This check has to be done just once (not for each outputRoot) in case we have at least one rtmp output
				bool rtmpOutputFound = false;
				if (liveRecorderWorking)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. manifest check"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					);

					Json::Value outputsRoot = copiedLiveRecording->_encodingParametersRoot["outputsRoot"];
					for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
					{
						Json::Value outputRoot = outputsRoot[outputIndex];

						string outputType = outputRoot.get("outputType", "").asString();
						string manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "").
							asString();
						string manifestFileName = outputRoot.get("manifestFileName", "").asString();

						if (!liveRecorderWorking)
							break;

						if (outputType == "HLS" || outputType == "DASH")
						{
							try
							{
								// First health check (HLS/DASH) looking the manifests path name timestamp

								string manifestFilePathName =
									manifestDirectoryPath + "/" + manifestFileName;
								{
									if(!FileIO::fileExisting(manifestFilePathName))
									{
										liveRecorderWorking = false;

										_logger->error(__FILEREF__ + "liveRecorderMonitor. Manifest file does not exist"
											+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestFilePathName: " + manifestFilePathName
										);

										localErrorMessage = " restarted because of 'manifest file is missing'";

										break;
									}
									else
									{
										time_t utcManifestFileLastModificationTime;

										FileIO::getFileTime (manifestFilePathName.c_str(),
											&utcManifestFileLastModificationTime);

										unsigned long long	ullNow = 0;
										unsigned long		ulAdditionalMilliSecs;
										long				lTimeZoneDifferenceInHours;

										DateTime:: nowUTCInMilliSecs (&ullNow, &ulAdditionalMilliSecs,
											&lTimeZoneDifferenceInHours);

										long maxLastManifestFileUpdateInSeconds = 45;

										unsigned long long lastManifestFileUpdateInSeconds = ullNow - utcManifestFileLastModificationTime;
										if (lastManifestFileUpdateInSeconds > maxLastManifestFileUpdateInSeconds)
										{
											liveRecorderWorking = false;

											_logger->error(__FILEREF__ + "liveRecorderMonitor. Manifest file was not updated "
												+ "in the last " + to_string(maxLastManifestFileUpdateInSeconds) + " seconds"
												+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
												+ ", manifestFilePathName: " + manifestFilePathName
												+ ", lastManifestFileUpdateInSeconds: " + to_string(lastManifestFileUpdateInSeconds) + " seconds ago"
											);

											localErrorMessage = " restarted because of 'manifest file was not updated'";

											break;
										}
									}
								}
							}
							catch(runtime_error e)
							{
								string errorMessage = string ("liveRecorderMonitor (HLS) on manifest path name failed")
									+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
							catch(exception e)
							{
								string errorMessage = string ("liveRecorderMonitor (HLS) on manifest path name failed")
									+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
						}
						else	// rtmp (Proxy) 
						{
							rtmpOutputFound = true;
						}
					}
				}

				if (!sourceLiveRecording->_running ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_running: " + to_string(sourceLiveRecording->_running)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveRecorderWorking && rtmpOutputFound)
				{
					try
					{
						_logger->info(__FILEREF__ + "liveRecordingMonitor. nonMonotonousDTSInOutputLog check"
							+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						);

						// First health check (rtmp), looks the log and check there is no message like
						//	[flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to 95383372. This may result in incorrect timestamps in the output file.
						//	This message causes proxy not working
						if (sourceLiveRecording->_ffmpeg->nonMonotonousDTSInOutputLog())
						{
							liveRecorderWorking = false;

							_logger->error(__FILEREF__ + "liveRecorderMonitor (rtmp). Live Recorder is logging 'Non-monotonous DTS in output stream/incorrect timestamps'. LiveRecorder (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
								+ ", channelLabel: " + copiedLiveRecording->_channelLabel
								+ ", copiedLiveRecording->_childPid: " + to_string(copiedLiveRecording->_childPid)
							);

							localErrorMessage = " restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps'";
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) Non-monotonous DTS failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) Non-monotonous DTS failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (!sourceLiveRecording->_running ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_running: " + to_string(sourceLiveRecording->_running)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				// Thirth health 
				//		HLS/DASH:	kill if segments were not generated
				//					frame increasing check
				//					it is also implemented the retention of segments too old (10 minutes)
				//						This is already implemented by the HLS parameters (into the ffmpeg command)
				//						We do it for the DASH option and in case ffmpeg does not work
				//		rtmp(Proxy):		frame increasing check
				//			This check has to be done just once (not for each outputRoot) in case we have at least one rtmp output
				if (liveRecorderWorking)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. segment check"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					);

					Json::Value outputsRoot = copiedLiveRecording->_encodingParametersRoot["outputsRoot"];
					for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
					{
						Json::Value outputRoot = outputsRoot[outputIndex];

						string outputType = outputRoot.get("outputType", "").asString();
						string manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "").
							asString();
						string manifestFileName = outputRoot.get("manifestFileName", "").asString();
						int outputPlaylistEntriesNumber = JSONUtils::asInt(outputRoot,
							"playlistEntriesNumber", 10);
						int outputSegmentDurationInSeconds = JSONUtils::asInt(outputRoot,
							"segmentDurationInSeconds", 10);

						if (!liveRecorderWorking)
							break;

						if (outputType == "HLS" || outputType == "DASH")
						{
							try
							{
								string manifestFilePathName =
									manifestDirectoryPath + "/" + manifestFileName;
								{
									vector<string>	chunksTooOldToBeRemoved;
									bool chunksWereNotGenerated = false;

									string manifestDirectoryPathName;
									{
										size_t manifestFilePathIndex = manifestFilePathName.find_last_of("/");
										if (manifestFilePathIndex == string::npos)
										{
											string errorMessage = __FILEREF__ + "liveRecordingMonitor. No manifestDirectoryPath find in the m3u8/mpd file path name"
												+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
												+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
												+ ", manifestFilePathName: " + manifestFilePathName;
											_logger->error(errorMessage);

											throw runtime_error(errorMessage);
										}
										manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
									}

									chrono::system_clock::time_point lastChunkTimestamp = copiedLiveRecording->_recordingStart;
									bool firstChunkRead = false;

									try
									{
										if (FileIO::directoryExisting(manifestDirectoryPathName))
										{
											FileIO::DirectoryEntryType_t detDirectoryEntryType;
											shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (
												manifestDirectoryPathName + "/");

											// chunks will be removed 10 minutes after the "capacity" of the playlist
											// 2022-05-26: it was 10 minutes fixed. This is an error
											// in case of LiveRecorderVirtualVOD because, in this scenario,
											// the segments have to be present according
											// LiveRecorderVirtualVODMaxDuration (otherwise we will have an error
											// during the building of the VirtualVOD (segments not found).
											// For this reason the retention has to consider segment duration
											// and playlistEntriesNumber
											// long liveProxyChunkRetentionInSeconds = 10 * 60;	// 10 minutes
											long liveProxyChunkRetentionInSeconds =
												(outputSegmentDurationInSeconds * outputPlaylistEntriesNumber)
												+ (10 * 60);	// 10 minutes
											_logger->info(__FILEREF__
												+ "liveRecordingMonitor. segment check"
												+ ", ingestionJobKey: " + to_string(
													copiedLiveRecording->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(
													copiedLiveRecording->_encodingJobKey)
												+ ", channelLabel: "
													+ copiedLiveRecording->_channelLabel
												+ ", outputSegmentDurationInSeconds: "
													+ to_string(outputSegmentDurationInSeconds)
												+ ", outputPlaylistEntriesNumber: "
													+ to_string(outputPlaylistEntriesNumber)
												+ ", liveProxyChunkRetentionInSeconds: "
													+ to_string(liveProxyChunkRetentionInSeconds)
											);

											bool scanDirectoryFinished = false;
											while (!scanDirectoryFinished)
											{
												string directoryEntry;
												try
												{
													string directoryEntry = FileIO::readDirectory (directory,
													&detDirectoryEntryType);

													if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
														continue;

													string dashPrefixInitFiles ("init-stream");
													if (outputType == "DASH" &&
														directoryEntry.size() >= dashPrefixInitFiles.size()
															&& 0 == directoryEntry.compare(0, dashPrefixInitFiles.size(), dashPrefixInitFiles)
													)
														continue;

													{
														string segmentPathNameToBeRemoved =
															manifestDirectoryPathName + "/" + directoryEntry;

														chrono::system_clock::time_point fileLastModification =
															FileIO::getFileTime (segmentPathNameToBeRemoved);
														chrono::system_clock::time_point now = chrono::system_clock::now();

														if (chrono::duration_cast<chrono::seconds>(now - fileLastModification).count()
															> liveProxyChunkRetentionInSeconds)
														{
															chunksTooOldToBeRemoved.push_back(segmentPathNameToBeRemoved);
														}

														if (!firstChunkRead
															|| fileLastModification > lastChunkTimestamp)
															lastChunkTimestamp = fileLastModification;

														firstChunkRead = true;
													}
												}
												catch(DirectoryListFinished e)
												{
													scanDirectoryFinished = true;
												}
												catch(runtime_error e)
												{
													string errorMessage = __FILEREF__ + "liveRecordingMonitor. listing directory failed"
														+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
														+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
														+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
														+ ", e.what(): " + e.what()
													;
													_logger->error(errorMessage);

													// throw e;
												}
												catch(exception e)
												{
													string errorMessage = __FILEREF__ + "liveRecordingMonitor. listing directory failed"
														+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
														+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
														+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
														+ ", e.what(): " + e.what()
													;
													_logger->error(errorMessage);

													// throw e;
												}
											}

											FileIO::closeDirectory (directory);
										}
									}
									catch(runtime_error e)
									{
										_logger->error(__FILEREF__ + "liveRecordingMonitor. scan LiveRecorder files failed"
											+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
											+ ", e.what(): " + e.what()
										);
									}
									catch(...)
									{
										_logger->error(__FILEREF__ + "liveRecordingMonitor. scan LiveRecorder files failed"
											+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
										);
									}
			
									if (!firstChunkRead
										|| lastChunkTimestamp < chrono::system_clock::now() - chrono::minutes(1))
									{
										// if we are here, it means the ffmpeg command is not generating the ts files

										_logger->error(__FILEREF__ + "liveRecorderMonitor. Chunks were not generated"
											+ ", liveRecorder->_ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", liveRecorder->_encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
											+ ", firstChunkRead: " + to_string(firstChunkRead)
										);

										chunksWereNotGenerated = true;

										liveRecorderWorking = false;
										localErrorMessage = " restarted because of 'no segments were generated'";

										_logger->error(__FILEREF__ + "liveRecordingMonitor. ProcessUtility::kill/quitProcess. liveRecorderMonitor. Live Recorder is not working (no segments were generated). LiveRecorder (ffmpeg) is killed in order to be started again"
											+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
											+ ", manifestFilePathName: " + manifestFilePathName
											+ ", channelLabel: " + copiedLiveRecording->_channelLabel
											+ ", liveRecorder->_childPid: " + to_string(copiedLiveRecording->_childPid)
										);


										// we killed the process, we do not care to remove the too old segments
										// since we will remove the entore directory
										break;
									}

									{
										bool exceptionInCaseOfError = false;

										for (string segmentPathNameToBeRemoved: chunksTooOldToBeRemoved)
										{
											try
											{
												_logger->info(__FILEREF__ + "liveRecorderMonitor. Remove chunk because too old"
													+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
													+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
													+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
												FileIO::remove(segmentPathNameToBeRemoved, exceptionInCaseOfError);
											}
											catch(runtime_error e)
											{
												_logger->error(__FILEREF__ + "liveRecordingMonitor. remove failed"
													+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
													+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
													+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved
													+ ", e.what(): " + e.what()
												);
											}
										}
									}
								}
							}
							catch(runtime_error e)
							{
								string errorMessage = string ("liveRecorderMonitor (HLS) on segments (and retention) failed")
									+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
							catch(exception e)
							{
								string errorMessage = string ("liveRecorderMonitor (HLS) on segments (and retention) failed")
									+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
									+ ", e.what(): " + e.what()
								;

								_logger->error(__FILEREF__ + errorMessage);
							}
						}
					}
				}

				if (!sourceLiveRecording->_running ||
					copiedLiveRecording->_recordingStart != sourceLiveRecording->_recordingStart)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. LiveRecorder changed"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", sourceLiveRecording->_running: " + to_string(sourceLiveRecording->_running)
						+ ", copiedLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(copiedLiveRecording->_recordingStart.time_since_epoch().count())
						+ ", sourceLiveRecording->_recordingStart.time_since_epoch().count(): " + to_string(sourceLiveRecording->_recordingStart.time_since_epoch().count())
					);

					continue;
				}

				if (liveRecorderWorking && copiedLiveRecording->_monitoringFrameIncreasingEnabled) // && rtmpOutputFound)
				{
					_logger->info(__FILEREF__ + "liveRecordingMonitor. isFrameIncreasing check"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					);

					try
					{
						// Second health check, rtmp(Proxy), looks if the frame is increasing
						int maxMilliSecondsToWait = 3000;
						if (!sourceLiveRecording->_ffmpeg->isFrameIncreasing(
							maxMilliSecondsToWait))
						{
							_logger->error(__FILEREF__ + "liveRecordingMonitor. ProcessUtility::kill/quitProcess. liveRecorderMonitor (rtmp). Live Recorder frame is not increasing'. LiveRecorder (ffmpeg) is killed in order to be started again"
								+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
								+ ", channelLabel: " + copiedLiveRecording->_channelLabel
								+ ", _childPid: " + to_string(copiedLiveRecording->_childPid)
							);

							liveRecorderWorking = false;

							localErrorMessage = " restarted because of 'frame is not increasing'";
						}
					}
					catch(FFMpegEncodingStatusNotAvailable e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) frame increasing check failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->warn(__FILEREF__ + errorMessage);
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) frame increasing check failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveRecorderMonitor (rtmp) frame increasing check failed")
							+ ", _ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				if (!liveRecorderWorking)
				{
					_logger->error(__FILEREF__ + "liveRecordingMonitor. ProcessUtility::kill/quitProcess. liveRecordingMonitor. Live Recording is not working (segment list file is missing or was not updated). LiveRecording (ffmpeg) is killed in order to be started again"
						+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
						+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
						+ ", channelLabel: " + copiedLiveRecording->_channelLabel
						+ ", copiedLiveRecording->_childPid: " + to_string(copiedLiveRecording->_childPid)
					);

					try
					{
						// 2021-12-14: switched from quit to kill because it seems
						//		ffmpeg didn't terminate (in case of quit) when he was
						//		failing. May be because it could not finish his sample/frame
						//		to process. The result is that the channels were not restarted.
						//		This is an ipothesys, not 100% sure
						// 2022-11-02: SIGQUIT is managed inside FFMpeg.cpp by liverecording
						// ProcessUtility::killProcess(sourceLiveRecording->_childPid);
						// sourceLiveRecording->_killedBecauseOfNotWorking = true;
						ProcessUtility::quitProcess(sourceLiveRecording->_childPid);
						{
							char strDateTime [64];
							{
								time_t utcTime = chrono::system_clock::to_time_t(
									chrono::system_clock::now());
								tm tmDateTime;
								localtime_r (&utcTime, &tmDateTime);
								sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
									tmDateTime. tm_year + 1900,
									tmDateTime. tm_mon + 1,
									tmDateTime. tm_mday,
									tmDateTime. tm_hour,
									tmDateTime. tm_min,
									tmDateTime. tm_sec);
							}
							sourceLiveRecording->_errorMessage = string(strDateTime) + " "
								+ sourceLiveRecording->_channelLabel +
								localErrorMessage;
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = string("liveRecordingMonitor. ProcessUtility::kill/quit Process failed")
							+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
							+ ", channelLabel: " + copiedLiveRecording->_channelLabel
							+ ", copiedLiveRecording->_childPid: " + to_string(copiedLiveRecording->_childPid)
							+ ", e.what(): " + e.what()
								;
						_logger->error(__FILEREF__ + errorMessage);
					}
				}

				_logger->info(__FILEREF__ + "liveRecordingMonitor "
					+ to_string(liveRecordingIndex) + "/" + to_string(liveRecordingRunningCounter)
					+ ", ingestionJobKey: " + to_string(copiedLiveRecording->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(copiedLiveRecording->_encodingJobKey)
					+ ", channelLabel: " + copiedLiveRecording->_channelLabel
					+ ", @MMS statistics@ - elapsed time: @" + to_string(
						chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - now).count()
					) + "@"
				);
			}
			_logger->info(__FILEREF__ + "liveRecordingMonitor"
				+ ", liveRecordingRunningCounter: " + to_string(liveRecordingRunningCounter)
				+ ", @MMS statistics@ - elapsed (millisecs): " + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now() - monitorStart).count())
			);
		}
		catch(runtime_error e)
		{
			string errorMessage = string ("liveRecordingMonitor failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("liveRecordingMonitor failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}

		this_thread::sleep_for(chrono::seconds(_monitorCheckInSeconds));
	}
}

void FFMPEGEncoder::stopMonitorThread()
{

	_monitorThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(_monitorCheckInSeconds));
}

void FFMPEGEncoder::cpuUsageThread()
{

	int64_t counter = 0;

	while(!_cpuUsageThreadShutdown)
	{
		this_thread::sleep_for(chrono::milliseconds(200));

		try
		{
			lock_guard<mutex> locker(*_cpuUsageMutex);

			_cpuUsage->pop_back();
			_cpuUsage->push_front(_getCpuUsage.getCpuUsage());
			// *_cpuUsage = _getCpuUsage.getCpuUsage();

			if (++counter % 100 == 0)
			{
				string lastCPUUsage;
				for(int cpuUsage: *_cpuUsage)
					lastCPUUsage += (to_string(cpuUsage) + " ");

				_logger->info(__FILEREF__ + "cpuUsageThread"
					+ ", lastCPUUsage: " + lastCPUUsage
				);
			}
		}
		catch(runtime_error e)
		{
			string errorMessage = string ("cpuUsage thread failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("cpuUsage thread failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
	}
}

void FFMPEGEncoder::stopCPUUsageThread()
{

	_cpuUsageThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(1));
}

void FFMPEGEncoder::addEncodingCompleted(
        int64_t encodingJobKey, bool completedWithError,
		string errorMessage,
		bool killedByUser, bool urlForbidden, bool urlNotFound)
{
	lock_guard<mutex> locker(*_encodingCompletedMutex);

	shared_ptr<EncodingCompleted> encodingCompleted = make_shared<EncodingCompleted>();

	encodingCompleted->_encodingJobKey		= encodingJobKey;
	encodingCompleted->_completedWithError	= completedWithError;
	encodingCompleted->_errorMessage		= errorMessage;
	encodingCompleted->_killedByUser		= killedByUser;
	encodingCompleted->_urlForbidden		= urlForbidden;
	encodingCompleted->_urlNotFound			= urlNotFound;
	encodingCompleted->_timestamp			= chrono::system_clock::now();

	_encodingCompletedMap->insert(make_pair(encodingCompleted->_encodingJobKey, encodingCompleted));

	_logger->info(__FILEREF__ + "addEncodingCompleted"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", encodingCompletedMap size: " + to_string(_encodingCompletedMap->size())
			);
}

void FFMPEGEncoder::removeEncodingCompletedIfPresent(int64_t encodingJobKey)
{

	lock_guard<mutex> locker(*_encodingCompletedMutex);

	map<int64_t, shared_ptr<EncodingCompleted>>::iterator it =
		_encodingCompletedMap->find(encodingJobKey);
	if (it != _encodingCompletedMap->end())
	{
		_encodingCompletedMap->erase(it);

		_logger->info(__FILEREF__ + "removeEncodingCompletedIfPresent"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", encodingCompletedMap size: " + to_string(_encodingCompletedMap->size())
			);
	}
}

void FFMPEGEncoder::encodingCompletedRetention()
{

	lock_guard<mutex> locker(*_encodingCompletedMutex);

	chrono::system_clock::time_point start = chrono::system_clock::now();

	for(map<int64_t, shared_ptr<EncodingCompleted>>::iterator it = _encodingCompletedMap->begin();
			it != _encodingCompletedMap->end(); )
	{
		if(start - (it->second->_timestamp) >= chrono::seconds(_encodingCompletedRetentionInSeconds))
			it = _encodingCompletedMap->erase(it);
		else
			it++;
	}

	chrono::system_clock::time_point end = chrono::system_clock::now();

	_logger->info(__FILEREF__ + "encodingCompletedRetention"
		+ ", encodingCompletedMap size: " + to_string(_encodingCompletedMap->size())
		+ ", @MMS statistics@ - duration encodingCompleted retention processing (secs): @"
			+ to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
	);
}

void FFMPEGEncoder::createOrUpdateTVDvbLastConfigurationFile(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string multicastIP,
	string multicastPort,
	string tvType,
	int64_t tvServiceId,
	int64_t tvFrequency,
	int64_t tvSymbolRate,
	int64_t tvBandwidthInMhz,
	string tvModulation,
	int tvVideoPid,
	int tvAudioItalianPid,
	bool toBeAdded
)
{
	try
	{
		_logger->info(__FILEREF__ + "Received createOrUpdateTVDvbLastConfigurationFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", multicastIP: " + multicastIP
			+ ", multicastPort: " + multicastPort
			+ ", tvType: " + tvType
			+ ", tvServiceId: " + to_string(tvServiceId)
			+ ", tvFrequency: " + to_string(tvFrequency)
			+ ", tvSymbolRate: " + to_string(tvSymbolRate)
			+ ", tvBandwidthInMhz: " + to_string(tvBandwidthInMhz)
			+ ", tvModulation: " + tvModulation
			+ ", tvVideoPid: " + to_string(tvVideoPid)
			+ ", tvAudioItalianPid: " + to_string(tvAudioItalianPid)
			+ ", toBeAdded: " + to_string(toBeAdded)
		);

		string localModulation;

		// dvblast modulation: qpsk|psk_8|apsk_16|apsk_32
		if (tvModulation != "")
		{
			if (tvModulation == "PSK/8")
				localModulation = "psk_8";
			else if (tvModulation == "QAM/64")
				localModulation = "QAM_64";
			else if (tvModulation == "QPSK")
				localModulation = "qpsk";
			else
			{
				string errorMessage = __FILEREF__ + "unknown modulation"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", tvModulation: " + tvModulation
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (!FileIO::directoryExisting(_tvChannelConfigurationDirectory))
		{
			_logger->info(__FILEREF__ + "Create directory"
				+ ", _ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(encodingJobKey)
				+ ", _tvChannelConfigurationDirectory: " + _tvChannelConfigurationDirectory
			);

			bool noErrorIfExists = true;
			bool recursive = true;
			FileIO::createDirectory(
				_tvChannelConfigurationDirectory,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IWUSR | S_IXGRP |
				S_IROTH | S_IWUSR | S_IXOTH,
				noErrorIfExists, recursive);
		}

		string dvblastConfigurationPathName =
			_tvChannelConfigurationDirectory
			+ "/" + to_string(tvFrequency)
		;
		if (tvSymbolRate < 0)
			dvblastConfigurationPathName += "-";
		else
			dvblastConfigurationPathName += (string("-") + to_string(tvSymbolRate));
		if (tvBandwidthInMhz < 0)
			dvblastConfigurationPathName += "-";
		else
			dvblastConfigurationPathName += (string("-") + to_string(tvBandwidthInMhz));
		dvblastConfigurationPathName += (string("-") + localModulation);

		ifstream ifConfigurationFile;
		bool changedFileFound = false;
		if (FileIO::fileExisting(dvblastConfigurationPathName + ".txt"))
			ifConfigurationFile.open(dvblastConfigurationPathName + ".txt", ios::in);
		else if (FileIO::fileExisting(dvblastConfigurationPathName + ".changed"))
		{
			changedFileFound = true;
			ifConfigurationFile.open(dvblastConfigurationPathName + ".changed", ios::in);
		}

		vector<string> vConfiguration;
		if (ifConfigurationFile.is_open())
        {
			string configuration;
            while(getline(ifConfigurationFile, configuration))
			{
				string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

				if (trimmedConfiguration.size() > 10)
					vConfiguration.push_back(trimmedConfiguration);
			}
            ifConfigurationFile.close();
			if (!changedFileFound)	// .txt found
			{
				_logger->info(__FILEREF__ + "Remove dvblast configuration file to create the new one"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", dvblastConfigurationPathName: " + dvblastConfigurationPathName + ".txt"
				);

				FileIO::remove(dvblastConfigurationPathName + ".txt");
			}
		}

		string newConfiguration =
			multicastIP + ":" + multicastPort 
			+ " 1 "
			+ to_string(tvServiceId)
			+ " "
			+ to_string(tvVideoPid) + "," + to_string(tvAudioItalianPid)
		;

		_logger->info(__FILEREF__ + "Creation dvblast configuration file"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", dvblastConfigurationPathName: " + dvblastConfigurationPathName + ".changed"
		);

		ofstream ofConfigurationFile(dvblastConfigurationPathName + ".changed", ofstream::trunc);
		if (!ofConfigurationFile)
		{
			string errorMessage = __FILEREF__ + "Creation dvblast configuration file failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", dvblastConfigurationPathName: " + dvblastConfigurationPathName + ".changed"
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		bool configurationAlreadyPresent = false;
		bool wroteFirstLine = false;
		for(string configuration: vConfiguration)
		{
			if (toBeAdded)
			{
				if (newConfiguration == configuration)
					configurationAlreadyPresent = true;

				if (wroteFirstLine)
					ofConfigurationFile << endl;
				ofConfigurationFile << configuration;
				wroteFirstLine = true;
			}
			else
			{
				if (newConfiguration != configuration)
				{
					if (wroteFirstLine)
						ofConfigurationFile << endl;
					ofConfigurationFile << configuration;
					wroteFirstLine = true;
				}
			}
		}

		if (toBeAdded)
		{
			// added only if not already present
			if (!configurationAlreadyPresent)
			{
				if (wroteFirstLine)
					ofConfigurationFile << endl;
				ofConfigurationFile << newConfiguration;
				wroteFirstLine = true;
			}
		}

		ofConfigurationFile << endl;
	}
	catch (...)
	{
		// make sure do not raise an exception to the calling method to avoid
		// to interrupt "closure" encoding procedure
		string errorMessage = __FILEREF__ + "createOrUpdateTVDvbLastConfigurationFile failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		;
		_logger->error(errorMessage);
	}
}

pair<string, string> FFMPEGEncoder::getTVMulticastFromDvblastConfigurationFile(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string tvType,
	int64_t tvServiceId,
	int64_t tvFrequency,
	int64_t tvSymbolRate,
	int64_t tvBandwidthInMhz,
	string tvModulation
)
{
	string multicastIP;
	string multicastPort;

	try
	{
		_logger->info(__FILEREF__ + "Received getTVMulticastFromDvblastConfigurationFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", tvType: " + tvType
			+ ", tvServiceId: " + to_string(tvServiceId)
			+ ", tvFrequency: " + to_string(tvFrequency)
			+ ", tvSymbolRate: " + to_string(tvSymbolRate)
			+ ", tvBandwidthInMhz: " + to_string(tvBandwidthInMhz)
			+ ", tvModulation: " + tvModulation
		);

		string localModulation;

		// dvblast modulation: qpsk|psk_8|apsk_16|apsk_32
		if (tvModulation != "")
		{
			if (tvModulation == "PSK/8")
				localModulation = "psk_8";
			else if (tvModulation == "QAM/64")
				localModulation = "QAM_64";
			else if (tvModulation == "QPSK")
				localModulation = "qpsk";
			else
			{
				string errorMessage = __FILEREF__ + "unknown modulation"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", tvModulation: " + tvModulation
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string dvblastConfigurationPathName =
			_tvChannelConfigurationDirectory
			+ "/" + to_string(tvFrequency)
		;
		if (tvSymbolRate < 0)
			dvblastConfigurationPathName += "-";
		else
			dvblastConfigurationPathName += (string("-") + to_string(tvSymbolRate));
		if (tvBandwidthInMhz < 0)
			dvblastConfigurationPathName += "-";
		else
			dvblastConfigurationPathName += (string("-") + to_string(tvBandwidthInMhz));
		dvblastConfigurationPathName += (string("-") + localModulation);


		ifstream configurationFile;
		if (FileIO::fileExisting(dvblastConfigurationPathName + ".txt"))
			configurationFile.open(dvblastConfigurationPathName + ".txt", ios::in);
		else if (FileIO::fileExisting(dvblastConfigurationPathName + ".changed"))
			configurationFile.open(dvblastConfigurationPathName + ".changed", ios::in);

		if (configurationFile.is_open())
		{
			string configuration;
            while(getline(configurationFile, configuration))
			{
				string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

				// configuration is like: 239.255.1.1:8008 1 3401 501,601
				istringstream iss(trimmedConfiguration);
				vector<string> configurationPieces;
				copy(
					istream_iterator<std::string>(iss),
					istream_iterator<std::string>(),
					back_inserter(configurationPieces)
				);
				if(configurationPieces.size() < 3)
					continue;

				if (configurationPieces[2] == to_string(tvServiceId))
				{
					size_t ipSeparator = (configurationPieces[0]).find(":");
					if (ipSeparator != string::npos)
					{
						multicastIP = (configurationPieces[0]).substr(0, ipSeparator);
						multicastPort = (configurationPieces[0]).substr(ipSeparator + 1);

						break;
					}
				}
			}
            configurationFile.close();
        }

		_logger->info(__FILEREF__ + "Received getTVMulticastFromDvblastConfigurationFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", tvType: " + tvType
			+ ", tvServiceId: " + to_string(tvServiceId)
			+ ", tvFrequency: " + to_string(tvFrequency)
			+ ", tvSymbolRate: " + to_string(tvSymbolRate)
			+ ", tvBandwidthInMhz: " + to_string(tvBandwidthInMhz)
			+ ", tvModulation: " + tvModulation
			+ ", multicastIP: " + multicastIP
			+ ", multicastPort: " + multicastPort
		);
	}
	catch (...)
	{
		// make sure do not raise an exception to the calling method to avoid
		// to interrupt "closure" encoding procedure
		string errorMessage = __FILEREF__ + "getTVMulticastFromDvblastConfigurationFile failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		;
		_logger->error(errorMessage);
	}

	return make_pair(multicastIP, multicastPort);
}

int FFMPEGEncoder::getMaxEncodingsCapability(void)
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to be done
	{
		lock_guard<mutex> locker(*_cpuUsageMutex);

		int maxCapability = VECTOR_MAX_CAPACITY;	// it could be done

		for(int cpuUsage: *_cpuUsage)
		{
			if (cpuUsage > _cpuUsageThresholdForEncoding)
			{
				maxCapability = 0;						// no to be done

				break;
			}
		}

		string lastCPUUsage;
		for(int cpuUsage: *_cpuUsage)
			lastCPUUsage += (to_string(cpuUsage) + " ");
		_logger->info(__FILEREF__ + "getMaxXXXXCapability"
			+ ", lastCPUUsage: " + lastCPUUsage
			+ ", maxCapability: " + to_string(maxCapability)
		);

		return maxCapability;
	}
}

int FFMPEGEncoder::getMaxLiveProxiesCapability(void)
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to be done
	{
		lock_guard<mutex> locker(*_cpuUsageMutex);

		int maxCapability = VECTOR_MAX_CAPACITY;	// it could be done

		for(int cpuUsage: *_cpuUsage)
		{
			if (cpuUsage > _cpuUsageThresholdForProxy)
			{
				maxCapability = 0;						// no to be done

				break;
			}
		}

		string lastCPUUsage;
		for(int cpuUsage: *_cpuUsage)
			lastCPUUsage += (to_string(cpuUsage) + " ");
		_logger->info(__FILEREF__ + "getMaxXXXXCapability"
			+ ", lastCPUUsage: " + lastCPUUsage
			+ ", maxCapability: " + to_string(maxCapability)
		);

		return maxCapability;
	}

	/*
	int maxLiveProxiesCapability = 1;

	try
	{
		if (FileIO::fileExisting(_encoderCapabilityConfigurationPathName))
		{
			Json::Value encoderCapabilityConfiguration = APICommon::loadConfigurationFile(
				_encoderCapabilityConfigurationPathName.c_str());

			maxLiveProxiesCapability = JSONUtils::asInt(encoderCapabilityConfiguration["ffmpeg"],
				"maxLiveProxiesCapability", 1);
			_logger->info(__FILEREF__ + "Configuration item"
				+ ", ffmpeg->maxLiveProxiesCapability: " + to_string(maxLiveProxiesCapability)
			);

			if (maxLiveProxiesCapability > VECTOR_MAX_CAPACITY)
			{
				_logger->error(__FILEREF__ + "getMaxXXXXCapability. maxLiveProxiesCapability cannot be bigger than VECTOR_MAX_CAPACITY"
					+ ", _encoderCapabilityConfigurationPathName: " + _encoderCapabilityConfigurationPathName
					+ ", maxLiveProxiesCapability: " + to_string(maxLiveProxiesCapability)
					+ ", VECTOR_MAX_CAPACITY: " + to_string(VECTOR_MAX_CAPACITY)
				);

				maxLiveProxiesCapability = VECTOR_MAX_CAPACITY;
			}

			maxLiveProxiesCapability = calculateCapabilitiesBasedOnOtherRunningProcesses(
				-1,
				maxLiveProxiesCapability,
				-1
			);
		}
		else
		{
			_logger->error(__FILEREF__ + "getMaxXXXXCapability. Encoder Capability Configuration Path Name is not present"
				+ ", _encoderCapabilityConfigurationPathName: " + _encoderCapabilityConfigurationPathName
			);
		}
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "getMaxXXXXCapability failed"
			+ ", _encoderCapabilityConfigurationPathName: " + _encoderCapabilityConfigurationPathName
		);
	}

	_logger->info(__FILEREF__ + "getMaxXXXXCapability"
		+ ", maxLiveProxiesCapability: " + to_string(maxLiveProxiesCapability)
	);

	return maxLiveProxiesCapability;
	*/
}

int FFMPEGEncoder::getMaxLiveRecordingsCapability(void)
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to be done
	{
		lock_guard<mutex> locker(*_cpuUsageMutex);

		int maxCapability = VECTOR_MAX_CAPACITY;	// it could be done

		for(int cpuUsage: *_cpuUsage)
		{
			if (cpuUsage > _cpuUsageThresholdForRecording)
			{
				maxCapability = 0;						// no to be done

				break;
			}
		}

		string lastCPUUsage;
		for(int cpuUsage: *_cpuUsage)
			lastCPUUsage += (to_string(cpuUsage) + " ");
		_logger->info(__FILEREF__ + "getMaxXXXXCapability"
			+ ", lastCPUUsage: " + lastCPUUsage
			+ ", maxCapability: " + to_string(maxCapability)
		);

		return maxCapability;
	}

	/*

	try
	{
		if (FileIO::fileExisting(_encoderCapabilityConfigurationPathName))
		{
			Json::Value encoderCapabilityConfiguration = APICommon::loadConfigurationFile(
				_encoderCapabilityConfigurationPathName.c_str());

			maxLiveRecordingsCapability = JSONUtils::asInt(encoderCapabilityConfiguration["ffmpeg"],
				"maxLiveRecordingsCapability", 1);
			_logger->info(__FILEREF__ + "Configuration item"
				+ ", ffmpeg->maxLiveRecordingsCapability: " + to_string(maxLiveRecordingsCapability)
			);

			if (maxLiveRecordingsCapability > VECTOR_MAX_CAPACITY)
			{
				_logger->error(__FILEREF__ + "getMaxXXXXCapability. maxLiveRecordingsCapability cannot be bigger than VECTOR_MAX_CAPACITY"
					+ ", _encoderCapabilityConfigurationPathName: " + _encoderCapabilityConfigurationPathName
					+ ", maxLiveRecordingsCapability: " + to_string(maxLiveRecordingsCapability)
					+ ", VECTOR_MAX_CAPACITY: " + to_string(VECTOR_MAX_CAPACITY)
				);

				maxLiveRecordingsCapability = VECTOR_MAX_CAPACITY;
			}

			maxLiveRecordingsCapability = calculateCapabilitiesBasedOnOtherRunningProcesses(
				-1,
				-1,
				maxLiveRecordingsCapability
			);
		}
		else
		{
			_logger->error(__FILEREF__ + "getMaxXXXXCapability. Encoder Capability Configuration Path Name is not present"
				+ ", _encoderCapabilityConfigurationPathName: " + _encoderCapabilityConfigurationPathName
			);
		}
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "getMaxLiveRecordingsCapability failed"
			+ ", _encoderCapabilityConfigurationPathName: " + _encoderCapabilityConfigurationPathName
		);
	}

	_logger->info(__FILEREF__ + "getMaxXXXXCapability"
		+ ", maxLiveRecordingsCapability: " + to_string(maxLiveRecordingsCapability)
	);

	return maxLiveRecordingsCapability;
	*/
}

