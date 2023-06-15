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
#include "catralibraries/DateTime.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/GetCpuUsage.h"
#include "FFMPEGEncoder.h"
#include "FFMPEGEncoderDaemons.h"
#include "MMSStorage.h"
#include "EncodeContent.h"
#include "OverlayImageOnVideo.h"
#include "OverlayTextOnVideo.h"
#include "GenerateFrames.h"
#include "SlideShow.h"
#include "VideoSpeed.h"
#include "AddSilentAudio.h"
#include "PictureInPicture.h"
#include "IntroOutroOverlay.h"
#include "CutFrameAccurate.h"
#include "LiveRecorder.h"
#include "LiveRecorderDaemons.h"
#include "LiveProxy.h"
#include "LiveGrid.h"

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

		string logPathName =  JSONUtils::asString(configuration["log"]["encoder"], "pathName", "");
		string logType =  JSONUtils::asString(configuration["log"]["encoder"], "type", "");
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
    
		string logLevel =  JSONUtils::asString(configuration["log"]["encoder"], "level", "err");
		if (logLevel == "debug")
			spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off
		else if (logLevel == "info")
			spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off
		else if (logLevel == "err")
			spdlog::set_level(spdlog::level::err); // trace, debug, info, warn, err, critical, off
		string pattern =  JSONUtils::asString(configuration["log"]["encoder"], "pattern", "");
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
				shared_ptr<FFMPEGEncoderBase::Encoding>    encoding = make_shared<FFMPEGEncoderBase::Encoding>();
				encoding->_available   = true;
				encoding->_childPid		= 0;	// not running
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
				shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>    liveProxy = make_shared<FFMPEGEncoderBase::LiveProxyAndGrid>();
				liveProxy->_available				= true;
				liveProxy->_childPid				= 0;	// not running
				liveProxy->_ingestionJobKey			= 0;
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
				shared_ptr<FFMPEGEncoderBase::LiveRecording>    liveRecording = make_shared<FFMPEGEncoderBase::LiveRecording>();
				liveRecording->_available			= true;
				liveRecording->_childPid			= 0;	// not running
				liveRecording->_ingestionJobKey		= 0;
				liveRecording->_encodingParametersRoot = Json::nullValue;
				liveRecording->_ffmpeg   = make_shared<FFMpeg>(configuration, logger);

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
		// if (threadsNumber > 0)
		{
			// thread liveRecorderChunksIngestion(&FFMPEGEncoder::liveRecorderChunksIngestionThread,
			// 		ffmpegEncoders[0]);
			// thread liveRecorderVirtualVODIngestion(&FFMPEGEncoder::liveRecorderVirtualVODIngestionThread,
			// 		ffmpegEncoders[0]);
			shared_ptr<LiveRecorderDaemons> liveRecorderDaemons = make_shared<LiveRecorderDaemons>(
				configuration, &liveRecordingMutex, &liveRecordingsCapability, logger);

			thread liveRecorderChunksIngestion(&LiveRecorderDaemons::startChunksIngestionThread,
					liveRecorderDaemons);
			thread liveRecorderVirtualVODIngestion(&LiveRecorderDaemons::startVirtualVODIngestionThread,
					liveRecorderDaemons);

			// thread monitor(&FFMPEGEncoder::monitorThread, ffmpegEncoders[0]);
			// thread cpuUsage(&FFMPEGEncoder::cpuUsageThread, ffmpegEncoders[0]);
			shared_ptr<FFMPEGEncoderDaemons> ffmpegEncoderDaemons = make_shared<FFMPEGEncoderDaemons>(
				configuration, &liveRecordingMutex, &liveRecordingsCapability,
				&liveProxyMutex, &liveProxiesCapability, &cpuUsageMutex, &cpuUsage, logger);

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
		vector<shared_ptr<FFMPEGEncoderBase::Encoding>>* encodingsCapability,

		mutex* liveProxyMutex,
		vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>>* liveProxiesCapability,

		mutex* liveRecordingMutex,
		vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>>* liveRecordingsCapability,

		mutex* encodingCompletedMutex,
		map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>* encodingCompletedMap,
		chrono::system_clock::time_point* lastEncodingCompletedCheck,

		mutex* tvChannelsPortsMutex,
		long* tvChannelPort_CurrentOffset,

        shared_ptr<spdlog::logger> logger)
    : APICommon(configuration, 
		fcgiAcceptMutex,
		logger) 
{

    _encodingCompletedRetentionInSeconds = JSONUtils::asInt(_configuration["ffmpeg"], "encodingCompletedRetentionInSeconds", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encodingCompletedRetentionInSeconds: " + to_string(_encodingCompletedRetentionInSeconds)
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

	_liveProxyMutex = liveProxyMutex;
	_liveProxiesCapability = liveProxiesCapability;

	_liveRecordingMutex = liveRecordingMutex;
	_liveRecordingsCapability = liveRecordingsCapability;

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

			string responseBody = JSONUtils::toString(responseBodyRoot);

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

			string responseBody = JSONUtils::toString(infoRoot);

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
	/*
    else if (method == "encodeContent")
    {
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
			_logger->error(__FILEREF__ + "The 'ingestionJobKey' parameter is not found");

			string errorMessage = string("Internal server error");

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
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
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + MaxConcurrentJobsReached().what();
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0;	// not running
				selectedEncoding->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating encodeContent thread"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread encodeContentThread(&FFMPEGEncoder::encodeContentThread, this,
					selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
				encodeContentThread.detach();

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "encodeContentThread failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "encodeContentThread failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
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
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + MaxConcurrentJobsReached().what();
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0;	// not running
				selectedEncoding->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating cut thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread cutFrameAccurateThread(&FFMPEGEncoder::cutFrameAccurateThread,
					this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
				cutFrameAccurateThread.detach();

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "cutFrameAccurateThread failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "cutFrameAccurateThread failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
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
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + MaxConcurrentJobsReached().what();
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0;	// not running
				selectedEncoding->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating overlayImageOnVideo thread"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread overlayImageOnVideoThread(&FFMPEGEncoder::overlayImageOnVideoThread,
					this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
				overlayImageOnVideoThread.detach();

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "overlayImageOnVideoThread failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "overlayImageOnVideoThread failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
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
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + MaxConcurrentJobsReached().what();
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0;	// not running
				selectedEncoding->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating overlayTextOnVideo thread"
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread overlayTextOnVideoThread(&FFMPEGEncoder::overlayTextOnVideoThread,
					this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
				overlayTextOnVideoThread.detach();

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "overlayTextOnVideoThread failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "overlayTextOnVideoThread failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
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
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + MaxConcurrentJobsReached().what();
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0;	// not running
				selectedEncoding->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating generateFrames thread"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread generateFramesThread(&FFMPEGEncoder::generateFramesThread,
					this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
				generateFramesThread.detach();

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "generateFrames failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "generateFramesThread failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
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
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + MaxConcurrentJobsReached().what();
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0;	// not running
				selectedEncoding->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating slideShow thread"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread slideShowThread(&FFMPEGEncoder::slideShowThread,
					this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
				slideShowThread.detach();

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "slideShow failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "slideShowThread failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
	*/
    else if (method == "videoSpeed"
		|| method == "encodeContent"
		|| method == "cutFrameAccurate"
		|| method == "overlayImageOnVideo"
		|| method == "overlayTextOnVideo"
		|| method == "generateFrames"
		|| method == "slideShow"
		|| method == "addSilentAudio"
		|| method == "pictureInPicture"
		|| method == "introOutroOverlay"
	)
    {
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
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
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + MaxConcurrentJobsReached().what();
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				// 2023-06-15: scenario: tanti encoding stanno aspettando di essere gestiti.
				//	L'encoder finisce il task in corso, tutti gli encoding in attesa 
				//	verificano che la CPU è bassa e, tutti entrano per essere gestiti.
				// Per risolvere questo problema, è necessario aggiornare _lastEncodingAcceptedTime
				// as soon as possible, altrimenti tutti quelli in coda entrano per essere gestiti
				*_lastEncodingAcceptedTime = chrono::system_clock::now();

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0;	// not running
				selectedEncoding->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating " + method + " thread"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				if (method == "videoSpeed")
				{
					thread videoSpeedThread(&FFMPEGEncoder::videoSpeedThread,
						this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					videoSpeedThread.detach();
				}
				else if (method == "encodeContent")
				{
					thread encodeContentThread(&FFMPEGEncoder::encodeContentThread, this,
						selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					encodeContentThread.detach();
				}
				else if (method == "cutFrameAccurate")
				{
					thread cutFrameAccurateThread(&FFMPEGEncoder::cutFrameAccurateThread,
						this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					cutFrameAccurateThread.detach();
				}
				else if (method == "overlayImageOnVideo")
				{
					thread overlayImageOnVideoThread(&FFMPEGEncoder::overlayImageOnVideoThread,
						this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					overlayImageOnVideoThread.detach();
				}
				else if (method == "overlayTextOnVideo")
				{
					thread overlayTextOnVideoThread(&FFMPEGEncoder::overlayTextOnVideoThread,
						this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					overlayTextOnVideoThread.detach();
				}
				else if (method == "addSilentAudio")
				{
					thread addSilentAudioThread(&FFMPEGEncoder::addSilentAudioThread,
						this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					addSilentAudioThread.detach();
				}
				else if (method == "slideShow")
				{
					thread slideShowThread(&FFMPEGEncoder::slideShowThread,
						this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					slideShowThread.detach();
				}
				else if (method == "generateFrames")
				{
					thread generateFramesThread(&FFMPEGEncoder::generateFramesThread,
						this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					generateFramesThread.detach();
				}
				else if (method == "pictureInPicture")
				{
					thread pictureInPictureThread(&FFMPEGEncoder::pictureInPictureThread,
						this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					pictureInPictureThread.detach();
				}
				else if (method == "introOutroOverlay")
				{
					thread introOutroOverlayThread(&FFMPEGEncoder::introOutroOverlayThread,
						this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
					introOutroOverlayThread.detach();
				}
				else
				{
					selectedEncoding->_available = true;
					selectedEncoding->_childPid = 0;	// not running

					string errorMessage = string("wrong method")
						+ ", method: " + method
					;

					_logger->error(__FILEREF__ + errorMessage
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
					);

					sendError(request, 500, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}
			}
			catch(exception e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + method + " failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "sendSuccess failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
	/*
    else if (method == "pictureInPicture")
    {
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
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
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + MaxConcurrentJobsReached().what();
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0;	// not running
				selectedEncoding->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating pictureInPicture thread"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread pictureInPictureThread(&FFMPEGEncoder::pictureInPictureThread,
					this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
				pictureInPictureThread.detach();

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "pictureInPictureThread failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "sendSuccess failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for(int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
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
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
						+ ", " + MaxConcurrentJobsReached().what();
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0;	// not running
				selectedEncoding->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating introOutroOverlay thread"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread introOutroOverlayThread(&FFMPEGEncoder::introOutroOverlayThread,
					this, selectedEncoding, ingestionJobKey, encodingJobKey, requestBody);
				introOutroOverlayThread.detach();

				*_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "introOutroOverlayThread failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "sendSuccess failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
	*/
    else if (method == "liveRecorder")
    {
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::LiveRecording>    selectedLiveRecording;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording: *_liveRecordingsCapability)
			int maxLiveRecordingsCapability = getMaxLiveRecordingsCapability();
			for(int liveRecordingIndex = 0; liveRecordingIndex < maxLiveRecordingsCapability;
				liveRecordingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording = (*_liveRecordingsCapability)[liveRecordingIndex];

				if (liveRecording->_available)
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

				// 2022-11-23: ho visto che, in caso di autoRenew, monitoring generates errors and trys to kill
				//		the process. Moreover the selectedLiveRecording->_errorMessage remain initialized
				//		with the error (like killed because segment file is not present).
				//		For this reason, _recordingStart is initialized to make sure monitoring does not perform
				//		his checks before recorder is not really started.
				//		_recordingStart will be initialized correctly into the liveRecorderThread method
				selectedLiveRecording->_recordingStart = chrono::system_clock::now() + chrono::seconds(60);
				selectedLiveRecording->_available = false;
				selectedLiveRecording->_childPid = 0;	// not running
				selectedLiveRecording->_encodingJobKey = encodingJobKey;

				_logger->info(__FILEREF__ + "Creating liveRecorder thread"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", selectedLiveRecording->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread liveRecorderThread(&FFMPEGEncoder::liveRecorderThread,
					this, selectedLiveRecording, ingestionJobKey, encodingJobKey, requestBody);
				liveRecorderThread.detach();

				// *_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedLiveRecording->_available = true;
				selectedLiveRecording->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "liveRecorder failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "liveRecorderThread failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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

			shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>    selectedLiveProxy;
			bool					freeEncodingFound = false;
			bool					encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			int maxLiveProxiesCapability = getMaxLiveProxiesCapability();
			for(int liveProxyIndex = 0; liveProxyIndex < maxLiveProxiesCapability; liveProxyIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy = (*_liveProxiesCapability)[liveProxyIndex];

				if (liveProxy->_available)
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
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning)
					+ ", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}

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

				selectedLiveProxy->_available = false;
				selectedLiveProxy->_childPid = 0;	// not running
				selectedLiveProxy->_encodingJobKey = encodingJobKey;
				selectedLiveProxy->_method = method;

				if (method == "liveProxy")
				{
					_logger->info(__FILEREF__ + "Creating liveProxy thread"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
						+ ", requestBody: " + requestBody
					);
					thread liveProxyThread(&FFMPEGEncoder::liveProxyThread,
						this, selectedLiveProxy, ingestionJobKey, encodingJobKey, requestBody);
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
						+ ", requestBody: " + requestBody
					);
					thread liveGridThread(&FFMPEGEncoder::liveGridThread,
						this, selectedLiveProxy, ingestionJobKey, encodingJobKey, requestBody);
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

				// *_lastEncodingAcceptedTime = chrono::system_clock::now();
			}
			catch(exception e)
			{
				selectedLiveProxy->_available = true;
				selectedLiveProxy->_childPid = 0;	// not running

				_logger->error(__FILEREF__ + "liveProxyThread failed"
					+ ", method: " + method
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "liveProxy/vodProxy/liveGrid/awaitingTheBeginning Thread failed"
                + ", method: " + method
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
		int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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
		shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;

		bool                    liveProxyFound = false;
		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>	selectedLiveProxy;

		bool                    liveRecordingFound = false;
		shared_ptr<FFMPEGEncoderBase::LiveRecording>    selectedLiveRecording;

		bool                    encodingCompleted = false;
		shared_ptr<FFMPEGEncoderBase::EncodingCompleted>    selectedEncodingCompleted;

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

			map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>::iterator it =
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
				for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
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
					for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
					{
						if (liveProxy->_encodingJobKey == encodingJobKey)
						{
							liveProxyFound = true;
							selectedLiveProxy = liveProxy;

							break;
						}
					}
				}

				if (!liveProxyFound)
				{
					for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording: *_liveRecordingsCapability)
					{
						if (liveRecording->_encodingJobKey == encodingJobKey)
						{
							liveRecordingFound = true;
							selectedLiveRecording = liveRecording;

							break;
						}
					}
				}
			}
		}

		chrono::system_clock::time_point endLookingForEncodingStatus = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "encodingStatus"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", encodingFound: " + to_string(encodingFound)
			+ (encodingFound ? (", available: " + to_string(selectedEncoding->_available)) : "")
			+ (encodingFound ? (", childPid: " + to_string(selectedEncoding->_childPid)) : "")
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
			Json::Value responseBodyRoot;

			string field = "ingestionJobKey";
			responseBodyRoot[field] = ingestionJobKey;

			field = "encodingJobKey";
			responseBodyRoot[field] = encodingJobKey;

			field = "pid";
			responseBodyRoot[field] = 0;

			field = "killedByUser";
			responseBodyRoot[field] = false;

			field = "encodingFinished";
			responseBodyRoot[field] = true;

			field = "encodingProgress";
			responseBodyRoot[field] = 100;

			responseBody = JSONUtils::toString(responseBodyRoot);
        }
        else
        {
			if (encodingCompleted)
			{
				Json::Value responseBodyRoot;

				string field = "ingestionJobKey";
				responseBodyRoot[field] = ingestionJobKey;

				field = "encodingJobKey";
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

				responseBody = JSONUtils::toString(responseBodyRoot);
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", e.what(): " + e.what()
                    ;
					_logger->error(__FILEREF__ + errorMessage);

					// sendError(request, 500, errorMessage);

					// throw e;
					// return;
				}

				Json::Value responseBodyRoot;

				string field = "ingestionJobKey";
				responseBodyRoot[field] = ingestionJobKey;

				field = "encodingJobKey";
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
				responseBodyRoot[field] = encodingCompleted;

				field = "encodingProgress";
				if (encodingProgress == -2)
				{
					if (selectedEncoding->_available && !encodingCompleted)	// non dovrebbe accadere mai
						responseBodyRoot[field] = 100;
					else
						responseBodyRoot[field] = Json::nullValue;
				}
				else
					responseBodyRoot[field] = encodingProgress;

				responseBody = JSONUtils::toString(responseBodyRoot);
			}
			else if (liveProxyFound)
			{
				Json::Value responseBodyRoot;

				string field = "ingestionJobKey";
				responseBodyRoot[field] = selectedLiveProxy->_ingestionJobKey;

				field = "encodingJobKey";
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
				responseBodyRoot[field] = encodingCompleted;

				// 2020-06-11: it's a live, it does not have sense the encoding progress
				field = "encodingProgress";
				responseBodyRoot[field] = Json::nullValue;

				responseBody = JSONUtils::toString(responseBodyRoot);
			}
			else // if (liveRecording)
			{
				Json::Value responseBodyRoot;

				string field = "ingestionJobKey";
				responseBodyRoot[field] = selectedLiveRecording->_ingestionJobKey;

				field = "encodingJobKey";
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
				responseBodyRoot[field] = encodingCompleted;

				// 2020-10-13: we do not have here the information to calculate the encoding progress,
				//	it is calculated in EncoderVideoAudioProxy.cpp
				field = "encodingProgress";
				responseBodyRoot[field] = Json::nullValue;

				responseBody = JSONUtils::toString(responseBodyRoot);
			}
        }

		chrono::system_clock::time_point endEncodingStatus = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "encodingStatus"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
		}
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

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
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", lightKill: " + to_string(lightKill)
		);

		pid_t			pidToBeKilled;
		bool			encodingFound = false;
		bool			liveProxyFound = false;
		bool			liveRecorderFound = false;

		{
			for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
			{
				if (encoding->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					pidToBeKilled = encoding->_childPid;

					break;
				}
			}
		}

		if (!encodingFound)
		{
			for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			{
				if (liveProxy->_encodingJobKey == encodingJobKey)
				{
					liveProxyFound = true;
					pidToBeKilled = liveProxy->_childPid;

					break;
				}
			}
		}

		if (!encodingFound && !liveProxyFound)
		{
			for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording: *_liveRecordingsCapability)
			{
				if (liveRecording->_encodingJobKey == encodingJobKey)
				{
					liveRecorderFound = true;
					pidToBeKilled = liveRecording->_childPid;

					break;
				}
			}
		}

        if (!encodingFound && !liveProxyFound && !liveRecorderFound)
        {
            string errorMessage =
				"ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", " + NoEncodingJobKeyFound().what();
            
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(errorMessage);
			return;
        }

		_logger->info(__FILEREF__ + "ProcessUtility::killProcess. Found Encoding to kill"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", pidToBeKilled: " + to_string(pidToBeKilled)
			+ ", lightKill: " + to_string(lightKill)
		);

		if (pidToBeKilled == 0)
		{
			_logger->error(__FILEREF__
				+ "The EncodingJob seems not running (see pidToBeKilled)"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
				// 2023-02-18: using SIGQUIT, the process was not stopped, it worked with SIGTERM
				//	SIGTERM now is managed by FFMpeg.cpp too
				_logger->info(__FILEREF__ + "ProcessUtility::termProcess"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", pidToBeKilled: " + to_string(pidToBeKilled)
				);
				// ProcessUtility::quitProcess(pidToBeKilled);
				ProcessUtility::termProcess(pidToBeKilled);
			}
			else
			{
				_logger->info(__FILEREF__ + "ProcessUtility::killProcess"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", pidToBeKilled: " + to_string(pidToBeKilled)
                + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw e;
        }

		string responseBody;
		{
			/*
			string responseBody = string("{ ")
				+ "\"ingestionJobKey\": " + to_string(ingestionJobKey)
				+ "\"encodingJobKey\": " + to_string(encodingJobKey)
				+ ", \"pid\": " + to_string(pidToBeKilled)
				+ "}";
			*/

			Json::Value responseBodyRoot;

			string field = "ingestionJobKey";
			responseBodyRoot[field] = ingestionJobKey;

			field = "encodingJobKey";
			responseBodyRoot[field] = encodingJobKey;

			field = "pid";
			responseBodyRoot[field] = pidToBeKilled;

			responseBody = JSONUtils::toString(responseBodyRoot);
		}

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
			newInputsRoot = JSONUtils::toJson(
				-1, encodingJobKey, requestBody);
		}
		catch(runtime_error e)
		{
			_logger->error(__FILEREF__ + e.what());

			sendError(request, 500, e.what());

			return;
		}

		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>	selectedLiveProxy;

			for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			{
				if (liveProxy->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					selectedLiveProxy = liveProxy;

					break;
				}
			}

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
					// 2023-02-18: using SIGQUIT, the process was not stopped, it worked with SIGTERM
					//	SIGTERM now is managed by FFMpeg.cpp too
					_logger->info(__FILEREF__ + "ProcessUtility::termProcess"
						+ ", ingestionJobKey: " + to_string(selectedLiveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", selectedLiveProxy->_childPid: " + to_string(selectedLiveProxy->_childPid)
					);
					// ProcessUtility::quitProcess(selectedLiveProxy->_childPid);
					ProcessUtility::termProcess(selectedLiveProxy->_childPid);
				}
				catch(runtime_error e)
				{
					string errorMessage = string("ProcessUtility::termProcess failed")
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

			responseBody = JSONUtils::toString(responseBodyRoot);
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
		shared_ptr<FFMPEGEncoderBase::EncodingCompleted>    selectedEncodingCompleted;

		shared_ptr<FFMPEGEncoderBase::Encoding>    selectedEncoding;
		bool                    encodingFound = false;

		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>	selectedLiveProxy;
		bool					liveProxyFound = false;

		{
			lock_guard<mutex> locker(*_encodingCompletedMutex);

			map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>::iterator it =
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

				for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding: *_encodingsCapability)
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

				for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
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
        shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string requestBody)
{
    try
    {
		EncodeContent encodeContent(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		encodeContent.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::overlayImageOnVideoThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string requestBody)
{
    try
    {
		OverlayImageOnVideo overlayImageOnVideo(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		overlayImageOnVideo.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::overlayTextOnVideoThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string requestBody)
{
    try
    {
		OverlayTextOnVideo overlayTextOnVideo(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		overlayTextOnVideo.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::generateFramesThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string requestBody)
{
    try
    {
		GenerateFrames generateFrames(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		generateFrames.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());
	}
}

void FFMPEGEncoder::slideShowThread(
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string requestBody)
{
    try
    {
		SlideShow slideShow(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		slideShow.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());
    }
}

void FFMPEGEncoder::videoSpeedThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string requestBody)
{
    try
    {
		VideoSpeed videoSpeed(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		videoSpeed.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::addSilentAudioThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string requestBody)
{
    try
    {
		AddSilentAudio addSilentAudio(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		addSilentAudio.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::pictureInPictureThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string requestBody)
{
    try
    {
		PictureInPicture pictureInPicture(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		pictureInPicture.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::introOutroOverlayThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string requestBody)
{
    try
    {
		IntroOutroOverlay introOutroOverlay(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		introOutroOverlay.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::cutFrameAccurateThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string requestBody)
{
    try
    {
		CutFrameAccurate cutFrameAccurate(encoding, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		cutFrameAccurate.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::liveRecorderThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording,
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string requestBody)
{
    try
    {
		LiveRecorder liveRecorder(liveRecording, ingestionJobKey, encodingJobKey,                                    
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger,
			_tvChannelsPortsMutex, _tvChannelPort_CurrentOffset);
		liveRecorder.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(FFMpegURLForbidden e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(FFMpegURLNotFound e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::liveProxyThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxyData,
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string requestBody)
{
    try
    {
		LiveProxy liveProxy(liveProxyData, ingestionJobKey, encodingJobKey,
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger,
			_tvChannelsPortsMutex, _tvChannelPort_CurrentOffset);
		liveProxy.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(FFMpegURLForbidden e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(FFMpegURLNotFound e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::liveGridThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxyData,
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string requestBody)
{
    try
    {
		LiveGrid liveGrid(liveProxyData, ingestionJobKey, encodingJobKey,
			_configuration, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		liveGrid.encodeContent(requestBody);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		_logger->error(__FILEREF__ + e.what());
    }
    catch(FFMpegURLForbidden e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(FFMpegURLNotFound e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + e.what());

        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::encodingCompletedRetention()
{

	lock_guard<mutex> locker(*_encodingCompletedMutex);

	chrono::system_clock::time_point start = chrono::system_clock::now();

	for(map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>::iterator it = _encodingCompletedMap->begin();
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

