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
#include <fstream>
#include <sstream>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/System.h"
#include "catralibraries/Convert.h"
#include "catralibraries/FileIO.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/GetCpuUsage.h"
#include "FFMPEGEncoder.h"
#include "MMSStorage.h"

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>

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
		int cpuUsage = 0;

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

		mutex satelliteChannelsPortsMutex;
		long satelliteChannelPort_CurrentOffset = 0;

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

				&satelliteChannelsPortsMutex,
				&satelliteChannelPort_CurrentOffset,

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
		int* cpuUsage,

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

		mutex* satelliteChannelsPortsMutex,
		long* satelliteChannelPort_CurrentOffset,

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
        + ", ffmpeg->liveRecorderVirtualVODRetention: " + _mmsAPIProtocol
    );

	_satelliteChannelConfigurationDirectory = _configuration["ffmpeg"].
		get("satelliteChannelConfigurationDirectory", "").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->satelliteChannelConfigurationDirectory: " + _satelliteChannelConfigurationDirectory
	);

    _encodingCompletedRetentionInSeconds = JSONUtils::asInt(_configuration["ffmpeg"], "encodingCompletedRetentionInSeconds", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encodingCompletedRetentionInSeconds: " + to_string(_encodingCompletedRetentionInSeconds)
    );

    _mmsAPIProtocol = _configuration["api"].get("protocol", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->protocol: " + _mmsAPIProtocol
    );
    _mmsAPIHostname = _configuration["api"].get("hostname", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->hostname: " + _mmsAPIHostname
    );
    _mmsAPIPort = JSONUtils::asInt(_configuration["api"], "port", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->port: " + to_string(_mmsAPIPort)
    );
    _mmsAPIIngestionURI = _configuration["api"].get("ingestionURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->ingestionURI: " + _mmsAPIIngestionURI
    );
    _mmsAPITimeoutInSeconds = JSONUtils::asInt(_configuration["api"], "timeoutInSeconds", 120);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->timeoutInSeconds: " + to_string(_mmsAPITimeoutInSeconds)
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

	_satelliteChannelPort_Start = 8000;
	_satelliteChannelPort_MaxNumberOfOffsets = 100;

	_cpuUsageThreadShutdown = false;
	_monitorThreadShutdown = false;
	_liveRecorderChunksIngestionThreadShutdown = false;
	_liveRecorderVirtualVODIngestionThreadShutdown = false;

	_encodingCompletedMutex = encodingCompletedMutex;
	_encodingCompletedMap = encodingCompletedMap;
	_lastEncodingCompletedCheck = lastEncodingCompletedCheck;

	_satelliteChannelsPortsMutex = satelliteChannelsPortsMutex;
	_satelliteChannelPort_CurrentOffset = satelliteChannelPort_CurrentOffset;

	*_lastEncodingCompletedCheck = chrono::system_clock::now();
}

FFMPEGEncoder::~FFMPEGEncoder() {
}

/*
void FFMPEGEncoder::getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool>& userKeyWorkspaceAndFlags,
        unsigned long contentLength
)
{
    _logger->error(__FILEREF__ + "FFMPEGEncoder application is able to manage ONLY NON-Binary requests");
    
    string errorMessage = string("Internal server error");
    _logger->error(__FILEREF__ + errorMessage);

    sendError(500, errorMessage);

    throw runtime_error(errorMessage);
}
*/

// 2020-06-11: FFMPEGEncoder is just one thread, so make sure manageRequestAndResponse is very fast because
//	the time used by manageRequestAndResponse is time FFMPEGEncoder is not listening
//	for new connections (encodingStatus, ...)

void FFMPEGEncoder::manageRequestAndResponse(
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
        string errorMessage = string("The 'method' parameter is not found");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 400, errorMessage);

        throw runtime_error(errorMessage);
    }
    string method = methodIt->second;

    if (method == "status")
    {
        try
        {
            string responseBody = string("{ ")
                    + "\"status\": \"Encoder up and running\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				_logger->error(__FILEREF__ + errorMessage);

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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
		|| method == "vodProxy"
		|| method == "liveGrid"
		|| method == "awaitingTheBeginning"
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

				_logger->error(__FILEREF__ + errorMessage);

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
				else // if (method == "awaitingTheBeginning")
				{
					_logger->info(__FILEREF__ + "Creating awaitingTheBeginning thread"
						+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
						+ ", requestBody: " + requestBody
					);
					thread awaitingTheBeginningThread(&FFMPEGEncoder::awaitingTheBeginningThread,
						this, selectedLiveProxy, encodingJobKey, requestBody);
					awaitingTheBeginningThread.detach();
				}

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
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
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

				field = "encodingFinished";
				responseBodyRoot[field] = !selectedLiveProxy->_running;

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

        sendSuccess(request, 200, responseBody);
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

		_logger->info(__FILEREF__ + "Received killEncodingJob"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		);

		pid_t			pidToBeKilled;
		bool			encodingFound = false;

		{
			lock_guard<mutex> locker(*_encodingMutex);

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
			lock_guard<mutex> locker(*_liveProxyMutex);

			#ifdef __VECTOR__
			for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			{
				if (liveProxy->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					pidToBeKilled = liveProxy->_childPid;

					break;
				}
			}
			#else	// __MAP__
			map<int64_t, shared_ptr<LiveProxyAndGrid>>::iterator it =
				_liveProxiesCapability->find(encodingJobKey);
			if (it != _liveProxiesCapability->end())
			{
				encodingFound = true;
				pidToBeKilled = it->second->_childPid;
			}
			#endif
		}

		if (!encodingFound)
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			#ifdef __VECTOR__
			for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
			{
				if (liveRecording->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					pidToBeKilled = liveRecording->_childPid;

					break;
				}
			}
			#else	// __MAP__
			map<int64_t, shared_ptr<LiveRecording>>::iterator it =
				_liveRecordingsCapability->find(encodingJobKey);
			if (it != _liveRecordingsCapability->end())
			{
				encodingFound = true;
				pidToBeKilled = it->second->_childPid;
			}
			#endif
		}

        if (!encodingFound)
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
				);

        try
        {
			chrono::system_clock::time_point startKillProcess = chrono::system_clock::now();

			ProcessUtility::killProcess(pidToBeKilled);

			chrono::system_clock::time_point endKillProcess = chrono::system_clock::now();
			_logger->info(__FILEREF__ + "killProcess statistics"
				+ ", @MMS statistics@ - killProcess (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
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

        sendSuccess(request, 200, responseBody);
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

        int videoTrackIndexToBeUsed = JSONUtils::asInt(encodingMedatada["ingestedParametersRoot"], "VideoTrackIndex", -1);
        int audioTrackIndexToBeUsed = JSONUtils::asInt(encodingMedatada["ingestedParametersRoot"], "AudioTrackIndex", -1);

		Json::Value sourcesToBeEncodedRoot = encodingMedatada["encodingParametersRoot"]["sourcesToBeEncodedRoot"];
		Json::Value sourceToBeEncodedRoot = sourcesToBeEncodedRoot[0];

        string mmsSourceAssetPathName = sourceToBeEncodedRoot.get("mmsSourceAssetPathName", "").asString();
        int64_t durationInMilliSeconds = JSONUtils::asInt64(sourceToBeEncodedRoot,
				"sourceDurationInMilliSecs", -1);
        string stagingEncodedAssetPathName = encodingMedatada.get("stagingEncodedAssetPathName", "").asString();
		Json::Value encodingProfileDetailsRoot = encodingMedatada["encodingParametersRoot"]
			["encodingProfileDetailsRoot"];
        MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(
				encodingMedatada["encodingParametersRoot"].get("contentType", "").asString());
        int64_t physicalPathKey = JSONUtils::asInt64(sourceToBeEncodedRoot, "sourcePhysicalPathKey", -1);
        string workspaceDirectoryName = encodingMedatada.get("workspaceDirectoryName", "").asString();
        string relativePath = sourceToBeEncodedRoot.get("sourceRelativePath", "").asString();
        int64_t encodingJobKey = JSONUtils::asInt64(encodingMedatada, "encodingJobKey", -1);
        int64_t ingestionJobKey = JSONUtils::asInt64(encodingMedatada, "ingestionJobKey", -1);

		Json::Value videoTracksRoot;
		string field = "videoTracks";
        if (JSONUtils::isMetadataPresent(encodingMedatada, field))
			videoTracksRoot = encodingMedatada[field];
		Json::Value audioTracksRoot;
		field = "audioTracks";
        if (JSONUtils::isMetadataPresent(encodingMedatada, field))
			audioTracksRoot = encodingMedatada[field];

		// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
        encoding->_ffmpeg->encodeContent(
                mmsSourceAssetPathName,
                durationInMilliSeconds,
                // encodedFileName,
                stagingEncodedAssetPathName,
                encodingProfileDetailsRoot,
                contentType == MMSEngineDBFacade::ContentType::Video,
				videoTracksRoot,
				audioTracksRoot,
				videoTrackIndexToBeUsed, audioTrackIndexToBeUsed,
                physicalPathKey,
                workspaceDirectoryName,
                relativePath,
                encodingJobKey,
                ingestionJobKey,
				&(encoding->_childPid)
		);
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
        
        string mmsSourceVideoAssetPathName = overlayTextMedatada.get("mmsSourceVideoAssetPathName", "XXX").asString();
        int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(overlayTextMedatada, "videoDurationInMilliSeconds", -1);

        string text = overlayTextMedatada.get("text", "XXX").asString();
        
        string textPosition_X_InPixel;
        string field = "textPosition_X_InPixel";
        if (JSONUtils::isMetadataPresent(overlayTextMedatada, field))
            textPosition_X_InPixel = overlayTextMedatada.get(field, "XXX").asString();
        
        string textPosition_Y_InPixel;
        field = "textPosition_Y_InPixel";
        if (JSONUtils::isMetadataPresent(overlayTextMedatada, field))
            textPosition_Y_InPixel = overlayTextMedatada.get(field, "XXX").asString();
        
        string fontType;
        field = "fontType";
        if (JSONUtils::isMetadataPresent(overlayTextMedatada, field))
            fontType = overlayTextMedatada.get(field, "XXX").asString();

        int fontSize = -1;
        field = "fontSize";
        if (JSONUtils::isMetadataPresent(overlayTextMedatada, field))
            fontSize = JSONUtils::asInt(overlayTextMedatada, field, -1);

        string fontColor;
        field = "fontColor";
        if (JSONUtils::isMetadataPresent(overlayTextMedatada, field))
            fontColor = overlayTextMedatada.get(field, "XXX").asString();

        int textPercentageOpacity = -1;
        field = "textPercentageOpacity";
        if (JSONUtils::isMetadataPresent(overlayTextMedatada, field))
            textPercentageOpacity = JSONUtils::asInt(overlayTextMedatada, field, -1);

        bool boxEnable = false;
        field = "boxEnable";
        if (JSONUtils::isMetadataPresent(overlayTextMedatada, field))
            boxEnable = JSONUtils::asBool(overlayTextMedatada, field, false);

        string boxColor;
        field = "boxColor";
        if (JSONUtils::isMetadataPresent(overlayTextMedatada, field))
            boxColor = overlayTextMedatada.get(field, "XXX").asString();
        
        int boxPercentageOpacity = -1;
        field = "boxPercentageOpacity";
        if (JSONUtils::isMetadataPresent(overlayTextMedatada, field))
            boxPercentageOpacity = JSONUtils::asInt(overlayTextMedatada, "boxPercentageOpacity", -1);

        // string encodedFileName = overlayTextMedatada.get("encodedFileName", "XXX").asString();
        string stagingEncodedAssetPathName = overlayTextMedatada.get("stagingEncodedAssetPathName", "XXX").asString();
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
                boxEnable,
                boxColor,
                boxPercentageOpacity,

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
        
        string imageDirectory = generateFramesMedatada.get("imageDirectory", "XXX").asString();
        double startTimeInSeconds = JSONUtils::asDouble(generateFramesMedatada, "startTimeInSeconds", 0);
        int maxFramesNumber = JSONUtils::asInt(generateFramesMedatada, "maxFramesNumber", -1);
        string videoFilter = generateFramesMedatada.get("videoFilter", "XXX").asString();
        int periodInSeconds = JSONUtils::asInt(generateFramesMedatada, "periodInSeconds", -1);
        bool mjpeg = JSONUtils::asBool(generateFramesMedatada, "mjpeg", false);
        int imageWidth = JSONUtils::asInt(generateFramesMedatada, "imageWidth", -1);
        int imageHeight = JSONUtils::asInt(generateFramesMedatada, "imageHeight", -1);
        int64_t ingestionJobKey = JSONUtils::asInt64(generateFramesMedatada, "ingestionJobKey", -1);
        string mmsSourceVideoAssetPathName = generateFramesMedatada.get("mmsSourceVideoAssetPathName", "XXX").asString();
        int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(generateFramesMedatada, "videoDurationInMilliSeconds", -1);

        vector<string> generatedFramesFileNames = encoding->_ffmpeg->generateFramesToIngest(
                ingestionJobKey,
                encodingJobKey,
                imageDirectory,
                to_string(ingestionJobKey),    // imageBaseFileName,
                startTimeInSeconds,
                maxFramesNumber,
                videoFilter,
                periodInSeconds,
                mjpeg,
                imageWidth, 
                imageHeight,
                mmsSourceVideoAssetPathName,
                videoDurationInMilliSeconds,
				&(encoding->_childPid)
        );
        
        encoding->_running = false;
        encoding->_childPid = 0;
        
        _logger->info(__FILEREF__ + "generateFrames finished"
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

	string satelliteMulticastIP;
	string satelliteMulticastPort;
	int64_t satelliteServiceId = -1;
	int64_t satelliteFrequency = -1;
	int64_t satelliteSymbolRate = -1;
	string satelliteModulation;
	int satelliteVideoPid = -1;
	int satelliteAudioItalianPid = -1;

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

        liveRecording->_ingestionJobKey = JSONUtils::asInt64(liveRecorderMedatada, "ingestionJobKey", -1);

		// _transcoderStagingContentsPath is a transcoder LOCAL path, this is important because in case of high bitrate,
		//		nfs would not be enough fast and could create random file system error
        liveRecording->_transcoderStagingContentsPath = liveRecorderMedatada.get("transcoderStagingContentsPath", "XXX").asString();
        string userAgent = liveRecorderMedatada.get("userAgent", "").asString();

		// this is the global shared path where the chunks would be moved for the ingestion
        liveRecording->_stagingContentsPath = liveRecorderMedatada.get("stagingContentsPath", "").asString();
        liveRecording->_segmentListFileName = liveRecorderMedatada.get("segmentListFileName", "").asString();
        liveRecording->_recordedFileNamePrefix = liveRecorderMedatada.get("recordedFileNamePrefix", "").asString();
        liveRecording->_virtualVODStagingContentsPath = liveRecorderMedatada.get("virtualVODStagingContentsPath", "").asString();
        liveRecording->_liveRecorderVirtualVODImageMediaItemKey = JSONUtils::asInt64(liveRecorderMedatada,
			"liveRecorderVirtualVODImageMediaItemKey", -1);

		// _encodingParametersRoot has to be the last field to be set because liveRecorderChunksIngestion()
		//		checks this field is set before to see if there are chunks to be ingested
		liveRecording->_encodingParametersRoot = liveRecorderMedatada["encodingParametersRoot"];
		liveRecording->_liveRecorderParametersRoot = liveRecorderMedatada["liveRecorderParametersRoot"];
		liveRecording->_channelLabel =  liveRecording->_liveRecorderParametersRoot.get("ConfigurationLabel", "").asString();

        liveRecording->_channelType = liveRecording->_liveRecorderParametersRoot.get("ChannelType", "IP_MMSAsClient").asString();
		int ipMMSAsServer_listenTimeoutInSeconds = liveRecording->
			_liveRecorderParametersRoot.get("ActAsServerListenTimeout", 300).asInt();

		int captureLive_videoDeviceNumber = -1;
		string captureLive_videoInputFormat;
		int captureLive_frameRate = -1;
		int captureLive_width = -1;
		int captureLive_height = -1;
		int captureLive_audioDeviceNumber = -1;
		int captureLive_channelsNumber = -1;
		if (liveRecording->_channelType == "CaptureLive")
		{
			Json::Value captureLiveRoot =
				(liveRecording->_liveRecorderParametersRoot)["CaptureLive"];

			captureLive_videoDeviceNumber = JSONUtils::asInt(captureLiveRoot, "VideoDeviceNumber", -1);
			captureLive_videoInputFormat = captureLiveRoot.get("VideoInputFormat", "").asString();
			captureLive_frameRate = JSONUtils::asInt(captureLiveRoot, "FrameRate", -1);
			captureLive_width = JSONUtils::asInt(captureLiveRoot, "Width", -1);
			captureLive_height = JSONUtils::asInt(captureLiveRoot, "Height", -1);
			captureLive_audioDeviceNumber = JSONUtils::asInt(captureLiveRoot, "AudioDeviceNumber", -1);
			captureLive_channelsNumber = JSONUtils::asInt(captureLiveRoot, "ChannelsNumber", -1);
		}

        string liveURL;

		if (liveRecording->_channelType == "Satellite")
		{
			satelliteServiceId = JSONUtils::asInt64(
				liveRecorderMedatada["encodingParametersRoot"], "satelliteServiceId", -1);
			satelliteFrequency = JSONUtils::asInt64(
				liveRecorderMedatada["encodingParametersRoot"], "satelliteFrequency", -1);
			satelliteSymbolRate = JSONUtils::asInt64(
				liveRecorderMedatada["encodingParametersRoot"], "satelliteSymbolRate", -1);
			satelliteModulation = liveRecorderMedatada["encodingParametersRoot"].
				get("satelliteModulation", "").asString();
			satelliteVideoPid = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"], "satelliteVideoPid", -1);
			satelliteAudioItalianPid = JSONUtils::asInt(
				liveRecorderMedatada["encodingParametersRoot"], "satelliteAudioItalianPid", -1);

			// In case ffmpeg crashes and is automatically restarted, it should use the same
			// IP-PORT it was using before because we already have a dbvlast sending the stream
			// to the specified IP-PORT.
			// For this reason, before to generate a new IP-PORT, let's look for the serviceId
			// inside the dvblast conf. file to see if it was already running before

			pair<string, string> satelliteMulticast = getSatelliteMulticastFromDvblastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation);
			tie(satelliteMulticastIP, satelliteMulticastPort) = satelliteMulticast;

			if (satelliteMulticastIP == "")
			{
				lock_guard<mutex> locker(*_satelliteChannelsPortsMutex);

				satelliteMulticastIP = "239.255.1.1";
				satelliteMulticastPort = to_string(*_satelliteChannelPort_CurrentOffset
					+ _satelliteChannelPort_Start);

				*_satelliteChannelPort_CurrentOffset = (*_satelliteChannelPort_CurrentOffset + 1)
					% _satelliteChannelPort_MaxNumberOfOffsets;
			}

			liveURL = string("udp://@") + satelliteMulticastIP + ":" + satelliteMulticastPort;

			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				true);
		}
		else
		{
			// in case of actAsServer
			//	true: it is set into the MMSEngineProcessor::manageLiveRecorder method
			//	false: it comes from the LiveRecorder json ingested
			liveURL = liveRecorderMedatada.get("liveURL", "").asString();
		}

        time_t utcRecordingPeriodStart = JSONUtils::asInt64(liveRecording->_encodingParametersRoot,
			"utcRecordingPeriodStart", -1);
        time_t utcRecordingPeriodEnd = JSONUtils::asInt64(liveRecording->_encodingParametersRoot,
			"utcRecordingPeriodEnd", -1);
        int segmentDurationInSeconds = JSONUtils::asInt(liveRecording->_encodingParametersRoot,
			"segmentDurationInSeconds", -1);
        string outputFileFormat = liveRecording->_encodingParametersRoot
			.get("outputFileFormat", "").asString();


		bool monitorHLS;
		bool virtualVOD;
		// Json::Value monitorVirtualVODEncodingProfileDetailsRoot = Json::nullValue;
		// bool monitorIsVideo = true;
		// string monitorManifestDirectoryPath;
		// string monitorManifestFileName;
		// int monitorVirtualVODPlaylistEntriesNumber = -1;
		// int monitorVirtualVODSegmentDurationInSeconds = -1;
		{
			monitorHLS = JSONUtils::asBool(liveRecording->_encodingParametersRoot,
				"monitorHLS", false);
			liveRecording->_virtualVOD = JSONUtils::asBool(liveRecording->_encodingParametersRoot,
				"liveRecorderVirtualVOD", false);

			if (monitorHLS || liveRecording->_virtualVOD)
			{
				// Json::Value monitorHLSRoot = liveRecording->_liveRecorderParametersRoot["MonitorHLS"];

				liveRecording->_monitorVirtualVODManifestDirectoryPath = liveRecording->_encodingParametersRoot
					.get("monitorManifestDirectoryPath", "").asString();
				liveRecording->_monitorVirtualVODManifestFileName = liveRecording->_encodingParametersRoot
					.get("monitorManifestFileName", "").asString();
				/*
				monitorVirtualVODPlaylistEntriesNumber = JSONUtils::asInt(liveRecording->_encodingParametersRoot,
					"monitorVirtualVODPlaylistEntriesNumber", 6);
				monitorVirtualVODSegmentDurationInSeconds = JSONUtils::asInt(liveRecording->_encodingParametersRoot,
					"monitorVirtualVODSegmentDurationInSeconds", 10);

				monitorVirtualVODEncodingProfileDetailsRoot =
					liveRecorderMedatada["monitorVirtualVODEncodingProfileDetailsRoot"];
				string monitorVirtualVODEncodingProfileContentType =
					liveRecorderMedatada.get("monitorVirtualVODEncodingProfileContentType", "Video").asString();

				monitorIsVideo = monitorVirtualVODEncodingProfileContentType == "Video" ? true : false;

				if (FileIO::directoryExisting(monitorManifestDirectoryPath))
				{
					try
					{
						_logger->info(__FILEREF__ + "removeDirectory"
							+ ", monitorManifestDirectoryPath: " + monitorManifestDirectoryPath
						);
						Boolean_t bRemoveRecursively = true;
						FileIO::removeDirectory(monitorManifestDirectoryPath, bRemoveRecursively);
					}
					catch(runtime_error e)
					{
						string errorMessage = __FILEREF__ + "remove directory failed"
							+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", monitorManifestDirectoryPath: " + monitorManifestDirectoryPath
							+ ", e.what(): " + e.what()
						;
						_logger->error(errorMessage);

						// throw e;
					}
					catch(exception e)
					{
						string errorMessage = __FILEREF__ + "remove directory failed"
							+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", monitorManifestDirectoryPath: " + monitorManifestDirectoryPath
							+ ", e.what(): " + e.what()
						;
						_logger->error(errorMessage);

						// throw e;
					}
				}
				*/
			}
		}

		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath
			+ liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
					exceptionInCaseOfError);
		}

		// since the first chunk is discarded, we will start recording before the period of the chunk
		// In case of autorenew, when it is renewed, we will lose the first chunk
		// utcRecordingPeriodStart -= segmentDurationInSeconds;
		// 2019-12-19: the above problem is managed inside _ffmpeg->liveRecorder
		//		(see the secondsToStartEarly variable inside _ffmpeg->liveRecorder)
		//		For this reason the above decrement was commented

		// based on liveProxy->_proxyStart, the monitor thread starts the checkings
		// In case of IP_MMSAsServer, the checks should be done after the ffmpeg server
		// receives the stream and we do not know what it happens.
		// For this reason, in this scenario, we have to set _proxyStart in the worst scenario
		if (liveRecording->_channelType == "IP_MMSAsServer")
		{
			if (chrono::system_clock::from_time_t(utcRecordingPeriodStart) < chrono::system_clock::now())
				liveRecording->_recordingStart = chrono::system_clock::now() +
					chrono::seconds(ipMMSAsServer_listenTimeoutInSeconds);
			else
				liveRecording->_recordingStart = chrono::system_clock::from_time_t(utcRecordingPeriodStart) +
					chrono::seconds(ipMMSAsServer_listenTimeoutInSeconds);
		}
		else
		{
			if (chrono::system_clock::from_time_t(utcRecordingPeriodStart) < chrono::system_clock::now())
				liveRecording->_recordingStart = chrono::system_clock::now();
			else
				liveRecording->_recordingStart = chrono::system_clock::from_time_t(utcRecordingPeriodStart);
		}

		liveRecording->_liveRecorderOutputRoots.clear();
		{
			Json::Value outputsRoot = liveRecording->_encodingParametersRoot["outputsRoot"];

			{
				Json::StreamWriterBuilder wbuilder;
				string sOutputsRoot = Json::writeString(wbuilder, outputsRoot);

				_logger->info(__FILEREF__ + "liveRecorder. outputsRoot"
					+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", sOutputsRoot: " + sOutputsRoot
				);
			}

			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				string otherOutputOptions;
				string audioVolumeChange;
				Json::Value encodingProfileDetailsRoot = Json::nullValue;
				string manifestDirectoryPath;
				string manifestFileName;
				int localSegmentDurationInSeconds = -1;
				int playlistEntriesNumber = -1;
				bool isVideo = true;
				string rtmpUrl;

				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = outputRoot.get("outputType", "").asString();

				if (outputType == "HLS" || outputType == "DASH")
				{
					otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();
					audioVolumeChange = outputRoot.get("audioVolumeChange", "").asString();
					encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];
					manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "").asString();
					manifestFileName = outputRoot.get("manifestFileName", "").asString();
					localSegmentDurationInSeconds = JSONUtils::asInt(outputRoot, "segmentDurationInSeconds", 10);
					playlistEntriesNumber = JSONUtils::asInt(outputRoot, "playlistEntriesNumber", 5);

					string encodingProfileContentType = outputRoot.get("encodingProfileContentType", "Video").asString();

					isVideo = encodingProfileContentType == "Video" ? true : false;
				}
				else if (outputType == "RTMP_Stream")
				{
					otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();
					audioVolumeChange = outputRoot.get("audioVolumeChange", "").asString();
					encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];
					rtmpUrl = outputRoot.get("rtmpUrl", "").asString();

					string encodingProfileContentType = outputRoot.get("encodingProfileContentType", "Video").asString();
					isVideo = encodingProfileContentType == "Video" ? true : false;
				}
				else
				{
					string errorMessage = __FILEREF__ + "liveRecorder. Wrong output type"
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", outputType: " + outputType;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				tuple<string, string, string, Json::Value, string, string, int, int, bool, string> tOutputRoot
					= make_tuple(outputType, otherOutputOptions, audioVolumeChange, encodingProfileDetailsRoot,
						manifestDirectoryPath, manifestFileName, localSegmentDurationInSeconds, playlistEntriesNumber,
						isVideo, rtmpUrl);

				liveRecording->_liveRecorderOutputRoots.push_back(tOutputRoot);

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
							FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
						}
						catch(runtime_error e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
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
								+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
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

		liveRecording->_segmenterType = "hlsSegmenter";
		// liveRecording->_segmenterType = "streamSegmenter";

		liveRecording->_ffmpeg->liveRecorder(
			liveRecording->_ingestionJobKey,
			encodingJobKey,
			liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
			liveRecording->_recordedFileNamePrefix,

			liveRecording->_channelType,
			StringUtils::trimTabToo(liveURL),
			ipMMSAsServer_listenTimeoutInSeconds,
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

			monitorHLS,
			liveRecording->_virtualVOD,
			// monitorVirtualVODEncodingProfileDetailsRoot,
			// monitorIsVideo,
			// liveRecording->_monitorVirtualVODManifestDirectoryPath,
			// liveRecording->_monitorVirtualVODManifestFileName,
			// monitorVirtualVODPlaylistEntriesNumber,
			// monitorVirtualVODSegmentDurationInSeconds,

			liveRecording->_liveRecorderOutputRoots,

			&(liveRecording->_childPid)
		);

		if (liveRecording->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

		// to wait the ingestion of the last chunk
		this_thread::sleep_for(chrono::seconds(2 * _liveRecorderChunksIngestionCheckInSeconds));

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_childPid = 0;
        liveRecording->_killedBecauseOfNotWorking = false;
        
        _logger->info(__FILEREF__ + "liveRecorded finished"
            + ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );

        liveRecording->_ingestionJobKey		= 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_liveRecorderOutputRoots.clear();

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
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath
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
				_logger->error(__FILEREF__ + "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		if (liveRecording->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_liveRecorderOutputRoots.clear();

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
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath
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
				_logger->error(__FILEREF__ + "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(FFMpegURLForbidden e)
    {
		if (liveRecording->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;
		liveRecording->_liveRecorderOutputRoots.clear();

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
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
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
				_logger->error(__FILEREF__ + "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(FFMpegURLNotFound e)
    {
		if (liveRecording->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;
		liveRecording->_liveRecorderOutputRoots.clear();

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
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
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
				_logger->error(__FILEREF__ + "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(runtime_error e)
    {
		if (liveRecording->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;
		liveRecording->_liveRecorderOutputRoots.clear();

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
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
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
				_logger->error(__FILEREF__ + "_liveRecordingsCapability->erase. Key not found"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", erase: " + to_string(erase)
				);
		}
		#endif
    }
    catch(exception e)
    {
		if (liveRecording->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveRecording->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;
		liveRecording->_liveRecorderOutputRoots.clear();

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
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath
					+ liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
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
				_logger->error(__FILEREF__ + "_liveRecordingsCapability->erase. Key not found"
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
									liveRecording->_channelType,
									// highAvailability, main,
									segmentDurationInSeconds, outputFileFormat,                                                                              
									liveRecording->_encodingParametersRoot,
									liveRecording->_liveRecorderParametersRoot,

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
									liveRecording->_channelType,
									// highAvailability, main,
									segmentDurationInSeconds, outputFileFormat,                                                                              
									liveRecording->_encodingParametersRoot,
									liveRecording->_liveRecorderParametersRoot,

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
		try
		{
			chrono::system_clock::time_point startAllChannelsVirtualVOD = chrono::system_clock::now();

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

				if (liveRecording->_running && liveRecording->_virtualVOD)
				{
					_logger->info(__FILEREF__ + "liveRecorder_buildAndIngestVirtualVOD ..."
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
						+ ", running: " + to_string(liveRecording->_running)
						+ ", virtualVOD: " + to_string(liveRecording->_virtualVOD)
					);

					chrono::system_clock::time_point startSingleChannelVirtualVOD = chrono::system_clock::now();

					long segmentsNumber = 0;

					try
					{
						int64_t deliveryCode = JSONUtils::asInt64(
							liveRecording->_liveRecorderParametersRoot, "DeliveryCode", 0);
						string ingestionJobLabel = liveRecording->_encodingParametersRoot
							.get("ingestionJobLabel", "").asString();
						string liveRecorderVirtualVODUniqueName = ingestionJobLabel + "("
							+ to_string(deliveryCode) + "_" + to_string(liveRecording->_ingestionJobKey)
							+ ")";

						int64_t userKey;
						string apiKey;
						{
							string field = "InternalMMS";
							if (JSONUtils::isMetadataPresent(liveRecording->_liveRecorderParametersRoot, field))
							{
								// internalMMSRootPresent = true;

								Json::Value internalMMSRoot = liveRecording->_liveRecorderParametersRoot[field];

								field = "userKey";
								userKey = JSONUtils::asInt64(internalMMSRoot, field, -1);

								field = "apiKey";
								apiKey = internalMMSRoot.get(field, "").asString();

							}
						}

						segmentsNumber = liveRecorder_buildAndIngestVirtualVOD(
							liveRecording->_ingestionJobKey,
							liveRecording->_encodingJobKey,

							liveRecording->_monitorVirtualVODManifestDirectoryPath,
							liveRecording->_monitorVirtualVODManifestFileName,
							liveRecording->_virtualVODStagingContentsPath,

							deliveryCode,
							ingestionJobLabel,
							liveRecorderVirtualVODUniqueName,
							_liveRecorderVirtualVODRetention,
							liveRecording->_liveRecorderVirtualVODImageMediaItemKey,
							userKey,
							apiKey);
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveRecorder_buildAndIngestVirtualVOD failed")
							+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveRecorder_buildAndIngestVirtualVOD failed")
							+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}

					_logger->info(__FILEREF__ + "Single Channel Virtual VOD"
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
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
	string channelType,
	// bool highAvailability, bool main,
	int segmentDurationInSeconds, string outputFileFormat,
	Json::Value encodingParametersRoot,
	Json::Value liveRecorderParametersRoot,
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
				int64_t deliveryCode = JSONUtils::asInt64(liveRecorderParametersRoot, "DeliveryCode", 0);

				uniqueName = to_string(deliveryCode);
				uniqueName += " - ";
				uniqueName += to_string(utcCurrentRecordedFileCreationTime);
			}

			string ingestionJobLabel = encodingParametersRoot.get("ingestionJobLabel", "").asString();

			// UserData
			Json::Value userDataRoot;
			{
				if (JSONUtils::isMetadataPresent(liveRecorderParametersRoot, "UserData"))
					userDataRoot = liveRecorderParametersRoot["UserData"];

				Json::Value mmsDataRoot;
				mmsDataRoot["dataType"] = "liveRecordingChunk";
				/*
				mmsDataRoot["channelType"] = channelType;
				if (channelType == "IP_MMSAsClient")
					mmsDataRoot["ipConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
				else if (channelType == "Satellite")
					mmsDataRoot["satConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
				else // if (channelType == "IP_MMSAsServer")
				*/
				{
					int64_t deliveryCode = JSONUtils::asInt64(liveRecorderParametersRoot,
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
				if (channelType == "IP_MMSAsServer")
				{
					int64_t deliveryCode = JSONUtils::asInt64(liveRecorderParametersRoot,
						"DeliveryCode", 0);
					addContentTitle = to_string(deliveryCode);
				}
				else
				{
					// 2021-02-03: in this case, we will use the 'ConfigurationLabel' that
					// it is much better that a code. Who will see the title of the chunks will recognize
					// easily the recording
					addContentTitle = liveRecorderParametersRoot.get("ConfigurationLabel", "").asString();
				}
				*/
				// string ingestionJobLabel = encodingParametersRoot.get("ingestionJobLabel", "").asString();
				if (ingestionJobLabel == "")
				{
					int64_t deliveryCode = JSONUtils::asInt64(liveRecorderParametersRoot,
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

					liveRecorder_ingestRecordedMedia(ingestionJobKey,
						transcoderStagingContentsPath, currentRecordedAssetFileName,
						stagingContentsPath,
						addContentTitle, uniqueName, /* highAvailability, */ userDataRoot, outputFileFormat,
						liveRecorderParametersRoot, encodingParametersRoot,
						false);
				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "liveRecorder_ingestRecordedMedia failed"
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
	string channelType,
	// bool highAvailability, bool main,
	int segmentDurationInSeconds, string outputFileFormat,
	Json::Value encodingParametersRoot,
	Json::Value liveRecorderParametersRoot,
	string transcoderStagingContentsPath,
	string stagingContentsPath,
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
			+ ", stagingContentsPath: " + stagingContentsPath
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
						_logger->info(__FILEREF__ + errorMessage);

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

					// ingestion
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

						{
							string uniqueName;
							{
								int64_t deliveryCode = JSONUtils::asInt64(liveRecorderParametersRoot, "DeliveryCode", 0);

								uniqueName = to_string(deliveryCode);
								uniqueName += " - ";
								uniqueName += to_string(toBeIngestedSegmentUtcStartTimeInMillisecs);
							}

							string ingestionJobLabel = encodingParametersRoot.get("ingestionJobLabel", "").asString();

							// UserData
							Json::Value userDataRoot;
							{
								if (JSONUtils::isMetadataPresent(liveRecorderParametersRoot, "UserData"))
									userDataRoot = liveRecorderParametersRoot["UserData"];

								Json::Value mmsDataRoot;
								mmsDataRoot["dataType"] = "liveRecordingChunk";
								/*
								mmsDataRoot["channelType"] = channelType;
								if (channelType == "IP_MMSAsClient")
									mmsDataRoot["ipConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
								else if (channelType == "Satellite")
									mmsDataRoot["satConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
								else // if (channelType == "IP_MMSAsServer")
								*/
								{
									int64_t deliveryCode = JSONUtils::asInt64(liveRecorderParametersRoot,
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
									int64_t deliveryCode = JSONUtils::asInt64(liveRecorderParametersRoot,
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
										+ ", stagingContentsPath: " + stagingContentsPath
										+ ", addContentTitle: " + addContentTitle
									);

									liveRecorder_ingestRecordedMedia(ingestionJobKey,
										transcoderStagingContentsPath, toBeIngestedSegmentFileName,
										stagingContentsPath,
										addContentTitle, uniqueName, /* highAvailability, */ userDataRoot, outputFileFormat,
										liveRecorderParametersRoot, encodingParametersRoot,
										true);
								}
								catch(runtime_error e)
								{
									_logger->error(__FILEREF__ + "liveRecorder_ingestRecordedMedia failed"
										+ ", encodingJobKey: " + to_string(encodingJobKey)
										+ ", ingestionJobKey: " + to_string(ingestionJobKey)
										+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
										+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
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
										+ ", transcoderStagingContentsPath: " + transcoderStagingContentsPath
										+ ", toBeIngestedSegmentFileName: " + toBeIngestedSegmentFileName
										+ ", stagingContentsPath: " + stagingContentsPath
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
			if (lastRecordedAssetFileName != ""		// file name is present
				&& !toBeIngested					// file name does not exist into the playlist
			)
			{
				_logger->error(__FILEREF__ + "Filename not found: scenario that should never happen"
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
			}
		}
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "liveRecorder_processHLSSegmenterOutput failed"
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
        _logger->error(__FILEREF__ + "liveRecorder_processHLSSegmenterOutput failed"
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

void FFMPEGEncoder::liveRecorder_ingestRecordedMedia(
	int64_t ingestionJobKey,
	string transcoderStagingContentsPath, string currentRecordedAssetFileName,
	string stagingContentsPath,
	string addContentTitle,
	string uniqueName,
	// bool highAvailability,
	Json::Value userDataRoot,
	string fileFormat,
	Json::Value liveRecorderParametersRoot,
	Json::Value encodingParametersRoot,
	bool copy)
{
	try
	{
		// moving chunk from transcoder staging path to shared staging path
		if (copy)
		{
			_logger->info(__FILEREF__ + "Chunk copying"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + stagingContentsPath
			);

			chrono::system_clock::time_point startCopying = chrono::system_clock::now();
			FileIO::copyFile(transcoderStagingContentsPath + currentRecordedAssetFileName, stagingContentsPath);
			chrono::system_clock::time_point endCopying = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Chunk copied"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + stagingContentsPath
				+ ", @MMS COPY statistics@ - copyingDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endCopying - startCopying).count()) + "@"
			);
		}
		else
		{
			_logger->info(__FILEREF__ + "Chunk moving"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + stagingContentsPath
			);

			chrono::system_clock::time_point startMoving = chrono::system_clock::now();
			FileIO::moveFile(transcoderStagingContentsPath + currentRecordedAssetFileName, stagingContentsPath);
			chrono::system_clock::time_point endMoving = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Chunk moved"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", source: " + transcoderStagingContentsPath + currentRecordedAssetFileName
				+ ", dest: " + stagingContentsPath
				+ ", @MMS MOVE statistics@ - movingDuration (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(endMoving - startMoving).count()) + "@"
			);
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = e.what();
		_logger->error(__FILEREF__ + "Moving of the chink failed"
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

	string mmsAPIURL;
	ostringstream response;
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

		// bool internalMMSRootPresent = false;
		int64_t userKey;
		string apiKey;
		{
			field = "InternalMMS";
    		if (JSONUtils::isMetadataPresent(liveRecorderParametersRoot, field))
			{
				// internalMMSRootPresent = true;

				Json::Value internalMMSRoot = liveRecorderParametersRoot[field];

				field = "userKey";
				userKey = JSONUtils::asInt64(internalMMSRoot, field, -1);

				field = "apiKey";
				apiKey = internalMMSRoot.get(field, "").asString();

				field = "OnSuccess";
    			if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
					addContentRoot[field] = internalMMSRoot[field];

				field = "OnError";
    			if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
					addContentRoot[field] = internalMMSRoot[field];

				field = "OnComplete";
    			if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
					addContentRoot[field] = internalMMSRoot[field];
			}
		}

		Json::Value addContentParametersRoot = liveRecorderParametersRoot;
		// if (internalMMSRootPresent)
		{
			Json::Value removed;
			field = "InternalMMS";
			addContentParametersRoot.removeMember(field, &removed);
		}

		field = "FileFormat";
		addContentParametersRoot[field] = fileFormat;

		string sourceURL = string("move") + "://" + stagingContentsPath + currentRecordedAssetFileName;
		field = "SourceURL";
		addContentParametersRoot[field] = sourceURL;

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

			int64_t deliveryCode = JSONUtils::asInt64(liveRecorderParametersRoot, "DeliveryCode", 0);
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

		mmsAPIURL =
			_mmsAPIProtocol
			+ "://"
			+ _mmsAPIHostname + ":"
			+ to_string(_mmsAPIPort)
			+ _mmsAPIIngestionURI
            ;

		list<string> header;

		header.push_back("Content-Type: application/json");
		{
			// string userPasswordEncoded = Convert::base64_encode(_mmsAPIUser + ":" + _mmsAPIPassword);
			string userPasswordEncoded = Convert::base64_encode(to_string(userKey) + ":" + apiKey);
			string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

			header.push_back(basicAuthorization);
		}

		curlpp::Cleanup cleaner;
		curlpp::Easy request;

		// Setting the URL to retrive.
		request.setOpt(new curlpp::options::Url(mmsAPIURL));

		// timeout consistent with nginx configuration (fastcgi_read_timeout)
		request.setOpt(new curlpp::options::Timeout(_mmsAPITimeoutInSeconds));

		if (_mmsAPIProtocol == "https")
		{
			/*
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
			typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
			typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
			typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
			typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
			typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
			typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
			typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
			typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
			*/

			/*
			// cert is stored PEM coded in file... 
			// since PEM is default, we needn't set it for PEM 
			// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
			curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
			equest.setOpt(sslCertType);

			// set the cert for client authentication
			// "testcert.pem"
			// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
			curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
			request.setOpt(sslCert);
			*/

			/*
			// sorry, for engine we must set the passphrase
			//   (if the key has one...)
			// const char *pPassphrase = NULL;
			if(pPassphrase)
			curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

			// if we use a key stored in a crypto engine,
			//   we must set the key type to "ENG"
			// pKeyType  = "PEM";
			curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

			// set the private key (file or ID in engine)
			// pKeyName  = "testkey.pem";
			curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

			// set the file with the certs vaildating the server
			// *pCACertFile = "cacert.pem";
			curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
			*/

			// disconnect if we can't validate server's cert
			bool bSslVerifyPeer = false;
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
			request.setOpt(sslVerifyPeer);
               
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
			request.setOpt(sslVerifyHost);
               
			// request.setOpt(new curlpp::options::SslEngineDefault());                                              
		}

		request.setOpt(new curlpp::options::HttpHeader(header));
		request.setOpt(new curlpp::options::PostFields(workflowMetadata));
		request.setOpt(new curlpp::options::PostFieldSize(workflowMetadata.length()));

		request.setOpt(new curlpp::options::WriteStream(&response));

		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "Ingesting recorded media file"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
		);
		chrono::system_clock::time_point startIngesting = chrono::system_clock::now();
		request.perform();
		chrono::system_clock::time_point endIngesting = chrono::system_clock::now();

		string sResponse = response.str();
		// LF and CR create problems to the json parser...
		while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
			sResponse.pop_back();

		long responseCode = curlpp::infos::ResponseCode::get(request);
		if (responseCode == 201)
		{
			string message = __FILEREF__ + "Ingested recorded response"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", @MMS statistics@ - ingestingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endIngesting - startIngesting).count()) + "@"
				+ ", workflowMetadata: " + workflowMetadata
				+ ", sResponse: " + sResponse
				;
			_logger->info(message);
		}
		else
		{
			string message = __FILEREF__ + "Ingested recorded response"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", @MMS statistics@ - ingestingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endIngesting - startIngesting).count()) + "@"
				+ ", workflowMetadata: " + workflowMetadata
				+ ", sResponse: " + sResponse
				+ ", responseCode: " + to_string(responseCode)
				;
			_logger->error(message);

           	throw runtime_error(message);
		}
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (LogicError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (RuntimeError)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingested URL failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
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

	string sourceSegmentsDirectoryPathName,
	string sourceManifestFileName,
	string stagingLiveRecorderVirtualVODPathName,

	int64_t deliveryCode,
	string liveRecorderIngestionJobLabel,
	string liveRecorderVirtualVODUniqueName,
	string liveRecorderVirtualVODRetention,
	int64_t liveRecorderVirtualVODImageMediaItemKey,
	int64_t liveRecorderUserKey,
	string liveRecorderApiKey
)
{

	long segmentsNumber = 0;

	// let's build the live recorder virtual VOD
	// - copy current manifest and TS files
	// - calculate start end time of the virtual VOD
	// - add end line to manifest
	// - create tar gz
	// - remove directory
	int64_t utcStartTimeInMilliSecs = -1;
	int64_t utcEndTimeInMilliSecs = -1;

	string liveRecorderVirtualVODName;
	string tarGzStagingLiveRecorderVirtualVODPathName;
	try
	{
		{
			liveRecorderVirtualVODName = to_string(liveRecorderIngestionJobKey)
				+ "_liveRecorderVirtualVOD"
			;

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

		string copiedManifestPathFileName = stagingLiveRecorderVirtualVODPathName + "/" +
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
				+ ", copiedManifestPathFileName: " + copiedManifestPathFileName
			);
			FileIO::copyFile(sourceManifestPathFileName, copiedManifestPathFileName);
		}

		if (!FileIO::isFileExisting (copiedManifestPathFileName.c_str()))
		{
			string errorMessage = string("manifest file not existing")
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", copiedManifestPathFileName: " + copiedManifestPathFileName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

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
				+ ", copiedManifestPathFileName: " + copiedManifestPathFileName
			);

			ifstream ifManifestFile(copiedManifestPathFileName);
			if (!ifManifestFile.is_open())
			{
				string errorMessage = string("Not authorized: manifest file not opened")
					+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
					+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
					+ ", copiedManifestPathFileName: " + copiedManifestPathFileName
				;
				_logger->info(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			segmentsNumber = 0;
			string manifestLine;
			while(getline(ifManifestFile, manifestLine))
			{
				// #EXTINF:14.640000,
				// #EXT-X-PROGRAM-DATE-TIME:2021-02-26T15:41:15.477+0100
				// liveRecorder_1479919_334303_1622362660.ts

				string prefix ("#EXTINF:");
				if (manifestLine.size() >= prefix.size()
					&& 0 == manifestLine.compare(0, prefix.size(), prefix))
				{
					size_t endOfSegmentDuration = manifestLine.find(",");
					if (endOfSegmentDuration == string::npos)
					{
						string errorMessage = string("wrong manifest line format")
							+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
							+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
							+ ", manifestLine: " + manifestLine
						;
						_logger->info(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}

					lastSegmentDuration = stod(manifestLine.substr(prefix.size(),
						endOfSegmentDuration - prefix.size()));
				}

				prefix = "#EXT-X-PROGRAM-DATE-TIME:";
				if (manifestLine.size() >= prefix.size() && 0 == manifestLine.compare(0, prefix.size(), prefix))
					lastSegmentUtcStartTimeInMillisecs = DateTime::sDateMilliSecondsToUtc(manifestLine.substr(prefix.size()));

				if (firstSegmentDuration == -1.0 && firstSegmentUtcStartTimeInMillisecs == -1
						&& lastSegmentDuration != -1.0 && lastSegmentUtcStartTimeInMillisecs != -1)
				{
					firstSegmentDuration = lastSegmentDuration;
					firstSegmentUtcStartTimeInMillisecs = lastSegmentUtcStartTimeInMillisecs;
				}

				if (manifestLine != "" && manifestLine[0] != '#')
				{
					string sourceTSPathFileName = sourceSegmentsDirectoryPathName + "/" +
						manifestLine;
					string copiedTSPathFileName = stagingLiveRecorderVirtualVODPathName + "/" +
						manifestLine;

					_logger->info(__FILEREF__ + "Coping"
						+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
						+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
						+ ", sourceTSPathFileName: " + sourceTSPathFileName
						+ ", copiedTSPathFileName: " + copiedTSPathFileName
					);
					FileIO::copyFile(sourceTSPathFileName, copiedTSPathFileName);

					segmentsNumber++;
				}
			}
		}
		utcStartTimeInMilliSecs = firstSegmentUtcStartTimeInMillisecs;
		utcEndTimeInMilliSecs = lastSegmentUtcStartTimeInMillisecs + (lastSegmentDuration * 1000);

		// add end list to manifest file
		{
			_logger->info(__FILEREF__ + "Add end manifest line to copied manifest file"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", copiedManifestPathFileName: " + copiedManifestPathFileName
			);

			string endLine = "\n";
			ofstream ofManifestFile(copiedManifestPathFileName, ofstream::app);
			ofManifestFile << endLine << "#EXT-X-ENDLIST" + endLine;
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
					+ " " + liveRecorderVirtualVODName;
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

		// 2021-05-30: changed from copy to move with the idea to have better performance
		string sourceURL = string("move") + "://" + tarGzStagingLiveRecorderVirtualVODPathName;
        field = "SourceURL";
        addContentParametersRoot[field] = sourceURL;

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
	ostringstream response;
	string mmsAPIURL;
	try
	{
		mmsAPIURL =
			_mmsAPIProtocol
			+ "://"
			+ _mmsAPIHostname + ":"
			+ to_string(_mmsAPIPort)
			+ _mmsAPIIngestionURI
           ;

		list<string> header;

		header.push_back("Content-Type: application/json");
		{
			// string userPasswordEncoded = Convert::base64_encode(_mmsAPIUser + ":" + _mmsAPIPassword);
			string userPasswordEncoded = Convert::base64_encode(to_string(liveRecorderUserKey) + ":" + liveRecorderApiKey);
			string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

			header.push_back(basicAuthorization);
		}

		curlpp::Cleanup cleaner;
		curlpp::Easy request;

		// Setting the URL to retrive.
		request.setOpt(new curlpp::options::Url(mmsAPIURL));

		// timeout consistent with nginx configuration (fastcgi_read_timeout)
		request.setOpt(new curlpp::options::Timeout(_mmsAPITimeoutInSeconds));

		if (_mmsAPIProtocol == "https")
		{
			// disconnect if we can't validate server's cert
			bool bSslVerifyPeer = false;
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
			request.setOpt(sslVerifyPeer);
              
			curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
			request.setOpt(sslVerifyHost);
              
			// request.setOpt(new curlpp::options::SslEngineDefault());                                              
		}

		request.setOpt(new curlpp::options::HttpHeader(header));
		request.setOpt(new curlpp::options::PostFields(workflowMetadata));
		request.setOpt(new curlpp::options::PostFieldSize(workflowMetadata.length()));

		request.setOpt(new curlpp::options::WriteStream(&response));

		_logger->info(__FILEREF__ + "Ingesting Live Recorder VOD workflow"
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey) 
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
		);
		chrono::system_clock::time_point startIngesting = chrono::system_clock::now();
		request.perform();
		chrono::system_clock::time_point endIngesting = chrono::system_clock::now();

		string sResponse = response.str();
		// LF and CR create problems to the json parser...
		while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
			sResponse.pop_back();

		long responseCode = curlpp::infos::ResponseCode::get(request);
		if (responseCode == 201)
		{
			string message = __FILEREF__ + "Ingested Live Recorder VOD workflow response"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey) 
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", @MMS statistics@ - ingestingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endIngesting - startIngesting).count()) + "@"
				+ ", workflowMetadata: " + workflowMetadata
				+ ", sResponse: " + sResponse
				;
			_logger->info(message);
		}
		else
		{
			string message = __FILEREF__ + "Ingested Live Recorder VOD workflow response"
				+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey) 
				+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
				+ ", @MMS statistics@ - ingestingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endIngesting - startIngesting).count()) + "@"
				+ ", workflowMetadata: " + workflowMetadata
				+ ", sResponse: " + sResponse
				+ ", responseCode: " + to_string(responseCode)
				;
			_logger->error(message);

			throw runtime_error(message);
		}
	}
	catch (curlpp::LogicError& e)
	{
		string errorMessage = string("ingest live recorder VOD failed (LogicError)")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", e.what: " + e.what()
			+ ", response.str(): " + response.str()
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
	catch (curlpp::RuntimeError& e)
	{
		string errorMessage = string("ingest live recorder VOD failed (RuntimeError)")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", e.what: " + e.what()
			+ ", response.str(): " + response.str()
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
	catch (runtime_error e)
	{
		string errorMessage = string("ingest live recorder VOD failed")
			+ ", liveRecorderIngestionJobKey: " + to_string(liveRecorderIngestionJobKey)
			+ ", liveRecorderEncodingJobKey: " + to_string(liveRecorderEncodingJobKey)
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", e.what: " + e.what()
			+ ", response.str(): " + response.str()
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
			+ ", mmsAPIURL: " + mmsAPIURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", response.str(): " + response.str()
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

	return segmentsNumber;
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

	string satelliteMulticastIP;
	string satelliteMulticastPort;
	int64_t satelliteServiceId = -1;
	int64_t satelliteFrequency = -1;
	int64_t satelliteSymbolRate = -1;
	string satelliteModulation;
	int satelliteVideoPid = -1;
	int satelliteAudioItalianPid = -1;
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

		// CHECK TO BE COMMENTED AFTER LAUNCH IN PRODUCTION
		/*
		if (!JSONUtils::isMetadataPresent(liveProxyMetadata, "outputsRoot"))
		{
			string errorMessage = string("Forced FFMpegEncodingKilledByUser")
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", requestBody: " + requestBody
			;
			_logger->error(__FILEREF__ + errorMessage);

			liveProxy->_killedBecauseOfNotWorking = false;

			throw FFMpegEncodingKilledByUser();
		}
		*/

		liveProxy->_liveProxyOutputRoots.clear();
		{
			Json::Value outputsRoot = liveProxyMetadata["encodingParametersRoot"]["outputsRoot"];

			{
				Json::StreamWriterBuilder wbuilder;
				string sOutputsRoot = Json::writeString(wbuilder, outputsRoot);

				_logger->info(__FILEREF__ + "liveProxy. outputsRoot"
					+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", sOutputsRoot: " + sOutputsRoot
				);
			}

			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				string otherOutputOptions;
				string audioVolumeChange;
				Json::Value encodingProfileDetailsRoot = Json::nullValue;
				string manifestDirectoryPath;
				string manifestFileName;
				int segmentDurationInSeconds = -1;
				int playlistEntriesNumber = -1;
				bool isVideo = true;
				string rtmpUrl;

				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = outputRoot.get("outputType", "").asString();

				if (outputType == "HLS" || outputType == "DASH")
				{
					otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();
					audioVolumeChange = outputRoot.get("audioVolumeChange", "").asString();
					encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];
					manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "").asString();
					manifestFileName = outputRoot.get("manifestFileName", "").asString();
					segmentDurationInSeconds = JSONUtils::asInt(outputRoot, "segmentDurationInSeconds", 10);
					playlistEntriesNumber = JSONUtils::asInt(outputRoot, "playlistEntriesNumber", 5);

					string encodingProfileContentType = outputRoot.get("encodingProfileContentType", "Video").asString();

					isVideo = encodingProfileContentType == "Video" ? true : false;
				}
				else if (outputType == "RTMP_Stream")
				{
					otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();
					audioVolumeChange = outputRoot.get("audioVolumeChange", "").asString();
					encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];
					rtmpUrl = outputRoot.get("rtmpUrl", "").asString();

					string encodingProfileContentType = outputRoot.get("encodingProfileContentType", "Video").asString();
					isVideo = encodingProfileContentType == "Video" ? true : false;
				}
				else
				{
					string errorMessage = __FILEREF__ + "liveProxy. Wrong output type"
						+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", outputType: " + outputType;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				tuple<string, string, string, Json::Value, string, string, int, int, bool, string> tOutputRoot
					= make_tuple(outputType, otherOutputOptions, audioVolumeChange, encodingProfileDetailsRoot,
						manifestDirectoryPath, manifestFileName, segmentDurationInSeconds, playlistEntriesNumber,
						isVideo, rtmpUrl);

				liveProxy->_liveProxyOutputRoots.push_back(tOutputRoot);

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

		string userAgent = (liveProxy->_ingestedParametersRoot).get("UserAgent", "").asString();
		int maxWidth = JSONUtils::asInt(liveProxy->_ingestedParametersRoot, "MaxWidth", -1);
		string otherInputOptions = (liveProxy->_ingestedParametersRoot).get(
			"OtherInputOptions", "").asString();
		// string otherOutputOptions = liveProxyMetadata.get("otherOutputOptions", "").asString();
		// int segmentDurationInSeconds = JSONUtils::asInt(liveProxyMetadata, "segmentDurationInSeconds", 10);
		// int playlistEntriesNumber = JSONUtils::asInt(liveProxyMetadata, "playlistEntriesNumber", 6);
		liveProxy->_channelLabel = liveProxy->_ingestedParametersRoot.get("ConfigurationLabel", "").asString();
		// string manifestDirectoryPath = liveProxyMetadata.get("manifestDirectoryPath", "").asString();
		// string manifestFileName = liveProxyMetadata.get("manifestFileName", "").asString();

		liveProxy->_channelType = liveProxy->_ingestedParametersRoot.get("ChannelType", "IP_MMSAsClient").asString();
		int ipMMSAsServer_listenTimeoutInSeconds = liveProxy->
			_ingestedParametersRoot.get("ActAsServerListenTimeout", -1).asInt();
		int captureLive_videoDeviceNumber = -1;
		string captureLive_videoInputFormat;
		int captureLive_frameRate = -1;
		int captureLive_width = -1;
		int captureLive_height = -1;
		int captureLive_audioDeviceNumber = -1;
		int captureLive_channelsNumber = -1;
		if (liveProxy->_channelType == "CaptureLive")
		{
			Json::Value captureLiveRoot =
				(liveProxy->_ingestedParametersRoot)["CaptureLive"];

			captureLive_videoDeviceNumber = JSONUtils::asInt(captureLiveRoot, "VideoDeviceNumber", -1);
			captureLive_videoInputFormat = captureLiveRoot.get("VideoInputFormat", "").asString();
			captureLive_frameRate = JSONUtils::asInt(captureLiveRoot, "FrameRate", -1);
			captureLive_width = JSONUtils::asInt(captureLiveRoot, "Width", -1);
			captureLive_height = JSONUtils::asInt(captureLiveRoot, "Height", -1);
			captureLive_audioDeviceNumber = JSONUtils::asInt(captureLiveRoot, "AudioDeviceNumber", -1);
			captureLive_channelsNumber = JSONUtils::asInt(captureLiveRoot, "ChannelsNumber", -1);
		}

		time_t utcProxyPeriodStart = -1;
		time_t utcProxyPeriodEnd = -1;
		bool timePeriod = JSONUtils::asBool(liveProxyMetadata["encodingParametersRoot"], "timePeriod", false);
		if (timePeriod)
		{
			utcProxyPeriodStart = JSONUtils::asInt64(liveProxyMetadata["encodingParametersRoot"],
				"utcProxyPeriodStart", -1);
			utcProxyPeriodEnd = JSONUtils::asInt64(liveProxyMetadata["encodingParametersRoot"],
				"utcProxyPeriodEnd", -1);
		}

		string liveURL;
		if (liveProxy->_channelType == "Satellite")
		{
			satelliteServiceId = JSONUtils::asInt64(
				liveProxyMetadata["encodingParametersRoot"], "satelliteServiceId", -1);
			satelliteFrequency = JSONUtils::asInt64(
				liveProxyMetadata["encodingParametersRoot"], "satelliteFrequency", -1);
			satelliteSymbolRate = JSONUtils::asInt64(
				liveProxyMetadata["encodingParametersRoot"], "satelliteSymbolRate", -1);
			satelliteModulation = liveProxyMetadata["encodingParametersRoot"].
				get("satelliteModulation", "").asString();
			satelliteVideoPid = JSONUtils::asInt(
				liveProxyMetadata["encodingParametersRoot"], "satelliteVideoPid", -1);
			satelliteAudioItalianPid = JSONUtils::asInt(
				liveProxyMetadata["encodingParametersRoot"], "satelliteAudioItalianPid", -1);

			// In case ffmpeg crashes and is automatically restarted, it should use the same
			// IP-PORT it was using before because we already have a dbvlast sending the stream
			// to the specified IP-PORT.
			// For this reason, before to generate a new IP-PORT, let's look for the serviceId
			// inside the dvblast conf. file to see if it was already running before

			pair<string, string> satelliteMulticast = getSatelliteMulticastFromDvblastConfigurationFile(
				liveProxy->_ingestionJobKey, encodingJobKey,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation);
			tie(satelliteMulticastIP, satelliteMulticastPort) = satelliteMulticast;

			if (satelliteMulticastIP == "")
			{
				lock_guard<mutex> locker(*_satelliteChannelsPortsMutex);

				satelliteMulticastIP = "239.255.1.1";
				satelliteMulticastPort = to_string(*_satelliteChannelPort_CurrentOffset
					+ _satelliteChannelPort_Start);

				*_satelliteChannelPort_CurrentOffset = (*_satelliteChannelPort_CurrentOffset + 1)
					% _satelliteChannelPort_MaxNumberOfOffsets;
			}

			liveURL = string("udp://@") + satelliteMulticastIP + ":" + satelliteMulticastPort;

			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveProxy->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				true);
		}
		else
		{
			// in case of actAsServer
			//	true: it is built into the MMSEngineProcessor::manageLiveProxy method
			//	false: it comes from the LiveProxy json ingested
			liveURL = liveProxyMetadata.get("liveURL", "").asString();
		}

		/*
		Json::Value encodingProfileDetailsRoot = Json::nullValue;
        MMSEngineDBFacade::ContentType contentType;
		int64_t encodingProfileKey;
        if (JSONUtils::isMetadataPresent(liveProxyMetadata, "encodingProfileDetails"))
		{
			encodingProfileDetailsRoot = liveProxyMetadata["encodingProfileDetails"];
			contentType = MMSEngineDBFacade::toContentType(liveProxyMetadata.get("contentType", "").asString());
			encodingProfileKey = JSONUtils::asInt64(liveProxyMetadata, "encofingProfileKey", -1);
		}
		*/

		{
			// based on liveProxy->_proxyStart, the monitor thread starts the checkings
			// In case of IP_MMSAsServer, the checks should be done after the ffmpeg server
			// receives the stream and we do not know what it happens.
			// For this reason, in this scenario, we have to set _proxyStart in the worst scenario
			if (liveProxy->_channelType == "IP_MMSAsServer")
			{
				if (utcProxyPeriodStart != -1)
				{
					if (chrono::system_clock::from_time_t(utcProxyPeriodStart) < chrono::system_clock::now())
						liveProxy->_proxyStart = chrono::system_clock::now() +
							chrono::seconds(ipMMSAsServer_listenTimeoutInSeconds);
					else
						liveProxy->_proxyStart = chrono::system_clock::from_time_t(utcProxyPeriodStart) +
							chrono::seconds(ipMMSAsServer_listenTimeoutInSeconds);
				}
				else
					liveProxy->_proxyStart = chrono::system_clock::now() +
						chrono::seconds(ipMMSAsServer_listenTimeoutInSeconds);
			}
			else
			{
				if (utcProxyPeriodStart != -1)
				{
					if (chrono::system_clock::from_time_t(utcProxyPeriodStart) < chrono::system_clock::now())
						liveProxy->_proxyStart = chrono::system_clock::now();
					else
						liveProxy->_proxyStart = chrono::system_clock::from_time_t(utcProxyPeriodStart);
				}
				else
					liveProxy->_proxyStart = chrono::system_clock::now();
			}

			liveProxy->_ffmpeg->liveProxy(
				liveProxy->_ingestionJobKey,
				encodingJobKey,
				maxWidth,
				liveProxy->_channelType,
				StringUtils::trimTabToo(liveURL),
				ipMMSAsServer_listenTimeoutInSeconds,
				captureLive_videoDeviceNumber,
				captureLive_videoInputFormat,
				captureLive_frameRate,
				captureLive_width,
				captureLive_height,
				captureLive_audioDeviceNumber,
				captureLive_channelsNumber,
				userAgent,
				otherInputOptions,
				timePeriod, utcProxyPeriodStart, utcProxyPeriodEnd,
				liveProxy->_liveProxyOutputRoots,
				&(liveProxy->_childPid));
		}

		if (liveProxy->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveProxy->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
        liveProxy->_childPid = 0;
		liveProxy->_killedBecauseOfNotWorking = false;
        
        _logger->info(__FILEREF__ + "_ffmpeg->liveProxyByHTTPStreaming finished"
			+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", liveProxy->_channelLabel: " + liveProxy->_channelLabel
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		liveProxy->_ingestionJobKey = 0;
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();

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
		if (liveProxy->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveProxy->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();

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
		if (liveProxy->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveProxy->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();
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
		if (liveProxy->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveProxy->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();
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
		if (liveProxy->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveProxy->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();
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
		if (liveProxy->_channelType == "Satellite"
			&& satelliteServiceId != -1	// this is just to be sure variables are initialized
		)
		{
			// remove configuration from dvblast configuration file
			createOrUpdateSatelliteDvbLastConfigurationFile(
				liveProxy->_ingestionJobKey, encodingJobKey,
				satelliteMulticastIP, satelliteMulticastPort,
				satelliteServiceId, satelliteFrequency, satelliteSymbolRate,
				satelliteModulation, satelliteVideoPid, satelliteAudioItalianPid,
				false);
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();
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

void FFMPEGEncoder::vodProxyThread(
	// FCGX_Request& request,
	shared_ptr<LiveProxyAndGrid> vodProxy,
	int64_t encodingJobKey,
	string requestBody)
{
    string api = "vodProxy";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		vodProxy->_killedBecauseOfNotWorking = false;
		vodProxy->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value vodProxyMetadata;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &vodProxyMetadata, &errors);
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

		vodProxy->_ingestionJobKey = JSONUtils::asInt64(vodProxyMetadata, "ingestionJobKey", -1);
		string contentType = (vodProxyMetadata["encodingParametersRoot"])
			.get("contentType", "").asString();
		string sourcePhysicalPathName = (vodProxyMetadata["encodingParametersRoot"])
			.get("sourcePhysicalPathName", "").asString();

		vodProxy->_liveProxyOutputRoots.clear();
		{
			Json::Value outputsRoot = vodProxyMetadata["encodingParametersRoot"]["outputsRoot"];

			{
				Json::StreamWriterBuilder wbuilder;
				string sOutputsRoot = Json::writeString(wbuilder, outputsRoot);

				_logger->info(__FILEREF__ + "vodProxy. outputsRoot"
					+ ", ingestionJobKey: " + to_string(vodProxy->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", sOutputsRoot: " + sOutputsRoot
				);
			}

			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				string otherOutputOptions;
				string audioVolumeChange;
				Json::Value encodingProfileDetailsRoot = Json::nullValue;
				string manifestDirectoryPath;
				string manifestFileName;
				int segmentDurationInSeconds = -1;
				int playlistEntriesNumber = -1;
				bool isVideo = true;
				string rtmpUrl;

				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = outputRoot.get("outputType", "").asString();

				if (outputType == "HLS" || outputType == "DASH")
				{
					otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();
					audioVolumeChange = outputRoot.get("audioVolumeChange", "").asString();
					encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];
					manifestDirectoryPath = outputRoot.get("manifestDirectoryPath", "").asString();
					manifestFileName = outputRoot.get("manifestFileName", "").asString();
					segmentDurationInSeconds = JSONUtils::asInt(outputRoot, "segmentDurationInSeconds", 10);
					playlistEntriesNumber = JSONUtils::asInt(outputRoot, "playlistEntriesNumber", 5);

					string encodingProfileContentType = outputRoot.get("encodingProfileContentType", "Video").asString();

					isVideo = encodingProfileContentType == "Video" ? true : false;
				}
				else if (outputType == "RTMP_Stream")
				{
					otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();
					audioVolumeChange = outputRoot.get("audioVolumeChange", "").asString();
					encodingProfileDetailsRoot = outputRoot["encodingProfileDetails"];
					rtmpUrl = outputRoot.get("rtmpUrl", "").asString();

					string encodingProfileContentType = outputRoot.get("encodingProfileContentType", "Video").asString();
					isVideo = encodingProfileContentType == "Video" ? true : false;
				}
				else
				{
					string errorMessage = __FILEREF__ + "vodProxy. Wrong output type"
						+ ", ingestionJobKey: " + to_string(vodProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", outputType: " + outputType;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				tuple<string, string, string, Json::Value, string, string, int, int, bool, string> tOutputRoot
					= make_tuple(outputType, otherOutputOptions, audioVolumeChange, encodingProfileDetailsRoot,
						manifestDirectoryPath, manifestFileName, segmentDurationInSeconds, playlistEntriesNumber,
						isVideo, rtmpUrl);

				vodProxy->_liveProxyOutputRoots.push_back(tOutputRoot);

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
							FileIO::removeDirectory(manifestDirectoryPath, bRemoveRecursively);
						}
						catch(runtime_error e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed"
								+ ", ingestionJobKey: " + to_string(vodProxy->_ingestionJobKey)
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
								+ ", ingestionJobKey: " + to_string(vodProxy->_ingestionJobKey)
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

		vodProxy->_ingestedParametersRoot = vodProxyMetadata["ingestedParametersRoot"];

		string otherInputOptions = (vodProxy->_ingestedParametersRoot).get(
			"OtherInputOptions", "").asString();

		time_t utcProxyPeriodStart = -1;
		time_t utcProxyPeriodEnd = -1;
		bool timePeriod = JSONUtils::asBool(vodProxyMetadata["encodingParametersRoot"],
			"timePeriod", false);
		if (timePeriod)
		{
			utcProxyPeriodStart = JSONUtils::asInt64(vodProxyMetadata["encodingParametersRoot"],
				"utcProxyPeriodStart", -1);
			utcProxyPeriodEnd = JSONUtils::asInt64(vodProxyMetadata["encodingParametersRoot"],
				"utcProxyPeriodEnd", -1);
		}

		{
			// based on vodProxy->_proxyStart, the monitor thread starts the checkings
			if (utcProxyPeriodStart != -1)
			{
				if (chrono::system_clock::from_time_t(utcProxyPeriodStart) <
						chrono::system_clock::now())
					vodProxy->_proxyStart = chrono::system_clock::now();
				else
					vodProxy->_proxyStart = chrono::system_clock::from_time_t(
						utcProxyPeriodStart);
			}
			else
				vodProxy->_proxyStart = chrono::system_clock::now();

			vodProxy->_ffmpeg->vodProxy(
				vodProxy->_ingestionJobKey,
				encodingJobKey,

				contentType,
				sourcePhysicalPathName,

				otherInputOptions,
				timePeriod, utcProxyPeriodStart, utcProxyPeriodEnd,
				vodProxy->_liveProxyOutputRoots,
				&(vodProxy->_childPid));
		}

        vodProxy->_running = false;
		vodProxy->_method = "";
        vodProxy->_childPid = 0;
		vodProxy->_killedBecauseOfNotWorking = false;
        
        _logger->info(__FILEREF__ + "_ffmpeg->vodProxy finished"
			+ ", ingestionJobKey: " + to_string(vodProxy->_ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", vodProxy->_channelLabel: " + vodProxy->_channelLabel
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, vodProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		vodProxy->_ingestionJobKey = 0;
		vodProxy->_channelLabel = "";
		vodProxy->_liveProxyOutputRoots.clear();

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(vodProxy->_encodingJobKey);
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
        vodProxy->_running = false;
		vodProxy->_method = "";
		vodProxy->_ingestionJobKey = 0;
        vodProxy->_childPid = 0;
		vodProxy->_channelLabel = "";
		vodProxy->_liveProxyOutputRoots.clear();

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
		if (vodProxy->_killedBecauseOfNotWorking)
		{
			// it was killed just because it was not working and not because of user
			// In this case the process has to be restarted soon
			killedByUser				= false;
			completedWithError			= true;
			vodProxy->_killedBecauseOfNotWorking = false;
		}
		else
		{
			killedByUser				= true;
		}
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(vodProxy->_encodingJobKey,
				completedWithError, vodProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(vodProxy->_encodingJobKey);
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
    catch(runtime_error e)
    {
        vodProxy->_running = false;
		vodProxy->_method = "";
		vodProxy->_ingestionJobKey = 0;
        vodProxy->_childPid = 0;
		vodProxy->_channelLabel = "";
		vodProxy->_liveProxyOutputRoots.clear();
		vodProxy->_killedBecauseOfNotWorking = false;

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

		vodProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, vodProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(vodProxy->_encodingJobKey);
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
        vodProxy->_running = false;
		vodProxy->_method = "";
		vodProxy->_ingestionJobKey = 0;
        vodProxy->_childPid = 0;
		vodProxy->_channelLabel = "";
		vodProxy->_liveProxyOutputRoots.clear();
		vodProxy->_killedBecauseOfNotWorking = false;

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

		vodProxy->_errorMessage	= errorMessage;

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, vodProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		#ifdef __VECTOR__
		#else	// __MAP__
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int erase = _liveProxiesCapability->erase(vodProxy->_encodingJobKey);
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

void FFMPEGEncoder::awaitingTheBeginningThread(
	// FCGX_Request& request,
	shared_ptr<LiveProxyAndGrid> liveProxy,
	int64_t encodingJobKey,
	string requestBody)
{
    string api = "awaitingTheBeginning";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", encodingJobKey: " + to_string(encodingJobKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		liveProxy->_killedBecauseOfNotWorking = false;
		liveProxy->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        Json::Value awaitingTheBeginningMetadata;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &awaitingTheBeginningMetadata, &errors);
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

		liveProxy->_ingestionJobKey = JSONUtils::asInt64(
			awaitingTheBeginningMetadata, "ingestionJobKey", -1);

		string outputType;
		// otherOutputOptions is not used for awaitingTheBeginning
		string otherOutputOptions;
		string audioVolumeChange;
		Json::Value encodingProfileDetailsRoot = Json::nullValue;
		string manifestDirectoryPath;
		string manifestFileName;
		int segmentDurationInSeconds = -1;
		int playlistEntriesNumber = -1;
		bool isVideo = true;
		string rtmpUrl;
		{
			outputType = awaitingTheBeginningMetadata.get("outputType", "").asString();

			if (outputType == "HLS" || outputType == "DASH")
			{
				// otherOutputOptions is not used by awaitingTheBeginning
				// otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();
				encodingProfileDetailsRoot = awaitingTheBeginningMetadata["encodingProfileDetails"];
				manifestDirectoryPath = awaitingTheBeginningMetadata.get("manifestDirectoryPath", "").asString();
				manifestFileName = awaitingTheBeginningMetadata.get("manifestFileName", "").asString();
				segmentDurationInSeconds = JSONUtils::asInt(awaitingTheBeginningMetadata, "segmentDurationInSeconds", 10);
				playlistEntriesNumber = JSONUtils::asInt(awaitingTheBeginningMetadata, "playlistEntriesNumber", 5);

				string encodingProfileContentType = awaitingTheBeginningMetadata.get("encodingProfileContentType", "Video").asString();

				isVideo = encodingProfileContentType == "Video" ? true : false;
			}
			else if (outputType == "RTMP_Stream")
			{
				// otherOutputOptions is not used by awaitingTheBeginning
				// otherOutputOptions = outputRoot.get("otherOutputOptions", "").asString();
				encodingProfileDetailsRoot = awaitingTheBeginningMetadata["encodingProfileDetails"];
				rtmpUrl = awaitingTheBeginningMetadata.get("rtmpUrl", "").asString();

				string encodingProfileContentType = awaitingTheBeginningMetadata.get("encodingProfileContentType", "Video").asString();
				isVideo = encodingProfileContentType == "Video" ? true : false;
			}
			else
			{
				string errorMessage = __FILEREF__ + "awaitingTheBegining. Wrong output type"
					+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", outputType: " + outputType;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		// we will fill liveProxy->_liveProxyOutputRoots just because it is needed to the monitor thread
		{
			liveProxy->_liveProxyOutputRoots.clear();

			tuple<string, string, string, Json::Value, string, string, int, int, bool, string>
				tOutputRoot
				= make_tuple(outputType, otherOutputOptions, audioVolumeChange,
					encodingProfileDetailsRoot, manifestDirectoryPath,
					manifestFileName, segmentDurationInSeconds, playlistEntriesNumber, isVideo, rtmpUrl);

			liveProxy->_liveProxyOutputRoots.push_back(tOutputRoot);
		}

        string mmsSourceVideoAssetPathName = awaitingTheBeginningMetadata.get("mmsSourceVideoAssetPathName", "").asString();
        int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(awaitingTheBeginningMetadata, "videoDurationInMilliSeconds", -1);
		liveProxy->_channelLabel = "";	// awaitingTheBeginningMetadata.get("configurationLabel", "").asString();

		liveProxy->_ingestedParametersRoot = awaitingTheBeginningMetadata["awaitingTheBeginningIngestedParametersRoot"];

		liveProxy->_channelType = ""; // liveProxy->_ingestedParametersRoot.get("ChannelType", "IP_MMSAsClient").asString();

		time_t utcCountDownEnd = JSONUtils::asInt64(awaitingTheBeginningMetadata, "utcCountDownEnd", -1);

		{
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

		string text;
		string textPosition_X_InPixel;
		string textPosition_Y_InPixel;
		string fontType;
		int fontSize = -1;
		string fontColor;
		int textPercentageOpacity = -1;
		bool boxEnable = false;
		string boxColor;
		int boxPercentageOpacity = -1;
		{
			string field = "Text";
			text = liveProxy->_ingestedParametersRoot.get(field, "").asString();

			field = "TextPosition_X_InPixel";
			if (JSONUtils::isMetadataPresent(liveProxy->_ingestedParametersRoot, field))
				textPosition_X_InPixel = liveProxy->_ingestedParametersRoot.get(field, "").asString();

			field = "TextPosition_Y_InPixel";
			if (JSONUtils::isMetadataPresent(liveProxy->_ingestedParametersRoot, field))
				textPosition_Y_InPixel = liveProxy->_ingestedParametersRoot.get(field, "").asString();

			field = "FontType";
			if (JSONUtils::isMetadataPresent(liveProxy->_ingestedParametersRoot, field))
				fontType = liveProxy->_ingestedParametersRoot.get(field, "").asString();

			field = "FontSize";
			if (JSONUtils::isMetadataPresent(liveProxy->_ingestedParametersRoot, field))
				fontSize = JSONUtils::asInt(liveProxy->_ingestedParametersRoot, field, -1);

			field = "FontColor";
			if (JSONUtils::isMetadataPresent(liveProxy->_ingestedParametersRoot, field))
				fontColor = liveProxy->_ingestedParametersRoot.get(field, "").asString();

			field = "TextPercentageOpacity";
			if (JSONUtils::isMetadataPresent(liveProxy->_ingestedParametersRoot, field))
				textPercentageOpacity = JSONUtils::asInt64(liveProxy->_ingestedParametersRoot, field, -1);

			field = "BoxEnable";
			if (JSONUtils::isMetadataPresent(liveProxy->_ingestedParametersRoot, field))
				boxEnable = JSONUtils::asBool(liveProxy->_ingestedParametersRoot, field, false);

			field = "BoxColor";
			if (JSONUtils::isMetadataPresent(liveProxy->_ingestedParametersRoot, field))
				boxColor = liveProxy->_ingestedParametersRoot.get(field, "").asString();

			field = "BoxPercentageOpacity";
			if (JSONUtils::isMetadataPresent(liveProxy->_ingestedParametersRoot, field))
				boxPercentageOpacity = JSONUtils::asInt64(liveProxy->_ingestedParametersRoot, field, -1);
		}

		{
			liveProxy->_proxyStart = chrono::system_clock::now();

			liveProxy->_ffmpeg->awaitingTheBegining(
				liveProxy->_ingestionJobKey,
				encodingJobKey,

				mmsSourceVideoAssetPathName,
				videoDurationInMilliSeconds,

				utcCountDownEnd,

				text,
				textPosition_X_InPixel,
				textPosition_Y_InPixel,
				fontType,
				fontSize,
				fontColor,
				textPercentageOpacity,
				boxEnable,
				boxColor,
				boxPercentageOpacity,

				outputType,
				encodingProfileDetailsRoot,
				manifestDirectoryPath,
				manifestFileName,
				segmentDurationInSeconds,
				playlistEntriesNumber,
				isVideo,
				rtmpUrl,

				&(liveProxy->_childPid));
		}

        liveProxy->_running = false;
		liveProxy->_method = "";
        liveProxy->_childPid = 0;
		liveProxy->_killedBecauseOfNotWorking = false;
        
        _logger->info(__FILEREF__ + "_ffmpeg->awaitingTheBegining finished"
			+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveProxy->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		liveProxy->_ingestionJobKey = 0;
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();

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
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();

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
    catch(runtime_error e)
    {
        liveProxy->_running = false;
		liveProxy->_method = "";
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();
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
		liveProxy->_channelLabel = "";
		liveProxy->_liveProxyOutputRoots.clear();
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
		liveProxy->_channelLabel = manifestFileName;

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
            + ", liveProxy->_channelLabel: " + liveProxy->_channelLabel
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
		liveProxy->_channelLabel = "";

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
		liveProxy->_channelLabel = "";

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
		liveProxy->_channelLabel = "";
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
		liveProxy->_channelLabel = "";
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
		liveProxy->_channelLabel = "";
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
		liveProxy->_channelLabel = "";
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
		try
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			int liveProxyAndGridRunningCounter = 0;
			int liveProxyAndGridNotRunningCounter = 0;
			#ifdef __VECTOR__
			for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			#else	// __MAP__
			for(map<int64_t, shared_ptr<LiveProxyAndGrid>>::iterator it = _liveProxiesCapability->begin();
				it != _liveProxiesCapability->end(); it++)
			#endif
			{
				#ifdef __VECTOR__
				#else	// __MAP__
				shared_ptr<LiveProxyAndGrid> liveProxy = it->second;
				#endif

				if (liveProxy->_running)
				{
					liveProxyAndGridRunningCounter++;

					_logger->info(__FILEREF__ + "liveProxyMonitor..."
						+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
					);

					chrono::system_clock::time_point now = chrono::system_clock::now();

					bool liveProxyWorking = true;
					string localErrorMessage;

					{
						// liveProxy->_proxyStart could be a bit in the future
						int64_t liveProxyLiveTimeInMinutes;
						if (now > liveProxy->_proxyStart)
							liveProxyLiveTimeInMinutes	=
								chrono::duration_cast<chrono::minutes>(now - liveProxy->_proxyStart).count();
						else
							liveProxyLiveTimeInMinutes	= 0;

						// checks are done after 3 minutes LiveProxy started, in order to be sure
						// the manifest file was already created
						if (liveProxyLiveTimeInMinutes <= 3)
						{
							_logger->info(__FILEREF__ + "Checks are not done because too early"
								+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
							);

							continue;
						}
					}

					// First health check
					//		HLS/DASH:	kill if manifest file does not exist or was not updated in the last 30 seconds
					//		rtmp(Proxy)/SRT(Grid):	kill if it was found 'Non-monotonous DTS in output stream' and 'incorrect timestamps'
					bool rtmpOutputFound = false;
					if (liveProxyWorking)
					{
						for (tuple<string, string, string, Json::Value, string, string, int, int, bool, string> outputRoot:
							liveProxy->_liveProxyOutputRoots)
						{
							string outputType;
							string otherOutputOptions;
							string audioVolumeChange;
							Json::Value encodingProfileDetailsRoot;
							string manifestDirectoryPath;
							string manifestFileName;
							int segmentDurationInSeconds;
							int playlistEntriesNumber;
							bool isVideo;
							string rtmpUrl;

							tie(outputType, otherOutputOptions, audioVolumeChange,
								encodingProfileDetailsRoot, manifestDirectoryPath,       
								manifestFileName, segmentDurationInSeconds,
								playlistEntriesNumber, isVideo, rtmpUrl)
								= outputRoot;

							if (!liveProxyWorking)
								break;

							if (outputType == "HLS" || outputType == "DASH")
							{
								try
								{
									// First health check (HLS/DASH) looking the manifests path name timestamp

									/*
									chrono::system_clock::time_point now = chrono::system_clock::now();
									int64_t liveProxyLiveTimeInMinutes =
										chrono::duration_cast<chrono::minutes>(now - liveProxy->_proxyStart).count();

									// check is done after 3 minutes LiveProxy started, in order to be sure
									// the manifest file was already created
									if (liveProxyLiveTimeInMinutes > 3)
									*/
									{
										string manifestFilePathName =
											manifestDirectoryPath + "/" + manifestFileName;
										{
											if(!FileIO::fileExisting(manifestFilePathName))
											{
												liveProxyWorking = false;

												_logger->error(__FILEREF__ + "liveProxyMonitor. Manifest file does not exist"
													+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
													+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
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

												long maxLastManifestFileUpdateInSeconds = 30;

												unsigned long long lastManifestFileUpdateInSeconds = ullNow - utcManifestFileLastModificationTime;
												if (lastManifestFileUpdateInSeconds > maxLastManifestFileUpdateInSeconds)
												{
													liveProxyWorking = false;

													_logger->error(__FILEREF__ + "liveProxyMonitor. Manifest file was not updated "
														+ "in the last " + to_string(maxLastManifestFileUpdateInSeconds) + " seconds"
														+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
														+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
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
									string errorMessage = string ("liveProxyMonitorCheck (HLS) on manifest path name failed")
										+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
										+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;

									_logger->error(__FILEREF__ + errorMessage);
								}
								catch(exception e)
								{
									string errorMessage = string ("liveProxyMonitorCheck (HLS) on manifest path name failed")
										+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
										+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
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
					if (liveProxyWorking && rtmpOutputFound)
					{
						try
						{
							// First health check (rtmp), looks the log and check there is no message like
							//	[flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to 95383372. This may result in incorrect timestamps in the output file.
							//	This message causes proxy not working
							if (liveProxy->_ffmpeg->nonMonotonousDTSInOutputLog())
							{
								liveProxyWorking = false;

								_logger->error(__FILEREF__ + "liveProxyMonitor (rtmp). Live Proxy is logging 'Non-monotonous DTS in output stream/incorrect timestamps'. LiveProxy (ffmpeg) is killed in order to be started again"
									+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
									+ ", channelLabel: " + liveProxy->_channelLabel
									+ ", liveProxy->_childPid: " + to_string(liveProxy->_childPid)
								);

								localErrorMessage = " restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps'";
							}
						}
						catch(runtime_error e)
						{
							string errorMessage = string ("liveProxyMonitorCheck (rtmp) Non-monotonous DTS failed")
								+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;

							_logger->error(__FILEREF__ + errorMessage);
						}
						catch(exception e)
						{
							string errorMessage = string ("liveProxyMonitorCheck (rtmp) Non-monotonous DTS failed")
								+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;

							_logger->error(__FILEREF__ + errorMessage);
						}
					}


					// Second health 
					//		HLS/DASH:	kill if segments were not generated
					//					frame increasing check
					//					it is also implemented the retention of segments too old (10 minutes)
					//						This is already implemented by the HLS parameters (into the ffmpeg command)
					//						We do it for the DASH option and in case ffmpeg does not work
					//		rtmp(Proxy)/SRT(Grid):		frame increasing check
					rtmpOutputFound = false;
					if (liveProxyWorking)
					{
						for (tuple<string, string, string, Json::Value, string, string, int, int, bool, string> outputRoot:
							liveProxy->_liveProxyOutputRoots)
						{
							string outputType;
							string otherOutputOptions;
							string audioVolumeChange;
							Json::Value encodingProfileDetailsRoot;
							string manifestDirectoryPath;
							string manifestFileName;
							int segmentDurationInSeconds;
							int playlistEntriesNumber;
							bool isVideo;
							string rtmpUrl;

							tie(outputType, otherOutputOptions, audioVolumeChange,
								encodingProfileDetailsRoot, manifestDirectoryPath,       
								manifestFileName, segmentDurationInSeconds,
								playlistEntriesNumber, isVideo, rtmpUrl)
								= outputRoot;

							if (!liveProxyWorking)
								break;

							if (outputType == "HLS" || outputType == "DASH")
							{
								try
								{
									/*
									chrono::system_clock::time_point now = chrono::system_clock::now();
									int64_t liveProxyLiveTimeInMinutes =
										chrono::duration_cast<chrono::minutes>(now - liveProxy->_proxyStart).count();

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
													string errorMessage = __FILEREF__ + "No manifestDirectoryPath find in the m3u8/mpd file path name"
														+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
														+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
														+ ", manifestFilePathName: " + manifestFilePathName;
													_logger->error(errorMessage);

													throw runtime_error(errorMessage);
												}
												manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
											}

											chrono::system_clock::time_point lastChunkTimestamp = liveProxy->_proxyStart;
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
															string errorMessage = __FILEREF__ + "listing directory failed"
																+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
																+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
																+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
																+ ", e.what(): " + e.what()
															;
															_logger->error(errorMessage);

															// throw e;
														}
														catch(exception e)
														{
															string errorMessage = __FILEREF__ + "listing directory failed"
																+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
																+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
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
												_logger->error(__FILEREF__ + "scan LiveProxy files failed"
													+ ", _ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
													+ ", _encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
													+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
													+ ", e.what(): " + e.what()
												);
											}
											catch(...)
											{
												_logger->error(__FILEREF__ + "scan LiveProxy files failed"
													+ ", _ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
													+ ", _encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
													+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
												);
											}
					
											if (!firstChunkRead
												|| lastChunkTimestamp < chrono::system_clock::now() - chrono::minutes(1))
											{
												// if we are here, it means the ffmpeg command is not generating the ts files

												_logger->error(__FILEREF__ + "liveProxyMonitor. Chunks were not generated"
													+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
													+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
													+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
													+ ", firstChunkRead: " + to_string(firstChunkRead)
												);

												chunksWereNotGenerated = true;

												liveProxyWorking = false;
												localErrorMessage = " restarted because of 'no segments were generated'";

												_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveProxyMonitor. Live Proxy is not working (no segments were generated). LiveProxy (ffmpeg) is killed in order to be started again"
													+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
													+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
													+ ", manifestFilePathName: " + manifestFilePathName
													+ ", channelLabel: " + liveProxy->_channelLabel
													+ ", liveProxy->_childPid: " + to_string(liveProxy->_childPid)
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
															+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
															+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
															+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
														FileIO::remove(segmentPathNameToBeRemoved, exceptionInCaseOfError);
													}
													catch(runtime_error e)
													{
														_logger->error(__FILEREF__ + "remove failed"
															+ ", _ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
															+ ", _encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
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
									string errorMessage = string ("liveProxyMonitorCheck (HLS) on segments (and retention) failed")
										+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
										+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;

									_logger->error(__FILEREF__ + errorMessage);
								}
								catch(exception e)
								{
									string errorMessage = string ("liveProxyMonitorCheck (HLS) on segments (and retention) failed")
										+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
										+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;

									_logger->error(__FILEREF__ + errorMessage);
								}

								if (!liveProxyWorking)
									continue;

								try
								{
									// Second health check (HLS/DASH), looks if the frame is increasing
									// awaitingTheBeginning skips this check because
									// often takes much time to update the frames
									int secondsToWaitBetweenSamples = 3;
									if (liveProxy->_method != "awaitingTheBeginning"
											&& !liveProxy->_ffmpeg->isFrameIncreasing(secondsToWaitBetweenSamples))
									{
										_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveProxyMonitor (HLS/DASH). Live Proxy frame is not increasing'. LiveProxy (ffmpeg) is killed in order to be started again"
											+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
											+ ", channelLabel: " + liveProxy->_channelLabel
											+ ", liveProxy->_childPid: " + to_string(liveProxy->_childPid)
										);

										liveProxyWorking = false;
										localErrorMessage = " restarted because of 'frame is not increasing'";
									}
								}
								catch(FFMpegEncodingStatusNotAvailable e)
								{
									string errorMessage = string ("liveProxyMonitorCheck (HLS/DASH) frame increasing check failed")
										+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
										+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;
									_logger->warn(__FILEREF__ + errorMessage);
								}
								catch(runtime_error e)
								{
									string errorMessage = string ("liveProxyMonitorCheck (HLS/DASH) frame increasing check failed")
										+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
										+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;
									_logger->error(__FILEREF__ + errorMessage);
								}
								catch(exception e)
								{
									string errorMessage = string ("liveProxyMonitorCheck (HLS/DASH) frame increasing check failed")
										+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
										+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;
									_logger->error(__FILEREF__ + errorMessage);
								}
							}
							else
							{
								rtmpOutputFound = true;
							}
						}
					}
					if (liveProxyWorking && rtmpOutputFound)
					{
						try
						{
							// Second health check, rtmp(Proxy)/SRT(Grid), looks if the frame is increasing
							int secondsToWaitBetweenSamples = 3;
							if (!liveProxy->_ffmpeg->isFrameIncreasing(secondsToWaitBetweenSamples))
							{
								_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveProxyMonitor (rtmp). Live Proxy frame is not increasing'. LiveProxy (ffmpeg) is killed in order to be started again"
									+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
									+ ", channelLabel: " + liveProxy->_channelLabel
									+ ", liveProxy->_childPid: " + to_string(liveProxy->_childPid)
								);

								liveProxyWorking = false;

								localErrorMessage = " restarted because of 'frame is not increasing'";
							}
						}
						catch(FFMpegEncodingStatusNotAvailable e)
						{
							string errorMessage = string ("liveProxyMonitorCheck (rtmp) frame increasing check failed")
								+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;
							_logger->warn(__FILEREF__ + errorMessage);
						}
						catch(runtime_error e)
						{
							string errorMessage = string ("liveProxyMonitorCheck (rtmp) frame increasing check failed")
								+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;
							_logger->error(__FILEREF__ + errorMessage);
						}
						catch(exception e)
						{
							string errorMessage = string ("liveProxyMonitorCheck (rtmp) frame increasing check failed")
								+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;
							_logger->error(__FILEREF__ + errorMessage);
						}
					}

					// Third health 
					//		HLS/DASH:	
					//		rtmp(Proxy)/SRT(Grid):	the ffmpeg is up and running, it is not working and,
					//			looking in the output log file, we have:
					//			[https @ 0x555a8e428a00] HTTP error 403 Forbidden
					rtmpOutputFound = false;
					if (liveProxyWorking)
					{
						for (tuple<string, string, string, Json::Value, string, string, int, int, bool, string> outputRoot:
							liveProxy->_liveProxyOutputRoots)
						{
							string outputType;
							string otherOutputOptions;
							string audioVolumeChange;
							Json::Value encodingProfileDetailsRoot;
							string manifestDirectoryPath;
							string manifestFileName;
							int segmentDurationInSeconds;
							int playlistEntriesNumber;
							bool isVideo;
							string rtmpUrl;

							tie(outputType, otherOutputOptions, audioVolumeChange,
								encodingProfileDetailsRoot, manifestDirectoryPath,       
								manifestFileName, segmentDurationInSeconds,
								playlistEntriesNumber, isVideo, rtmpUrl)
								= outputRoot;

							if (!liveProxyWorking)
								break;

							if (outputType == "HLS" || outputType == "DASH")
							{
							}
							else
							{
								rtmpOutputFound = true;
							}
						}
					}
					if (liveProxyWorking && rtmpOutputFound)
					{
						try
						{
							if (liveProxy->_ffmpeg->forbiddenErrorInOutputLog())
							{
								_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveProxyMonitor (rtmp). Live Proxy is returning 'HTTP error 403 Forbidden'. LiveProxy (ffmpeg) is killed in order to be started again"
									+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
									+ ", channelLabel: " + liveProxy->_channelLabel
									+ ", liveProxy->_childPid: " + to_string(liveProxy->_childPid)
								);

								liveProxyWorking = false;
								localErrorMessage = " restarted because of 'HTTP error 403 Forbidden'";
							}
						}
						catch(FFMpegEncodingStatusNotAvailable e)
						{
							string errorMessage = string ("liveProxyMonitorCheck (rtmp) HTTP error 403 Forbidden check failed")
								+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;
							_logger->warn(__FILEREF__ + errorMessage);
						}
						catch(runtime_error e)
						{
							string errorMessage = string ("liveProxyMonitorCheck (rtmp) HTTP error 403 Forbidden check failed")
								+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;
							_logger->error(__FILEREF__ + errorMessage);
						}
						catch(exception e)
						{
							string errorMessage = string ("liveProxyMonitorCheck (rtmp) HTTP error 403 Forbidden check failed")
								+ ", liveProxy->_ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", liveProxy->_encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;
							_logger->error(__FILEREF__ + errorMessage);
						}
					}

					if (!liveProxyWorking)
					{
						_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveProxyMonitor. LiveProxy (ffmpeg) is killed in order to be started again"
							+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
							+ ", localErrorMessage: " + localErrorMessage
							+ ", channelLabel: " + liveProxy->_channelLabel
							+ ", liveProxy->_childPid: " + to_string(liveProxy->_childPid)
						);

						try
						{
							ProcessUtility::killProcess(liveProxy->_childPid);
							liveProxy->_killedBecauseOfNotWorking = true;
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
								liveProxy->_errorMessage = string(strDateTime) + " "
									+ liveProxy->_channelLabel +
									localErrorMessage;
							}
						}
						catch(runtime_error e)
						{
							string errorMessage = string("ProcessUtility::killProcess failed")
								+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
								+ ", liveProxy->_childPid: " + to_string(liveProxy->_childPid)
								+ ", e.what(): " + e.what()
									;
							_logger->error(__FILEREF__ + errorMessage);
						}
					}

					_logger->info(__FILEREF__ + "liveProxyMonitorCheck"
						+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
						+ ", @MMS statistics@ - elapsed time: @" + to_string(
							chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - now).count()
						) + "@"
					);
				}
				else
				{
					liveProxyAndGridNotRunningCounter++;
				}
			}
			_logger->info(__FILEREF__ + "monitoringThread, LiveProxyAndGrid"
				+ ", total LiveProxyAndGrid: " + to_string(liveProxyAndGridRunningCounter + liveProxyAndGridNotRunningCounter)
				+ ", liveProxyAndGridRunningCounter: " + to_string(liveProxyAndGridRunningCounter)
				+ ", liveProxyAndGridNotRunningCounter: " + to_string(liveProxyAndGridNotRunningCounter)
			);
		}
		catch(runtime_error e)
		{
			string errorMessage = string ("monitor LiveProxy failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("monitor LiveProxy failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}

		try
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			int liveRecordingRunningCounter = 0;
			int liveRecordingNotRunningCounter = 0;
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
					liveRecordingRunningCounter++;

					_logger->info(__FILEREF__ + "liveRecordingMonitor..."
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
					);

					chrono::system_clock::time_point now = chrono::system_clock::now();

					bool liveRecorderWorking = true;
					string localErrorMessage;

					// liveRecording->_recordingStart could be a bit in the future
					int64_t liveRecordingLiveTimeInMinutes;
					if (now > liveRecording->_recordingStart)
						liveRecordingLiveTimeInMinutes = chrono::duration_cast<chrono::minutes>(
							now - liveRecording->_recordingStart).count();
					else
						liveRecordingLiveTimeInMinutes = 0;

					int segmentDurationInSeconds;
					string field = "segmentDurationInSeconds";
					segmentDurationInSeconds = JSONUtils::asInt(liveRecording->_encodingParametersRoot, field, 0);

					// check is done after 5 minutes + segmentDurationInSeconds LiveRecording started,
					// in order to be sure the file was already created
					if (liveRecordingLiveTimeInMinutes <= (segmentDurationInSeconds / 60) + 5)
					{
						_logger->info(__FILEREF__ + "Checks are not done because too early"
							+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
						);

						continue;
					}

					// First health check
					//		kill if 1840699_408620.liveRecorder.list file does not exist or was not updated in the last (2 * segment duration in secs) seconds
					if (liveRecorderWorking)
					{
						try
						{
							// looking the manifests path name timestamp

							string segmentListPathName = liveRecording->_transcoderStagingContentsPath
								+ liveRecording->_segmentListFileName;

							{
								if(!FileIO::fileExisting(segmentListPathName))
								{
									liveRecorderWorking = false;

									_logger->error(__FILEREF__ + "liveRecordingMonitor. Segment list file does not exist"
										+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
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
											+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
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
							string errorMessage = string ("liveRecordingMonitorCheck on path name failed")
								+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
								+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;

							_logger->error(__FILEREF__ + errorMessage);
						}
						catch(exception e)
						{
							string errorMessage = string ("liveRecordingMonitorCheck on path name failed")
								+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
								+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;

							_logger->error(__FILEREF__ + errorMessage);
						}
					}

					// Second health check
					//		HLS/DASH:	kill if manifest file does not exist or was not updated in the last 30 seconds
					//		rtmp(Proxy):	kill if it was found 'Non-monotonous DTS in output stream' and 'incorrect timestamps'
					//			This check has to be done just once (not for each outputRoot) in case we have at least one rtmp output
					bool rtmpOutputFound = false;
					if (liveRecorderWorking)
					{
						for (tuple<string, string, string, Json::Value, string, string, int, int, bool, string> outputRoot:
							liveRecording->_liveRecorderOutputRoots)
						{
							string outputType;
							string otherOutputOptions;
							string audioVolumeChange;
							Json::Value encodingProfileDetailsRoot;
							string manifestDirectoryPath;
							string manifestFileName;
							int segmentDurationInSeconds;
							int playlistEntriesNumber;
							bool isVideo;
							string rtmpUrl;

							tie(outputType, otherOutputOptions, audioVolumeChange,
								encodingProfileDetailsRoot, manifestDirectoryPath,       
								manifestFileName, segmentDurationInSeconds,
								playlistEntriesNumber, isVideo, rtmpUrl)
								= outputRoot;

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
												+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
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

											long maxLastManifestFileUpdateInSeconds = 30;

											unsigned long long lastManifestFileUpdateInSeconds = ullNow - utcManifestFileLastModificationTime;
											if (lastManifestFileUpdateInSeconds > maxLastManifestFileUpdateInSeconds)
											{
												liveRecorderWorking = false;

												_logger->error(__FILEREF__ + "liveRecorderMonitor. Manifest file was not updated "
													+ "in the last " + to_string(maxLastManifestFileUpdateInSeconds) + " seconds"
													+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
													+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
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
									string errorMessage = string ("liveRecorderMonitorCheck (HLS) on manifest path name failed")
										+ ", liveRecorder->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", liveRecorder->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;

									_logger->error(__FILEREF__ + errorMessage);
								}
								catch(exception e)
								{
									string errorMessage = string ("liveRecorderMonitorCheck (HLS) on manifest path name failed")
										+ ", liveRecorder->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", liveRecorder->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
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
					if (liveRecorderWorking && rtmpOutputFound)
					{
						try
						{
							// First health check (rtmp), looks the log and check there is no message like
							//	[flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to 95383372. This may result in incorrect timestamps in the output file.
							//	This message causes proxy not working
							if (liveRecording->_ffmpeg->nonMonotonousDTSInOutputLog())
							{
								liveRecorderWorking = false;

								_logger->error(__FILEREF__ + "liveRecorderMonitor (rtmp). Live Recorder is logging 'Non-monotonous DTS in output stream/incorrect timestamps'. LiveRecorder (ffmpeg) is killed in order to be started again"
									+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
									+ ", channelLabel: " + liveRecording->_channelLabel
									+ ", liveProxy->_childPid: " + to_string(liveRecording->_childPid)
								);

								localErrorMessage = " restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps'";
							}
						}
						catch(runtime_error e)
						{
							string errorMessage = string ("liveRecorderMonitorCheck (rtmp) Non-monotonous DTS failed")
								+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;

							_logger->error(__FILEREF__ + errorMessage);
						}
						catch(exception e)
						{
							string errorMessage = string ("liveRecorderMonitorCheck (rtmp) Non-monotonous DTS failed")
								+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;

							_logger->error(__FILEREF__ + errorMessage);
						}
					}

					// Thirth health 
					//		HLS/DASH:	kill if segments were not generated
					//					frame increasing check
					//					it is also implemented the retention of segments too old (10 minutes)
					//						This is already implemented by the HLS parameters (into the ffmpeg command)
					//						We do it for the DASH option and in case ffmpeg does not work
					//		rtmp(Proxy):		frame increasing check
					//			This check has to be done just once (not for each outputRoot) in case we have at least one rtmp output
					rtmpOutputFound = false;
					if (liveRecorderWorking)
					{
						for (tuple<string, string, string, Json::Value, string, string, int, int, bool, string> outputRoot:
							liveRecording->_liveRecorderOutputRoots)
						{
							string outputType;
							string otherOutputOptions;
							string audioVolumeChange;
							Json::Value encodingProfileDetailsRoot;
							string manifestDirectoryPath;
							string manifestFileName;
							int segmentDurationInSeconds;
							int playlistEntriesNumber;
							bool isVideo;
							string rtmpUrl;

							tie(outputType, otherOutputOptions, audioVolumeChange,
								encodingProfileDetailsRoot, manifestDirectoryPath,       
								manifestFileName, segmentDurationInSeconds,
								playlistEntriesNumber, isVideo, rtmpUrl)
								= outputRoot;

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
												string errorMessage = __FILEREF__ + "No manifestDirectoryPath find in the m3u8/mpd file path name"
													+ ", liveRecorder->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
													+ ", liveRecorder->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
													+ ", manifestFilePathName: " + manifestFilePathName;
												_logger->error(errorMessage);

												throw runtime_error(errorMessage);
											}
											manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
										}

										chrono::system_clock::time_point lastChunkTimestamp = liveRecording->_recordingStart;
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
														string errorMessage = __FILEREF__ + "listing directory failed"
															+ ", liveRecorder->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
															+ ", liveRecorder->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
															+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
															+ ", e.what(): " + e.what()
														;
														_logger->error(errorMessage);

														// throw e;
													}
													catch(exception e)
													{
														string errorMessage = __FILEREF__ + "listing directory failed"
															+ ", liveRecorder->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
															+ ", liveRecorder->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
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
											_logger->error(__FILEREF__ + "scan LiveRecorder files failed"
												+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
												+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
												+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
												+ ", e.what(): " + e.what()
											);
										}
										catch(...)
										{
											_logger->error(__FILEREF__ + "scan LiveRecorder files failed"
												+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
												+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
												+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
											);
										}
				
										if (!firstChunkRead
											|| lastChunkTimestamp < chrono::system_clock::now() - chrono::minutes(1))
										{
											// if we are here, it means the ffmpeg command is not generating the ts files

											_logger->error(__FILEREF__ + "liveRecorderMonitor. Chunks were not generated"
												+ ", liveRecorder->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
												+ ", liveRecorder->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
												+ ", manifestDirectoryPathName: " + manifestDirectoryPathName
												+ ", firstChunkRead: " + to_string(firstChunkRead)
											);

											chunksWereNotGenerated = true;

											liveRecorderWorking = false;
											localErrorMessage = " restarted because of 'no segments were generated'";

											_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveRecorderMonitor. Live Recorder is not working (no segments were generated). LiveRecorder (ffmpeg) is killed in order to be started again"
												+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
												+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
												+ ", manifestFilePathName: " + manifestFilePathName
												+ ", channelLabel: " + liveRecording->_channelLabel
												+ ", liveRecorder->_childPid: " + to_string(liveRecording->_childPid)
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
														+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
														+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
														+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
													FileIO::remove(segmentPathNameToBeRemoved, exceptionInCaseOfError);
												}
												catch(runtime_error e)
												{
													_logger->error(__FILEREF__ + "remove failed"
														+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
														+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
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
									string errorMessage = string ("liveRecorderMonitorCheck (HLS) on segments (and retention) failed")
										+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;

									_logger->error(__FILEREF__ + errorMessage);
								}
								catch(exception e)
								{
									string errorMessage = string ("liveRecorderMonitorCheck (HLS) on segments (and retention) failed")
										+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;

									_logger->error(__FILEREF__ + errorMessage);
								}

								if (!liveRecorderWorking)
									continue;

								try
								{
									// Second health check (HLS/DASH), looks if the frame is increasing
									int secondsToWaitBetweenSamples = 3;
									if (!liveRecording->_ffmpeg->isFrameIncreasing(secondsToWaitBetweenSamples))
									{
										_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveRecorderMonitor (HLS/DASH). Live Recorder frame is not increasing'. LiveRecorder (ffmpeg) is killed in order to be started again"
											+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
											+ ", channelLabel: " + liveRecording->_channelLabel
											+ ", _childPid: " + to_string(liveRecording->_childPid)
										);

										liveRecorderWorking = false;
										localErrorMessage = " restarted because of 'frame is not increasing'";
									}
								}
								catch(FFMpegEncodingStatusNotAvailable e)
								{
									string errorMessage = string ("liveRecorderMonitorCheck (HLS/DASH) frame increasing check failed")
										+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;
									_logger->warn(__FILEREF__ + errorMessage);
								}
								catch(runtime_error e)
								{
									string errorMessage = string ("liveRecorderMonitorCheck (HLS/DASH) frame increasing check failed")
										+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;
									_logger->error(__FILEREF__ + errorMessage);
								}
								catch(exception e)
								{
									string errorMessage = string ("liveRecorderMonitorCheck (HLS/DASH) frame increasing check failed")
										+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
										+ ", e.what(): " + e.what()
									;
									_logger->error(__FILEREF__ + errorMessage);
								}
							}
							else
							{
								rtmpOutputFound = true;
							}
						}
					}
					if (liveRecorderWorking && rtmpOutputFound)
					{
						try
						{
							// Second health check, rtmp(Proxy), looks if the frame is increasing
							int secondsToWaitBetweenSamples = 3;
							if (!liveRecording->_ffmpeg->isFrameIncreasing(secondsToWaitBetweenSamples))
							{
								_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveRecorderMonitor (rtmp). Live Recorder frame is not increasing'. LiveRecorder (ffmpeg) is killed in order to be started again"
									+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
									+ ", channelLabel: " + liveRecording->_channelLabel
									+ ", _childPid: " + to_string(liveRecording->_childPid)
								);

								liveRecorderWorking = false;

								localErrorMessage = " restarted because of 'frame is not increasing'";
							}
						}
						catch(FFMpegEncodingStatusNotAvailable e)
						{
							string errorMessage = string ("liveRecorderMonitorCheck (rtmp) frame increasing check failed")
								+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;
							_logger->warn(__FILEREF__ + errorMessage);
						}
						catch(runtime_error e)
						{
							string errorMessage = string ("liveRecorderMonitorCheck (rtmp) frame increasing check failed")
								+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;
							_logger->error(__FILEREF__ + errorMessage);
						}
						catch(exception e)
						{
							string errorMessage = string ("liveRecorderMonitorCheck (rtmp) frame increasing check failed")
								+ ", _ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
								+ ", e.what(): " + e.what()
							;
							_logger->error(__FILEREF__ + errorMessage);
						}
					}

					if (!liveRecorderWorking)
					{
						_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveRecordingMonitor. Live Recording is not working (segment list file is missing or was not updated). LiveRecording (ffmpeg) is killed in order to be started again"
							+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
							+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
							+ ", channelLabel: " + liveRecording->_channelLabel
							+ ", liveRecording->_childPid: " + to_string(liveRecording->_childPid)
						);

						try
						{
							ProcessUtility::killProcess(liveRecording->_childPid);
							liveRecording->_killedBecauseOfNotWorking = true;
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
								liveRecording->_errorMessage = string(strDateTime) + " "
									+ liveRecording->_channelLabel +
									localErrorMessage;
							}
						}
						catch(runtime_error e)
						{
							string errorMessage = string("ProcessUtility::killProcess failed")
								+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
								+ ", liveRecording->_childPid: " + to_string(liveRecording->_childPid)
								+ ", e.what(): " + e.what()
									;
							_logger->error(__FILEREF__ + errorMessage);
						}
					}

					_logger->info(__FILEREF__ + "liveRecordingMonitorCheck"
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
						+ ", @MMS statistics@ - elapsed time: @" + to_string(
							chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - now).count()
						) + "@"
					);
				}
				else
				{
					liveRecordingNotRunningCounter++;
				}
			}
			_logger->info(__FILEREF__ + "monitoringThread, LiveRecording"
				+ ", total LiveRecording: " + to_string(liveRecordingRunningCounter + liveRecordingNotRunningCounter)
				+ ", liveRecordingRunningCounter: " + to_string(liveRecordingRunningCounter)
				+ ", liveRecordingNotRunningCounter: " + to_string(liveRecordingNotRunningCounter)
			);
		}
		catch(runtime_error e)
		{
			string errorMessage = string ("monitor LiveRecording failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("monitor LiveRecording failed")
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
		this_thread::sleep_for(chrono::milliseconds(50));

		try
		{
			lock_guard<mutex> locker(*_cpuUsageMutex);

			*_cpuUsage = _getCpuUsage.getCpuUsage();

			if (++counter % 100 == 0)
				_logger->info(__FILEREF__ + "cpuUsageThread"
					+ ", _cpuUsage: " + to_string(*_cpuUsage)
				);
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

void FFMPEGEncoder::createOrUpdateSatelliteDvbLastConfigurationFile(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string multicastIP,
	string multicastPort,
	int64_t satelliteServiceId,
	int64_t satelliteFrequency,
	int64_t satelliteSymbolRate,
	string satelliteModulation,
	int satelliteVideoPid,
	int satelliteAudioItalianPid,
	bool toBeAdded
)
{
	try
	{
		_logger->info(__FILEREF__ + "Received createOrUpdateSatelliteDvbLastConfigurationFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", multicastIP: " + multicastIP
			+ ", multicastPort: " + multicastPort
			+ ", satelliteServiceId: " + to_string(satelliteServiceId)
			+ ", satelliteFrequency: " + to_string(satelliteFrequency)
			+ ", satelliteSymbolRate: " + to_string(satelliteSymbolRate)
			+ ", satelliteModulation: " + satelliteModulation
			+ ", satelliteVideoPid: " + to_string(satelliteVideoPid)
			+ ", satelliteAudioItalianPid: " + to_string(satelliteAudioItalianPid)
			+ ", toBeAdded: " + to_string(toBeAdded)
		);

		string localModulation;

		// dvblast modulation: qpsk|psk_8|apsk_16|apsk_32
		if (satelliteModulation != "")
		{
			if (satelliteModulation == "PSK/8")
				localModulation = "psk_8";
			else if (satelliteModulation == "QPSK")
				localModulation = "qpsk";
			else
			{
				string errorMessage = __FILEREF__ + "unknown modulation"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", satelliteModulation: " + satelliteModulation
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (!FileIO::directoryExisting(_satelliteChannelConfigurationDirectory))
		{
			_logger->info(__FILEREF__ + "Create directory"
				+ ", _ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(encodingJobKey)
				+ ", _satelliteChannelConfigurationDirectory: " + _satelliteChannelConfigurationDirectory
			);

			bool noErrorIfExists = true;
			bool recursive = true;
			FileIO::createDirectory(
				_satelliteChannelConfigurationDirectory,
				S_IRUSR | S_IWUSR | S_IXUSR |
				S_IRGRP | S_IWUSR | S_IXGRP |
				S_IROTH | S_IWUSR | S_IXOTH,
				noErrorIfExists, recursive);
		}

		string dvblastConfigurationPathName =
			_satelliteChannelConfigurationDirectory
			+ "/" + to_string(satelliteFrequency)
			+ "-" + to_string(satelliteSymbolRate)
			+ "-" + localModulation
		;

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
			+ to_string(satelliteServiceId)
			+ " "
			+ to_string(satelliteVideoPid) + "," + to_string(satelliteAudioItalianPid)
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
		string errorMessage = __FILEREF__ + "createOrUpdateSatelliteDvbLastConfigurationFile failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		;
		_logger->error(errorMessage);
	}
}

pair<string, string> FFMPEGEncoder::getSatelliteMulticastFromDvblastConfigurationFile(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	int64_t satelliteServiceId,
	int64_t satelliteFrequency,
	int64_t satelliteSymbolRate,
	string satelliteModulation
)
{
	string multicastIP;
	string multicastPort;

	try
	{
		_logger->info(__FILEREF__ + "Received getSatelliteMulticastFromDvblastConfigurationFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", satelliteServiceId: " + to_string(satelliteServiceId)
			+ ", satelliteFrequency: " + to_string(satelliteFrequency)
			+ ", satelliteSymbolRate: " + to_string(satelliteSymbolRate)
			+ ", satelliteModulation: " + satelliteModulation
		);

		string localModulation;

		// dvblast modulation: qpsk|psk_8|apsk_16|apsk_32
		if (satelliteModulation != "")
		{
			if (satelliteModulation == "PSK/8")
				localModulation = "psk_8";
			else if (satelliteModulation == "QPSK")
				localModulation = "qpsk";
			else
			{
				string errorMessage = __FILEREF__ + "unknown modulation"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", satelliteModulation: " + satelliteModulation
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string dvblastConfigurationPathName =
			_satelliteChannelConfigurationDirectory
			+ "/" + to_string(satelliteFrequency)
			+ "-" + to_string(satelliteSymbolRate)
			+ "-" + localModulation
		;

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

				if (configurationPieces[2] == to_string(satelliteServiceId))
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

		_logger->info(__FILEREF__ + "Received getSatelliteMulticastFromDvblastConfigurationFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", satelliteServiceId: " + to_string(satelliteServiceId)
			+ ", satelliteFrequency: " + to_string(satelliteFrequency)
			+ ", satelliteSymbolRate: " + to_string(satelliteSymbolRate)
			+ ", satelliteModulation: " + satelliteModulation
			+ ", multicastIP: " + multicastIP
			+ ", multicastPort: " + multicastPort
		);
	}
	catch (...)
	{
		// make sure do not raise an exception to the calling method to avoid
		// to interrupt "closure" encoding procedure
		string errorMessage = __FILEREF__ + "getSatelliteMulticastFromDvblastConfigurationFile failed"
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

		int maxCapability;


		if (*_cpuUsage > _cpuUsageThresholdForEncoding)
			maxCapability = 0;						// no to be done
		else
			maxCapability = VECTOR_MAX_CAPACITY;	// it could be done

		_logger->info(__FILEREF__ + "getMaxXXXXCapability"
			+ ", _cpuUsage: " + to_string(*_cpuUsage)
			+ ", maxCapability: " + to_string(maxCapability)
		);

		return maxCapability;
	}

	/*
	int maxEncodingsCapability = 1;

	try
	{
		if (FileIO::fileExisting(_encoderCapabilityConfigurationPathName))
		{
			Json::Value encoderCapabilityConfiguration = APICommon::loadConfigurationFile(
				_encoderCapabilityConfigurationPathName.c_str());

			maxEncodingsCapability = JSONUtils::asInt(encoderCapabilityConfiguration["ffmpeg"],
				"maxEncodingsCapability", 1);
			_logger->info(__FILEREF__ + "Configuration item"
				+ ", ffmpeg->maxEncodingsCapability: " + to_string(maxEncodingsCapability)
			);

			if (maxEncodingsCapability > VECTOR_MAX_CAPACITY)
			{
				_logger->error(__FILEREF__ + "getMaxXXXXCapability. maxEncodingsCapability cannot be bigger than VECTOR_MAX_CAPACITY"
					+ ", _encoderCapabilityConfigurationPathName: " + _encoderCapabilityConfigurationPathName
					+ ", maxEncodingsCapability: " + to_string(maxEncodingsCapability)
					+ ", VECTOR_MAX_CAPACITY: " + to_string(VECTOR_MAX_CAPACITY)
				);

				maxEncodingsCapability = VECTOR_MAX_CAPACITY;
			}

			maxEncodingsCapability = calculateCapabilitiesBasedOnOtherRunningProcesses(
				maxEncodingsCapability,
				-1,
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
		+ ", maxEncodingsCapability: " + to_string(maxEncodingsCapability)
	);

	return maxEncodingsCapability;
	*/
}

int FFMPEGEncoder::getMaxLiveProxiesCapability(void)
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to be done
	{
		lock_guard<mutex> locker(*_cpuUsageMutex);

		int maxCapability;

		if (*_cpuUsage > _cpuUsageThresholdForProxy)
			maxCapability = 0;						// no to be done
		else
			maxCapability = VECTOR_MAX_CAPACITY;	// it could be done

		_logger->info(__FILEREF__ + "getMaxXXXXCapability"
			+ ", _cpuUsage: " + to_string(*_cpuUsage)
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

		int maxCapability;

		if (*_cpuUsage > _cpuUsageThresholdForRecording)
			maxCapability = 0;						// no to be done
		else
			maxCapability = VECTOR_MAX_CAPACITY;	// it could be done

		_logger->info(__FILEREF__ + "getMaxXXXXCapability"
			+ ", _cpuUsage: " + to_string(*_cpuUsage)
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

/*
int FFMPEGEncoder::calculateCapabilitiesBasedOnOtherRunningProcesses(
	int configuredMaxEncodingsCapability,	// != -1 if we want to calculate maxEncodingsCapability
	int configuredMaxLiveProxiesCapability,	// != -1 if we want to calculate maxLiveProxiesCapability
	int configuredMaxLiveRecordingsCapability	// != -1 if we want to calculate maxLiveRecordingsCapability
)
{

	// proportion are: 1 encoding likes 3 recorders (1 encoding) likes 6 proxies

	int oneEncodingEqualsToRecorderNumber = 3;
	int oneEncodingEqualsToProxyNumber = 6;
	int oneRecorderEqualsToProxyNumber = 3;

	if (configuredMaxEncodingsCapability != -1)
	{
		int currentLiveProxiesRunning = 0;
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			#ifdef __VECTOR__
			for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			{
				if (liveProxy->_running)
					currentLiveProxiesRunning++;
			}
			#else	// __MAP__
			currentLiveProxiesRunning = _liveProxiesCapability->size();
			#endif
		}

		int currentLiveRecorderRunning = 0;
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			#ifdef __VECTOR__
			for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
			{
				if (liveRecording->_running)
					currentLiveRecorderRunning++;
			}
			#else	// __MAP__
			currentLiveRecorderRunning = _liveRecordingsCapability->size();
			#endif
		}

		int encodingsToBeSubtractedBecauseOfProxies = currentLiveProxiesRunning / oneEncodingEqualsToProxyNumber;
		int encodingsToBeSubtractedBecauseOfRecorders = currentLiveRecorderRunning / oneEncodingEqualsToRecorderNumber;

		int newMaxEncodingsCapability;
		if (encodingsToBeSubtractedBecauseOfProxies + encodingsToBeSubtractedBecauseOfRecorders
				> configuredMaxEncodingsCapability)
			newMaxEncodingsCapability = 0;
		else
			newMaxEncodingsCapability = configuredMaxEncodingsCapability
				- (encodingsToBeSubtractedBecauseOfProxies + encodingsToBeSubtractedBecauseOfRecorders);

		if (encodingsToBeSubtractedBecauseOfProxies + encodingsToBeSubtractedBecauseOfRecorders > 0)
			_logger->warn(__FILEREF__ + "getMaxXXXXCapability. capability reduced because of other processes running"
				+ ", configuredMaxEncodingsCapability: " + to_string(configuredMaxEncodingsCapability)
				+ ", currentLiveProxiesRunning: " + to_string(currentLiveProxiesRunning)
				+ ", encodingsToBeSubtractedBecauseOfProxies: " + to_string(encodingsToBeSubtractedBecauseOfProxies)
				+ ", currentLiveRecorderRunning: " + to_string(currentLiveRecorderRunning)
				+ ", encodingsToBeSubtractedBecauseOfRecorders: " + to_string(encodingsToBeSubtractedBecauseOfRecorders)
				+ ", newMaxEncodingsCapability: " + to_string(newMaxEncodingsCapability)
			);

		return newMaxEncodingsCapability;
	}
	else if (configuredMaxLiveProxiesCapability != -1)
	{
		int currentEncodingsRunning = 0;
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			{
				if (encoding->_running)
					currentEncodingsRunning++;
			}
			#else	// __MAP__
			currentEncodingsRunning = _encodingsCapability->size();
			#endif
		}

		int currentLiveRecorderRunning = 0;
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			#ifdef __VECTOR__
			for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
			{
				if (liveRecording->_running)
					currentLiveRecorderRunning++;
			}
			#else	// __MAP__
			currentLiveRecorderRunning = _liveRecordingsCapability->size();
			#endif
		}

		int proxiesToBeSubtractedBecauseOfEncodings = currentEncodingsRunning * oneEncodingEqualsToProxyNumber;
		int proxiesToBeSubtractedBecauseOfRecorders = currentLiveRecorderRunning * oneRecorderEqualsToProxyNumber;

		int newMaxLiveProxiesCapability;
		if (proxiesToBeSubtractedBecauseOfEncodings + proxiesToBeSubtractedBecauseOfRecorders
				> configuredMaxLiveProxiesCapability)
			newMaxLiveProxiesCapability = 0;
		else
			newMaxLiveProxiesCapability = configuredMaxLiveProxiesCapability
				- (proxiesToBeSubtractedBecauseOfEncodings + proxiesToBeSubtractedBecauseOfRecorders);

		if (proxiesToBeSubtractedBecauseOfEncodings + proxiesToBeSubtractedBecauseOfRecorders > 0)
			_logger->warn(__FILEREF__ + "getMaxXXXXCapability. capability reduced because of other processes running"
				+ ", configuredMaxLiveProxiesCapability: " + to_string(configuredMaxLiveProxiesCapability)
				+ ", currentEncodingsRunning: " + to_string(currentEncodingsRunning)
				+ ", proxiesToBeSubtractedBecauseOfEncodings: " + to_string(proxiesToBeSubtractedBecauseOfEncodings)
				+ ", currentLiveRecorderRunning: " + to_string(currentLiveRecorderRunning)
				+ ", proxiesToBeSubtractedBecauseOfRecorders: " + to_string(proxiesToBeSubtractedBecauseOfRecorders)
				+ ", newMaxLiveProxiesCapability: " + to_string(newMaxLiveProxiesCapability)
			);

		return newMaxLiveProxiesCapability;
	}
	else if (configuredMaxLiveRecordingsCapability != -1)
	{
		int currentEncodingsRunning = 0;
		{
			lock_guard<mutex> locker(*_encodingMutex);

			#ifdef __VECTOR__
			for (shared_ptr<Encoding> encoding: *_encodingsCapability)
			{
				if (encoding->_running)
					currentEncodingsRunning++;
			}
			#else	// __MAP__
			currentEncodingsRunning = _encodingsCapability->size();
			#endif
		}

		int currentLiveProxiesRunning = 0;
		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			#ifdef __VECTOR__
			for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			{
				if (liveProxy->_running)
					currentLiveProxiesRunning++;
			}
			#else	// __MAP__
			currentLiveProxiesRunning = _liveProxiesCapability->size();
			#endif
		}

		int newMaxLiveRecordingsCapability;
		int recordersToBeSubtractedBecauseOfEncodings = currentEncodingsRunning * oneEncodingEqualsToRecorderNumber;
		int recordersToBeSubtractedBecauseOfProxies = currentLiveProxiesRunning / oneRecorderEqualsToProxyNumber;

		if (recordersToBeSubtractedBecauseOfEncodings + recordersToBeSubtractedBecauseOfProxies
				> configuredMaxLiveRecordingsCapability)
			newMaxLiveRecordingsCapability = 0;
		else
			newMaxLiveRecordingsCapability = configuredMaxLiveRecordingsCapability
				- (recordersToBeSubtractedBecauseOfEncodings + recordersToBeSubtractedBecauseOfProxies);

		if (recordersToBeSubtractedBecauseOfEncodings + recordersToBeSubtractedBecauseOfProxies > 0)
			_logger->warn(__FILEREF__ + "getMaxXXXXCapability. capability reduced because of other processes running"
				+ ", configuredMaxLiveRecordingsCapability: " + to_string(configuredMaxLiveRecordingsCapability)
				+ ", currentEncodingsRunning: " + to_string(currentEncodingsRunning)
				+ ", recordersToBeSubtractedBecauseOfEncodings: " + to_string(recordersToBeSubtractedBecauseOfEncodings)
				+ ", currentLiveProxiesRunning: " + to_string(currentLiveProxiesRunning)
				+ ", recordersToBeSubtractedBecauseOfProxies: " + to_string(recordersToBeSubtractedBecauseOfProxies)
				+ ", newMaxLiveRecordingsCapability: " + to_string(newMaxLiveRecordingsCapability)
			);

		return newMaxLiveRecordingsCapability;
	}
	else
	{
		_logger->error(__FILEREF__ + "getMaxXXXXCapability. Wrong call to calculateCapabilitiesBasedOnOtherRunningProcesses"
				+ ", configuredMaxEncodingsCapability: " + to_string(configuredMaxEncodingsCapability)
				+ ", configuredMaxLiveProxiesCapability: " + to_string(configuredMaxLiveProxiesCapability)
				+ ", configuredMaxLiveRecordingsCapability: " + to_string(configuredMaxLiveRecordingsCapability)
			);

		return 0;
	}
}
*/

