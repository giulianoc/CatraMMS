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
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/System.h"
#include "catralibraries/Convert.h"
#include "catralibraries/FileIO.h"
#include "catralibraries/DateTime.h"
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
    
		string logPathName =  configuration["log"]["encoder"].get("pathName", "XXX").asString();
		bool stdout =  JSONUtils::asBool(configuration["log"]["encoder"], "stdout", false);
    
		std::vector<spdlog::sink_ptr> sinks;
		auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logPathName.c_str(), 11, 20);
		sinks.push_back(dailySink);
		if (stdout)
		{
			auto stdoutSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sinks.push_back(stdoutSink);
		}
		auto logger = std::make_shared<spdlog::logger>("Encoder", begin(sinks), end(sinks));
    
		// shared_ptr<spdlog::logger> logger = spdlog::stdout_logger_mt("API");
		// shared_ptr<spdlog::logger> logger = spdlog::daily_logger_mt("API", logPathName.c_str(), 11, 20);
    
		// trigger flush if the log severity is error or higher
		logger->flush_on(spdlog::level::trace);
    
		string logLevel =  configuration["log"]["encoder"].get("level", "XXX").asString();
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

		/*
		* FFMPEGEncoder has not to have access to DB
		*
		size_t dbPoolSize = configuration["database"].get("ffmpegEncoderPoolSize", 5).asInt();                    
		logger->info(__FILEREF__ + "Configuration item"
			+ ", database->poolSize: " + to_string(dbPoolSize)
		);
		logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, dbPoolSize, logger);
		*/

		MMSStorage::createDirectories(configuration, logger);
		/*
		{
			// here the MMSStorage is instantiated just because it will create
			// the local directories of the transcoder
			logger->info(__FILEREF__ + "Creating MMSStorage"
				);
			shared_ptr<MMSStorage> mmsStorage = make_shared<MMSStorage>(
				configuration, logger);
		}
		*/

		FCGX_Init();

		int threadsNumber = JSONUtils::asInt(configuration["ffmpeg"], "encoderThreadsNumber", 1);
		logger->info(__FILEREF__ + "Configuration item"
			+ ", ffmpeg->encoderThreadsNumber: " + to_string(threadsNumber)
		);

		mutex fcgiAcceptMutex;

		// here is allocated all it is shared among FFMPEGEncoder threads
		mutex encodingMutex;
		vector<shared_ptr<Encoding>> encodingsCapability;

		mutex liveProxyMutex;
		vector<shared_ptr<LiveProxyAndGrid>> liveProxiesCapability;

		mutex liveRecordingMutex;
		vector<shared_ptr<LiveRecording>> liveRecordingsCapability;

		mutex encodingCompletedMutex;
		map<int64_t, shared_ptr<EncodingCompleted>> encodingCompletedMap;
		chrono::system_clock::time_point lastEncodingCompletedCheck;
		{
			int maxEncodingsCapability =  JSONUtils::asInt(configuration["ffmpeg"], "maxEncodingsCapability", 0);
			logger->info(__FILEREF__ + "Configuration item"
				+ ", ffmpeg->maxEncodingsCapability: " + to_string(maxEncodingsCapability)
			);

			for (int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<Encoding>    encoding = make_shared<Encoding>();
				encoding->_running   = false;
				encoding->_childPid		= 0;
				encoding->_ffmpeg   = make_shared<FFMpeg>(configuration, logger);

				encodingsCapability.push_back(encoding);
			}

			int maxLiveProxiesCapability =  JSONUtils::asInt(configuration["ffmpeg"], "maxLiveProxiesCapability", 0);
			logger->info(__FILEREF__ + "Configuration item"
				+ ", ffmpeg->maxLiveProxiesCapability: " + to_string(maxLiveProxiesCapability)
			);

			for (int liveProxyIndex = 0; liveProxyIndex < maxLiveProxiesCapability; liveProxyIndex++)
			{
				shared_ptr<LiveProxyAndGrid>    liveProxy = make_shared<LiveProxyAndGrid>();
				liveProxy->_running   = false;
				liveProxy->_ingestionJobKey		= 0;
				liveProxy->_childPid		= 0;
				liveProxy->_ffmpeg   = make_shared<FFMpeg>(configuration, logger);

				liveProxiesCapability.push_back(liveProxy);
			}

			int maxLiveRecordingsCapability =  JSONUtils::asInt(configuration["ffmpeg"], "maxLiveRecordingsCapability", 0);
			logger->info(__FILEREF__ + "Configuration item"
				+ ", ffmpeg->maxLiveRecordingsCapability: " + to_string(maxEncodingsCapability)
			);

			for (int liveRecordingIndex = 0; liveRecordingIndex < maxLiveRecordingsCapability; liveRecordingIndex++)
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

		vector<shared_ptr<FFMPEGEncoder>> ffmpegEncoders;
		vector<thread> ffmpegEncoderThreads;

		for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
		{
			shared_ptr<FFMPEGEncoder> ffmpegEncoder = make_shared<FFMPEGEncoder>(configuration, 
				&fcgiAcceptMutex,
				&encodingMutex,
				&encodingsCapability,
				&liveProxyMutex,
				&liveProxiesCapability,
				&liveRecordingMutex,
				&liveRecordingsCapability,
				&encodingCompletedMutex,
				&encodingCompletedMap,
				&lastEncodingCompletedCheck,
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

			thread monitor(&FFMPEGEncoder::monitorThread, ffmpegEncoders[0]);

			ffmpegEncoderThreads[0].join();
        
			ffmpegEncoders[0]->stopLiveRecorderChunksIngestionThread();
			ffmpegEncoders[0]->stopMonitorThread();
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

FFMPEGEncoder::FFMPEGEncoder(Json::Value configuration, 
        mutex* fcgiAcceptMutex,
		mutex* encodingMutex,
		vector<shared_ptr<Encoding>>* encodingsCapability,
		mutex* liveProxyMutex,
		vector<shared_ptr<LiveProxyAndGrid>>* liveProxiesCapability,
		mutex* liveRecordingMutex,
		vector<shared_ptr<LiveRecording>>* liveRecordingsCapability,
		mutex* encodingCompletedMutex,
		map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,
		chrono::system_clock::time_point* lastEncodingCompletedCheck,
        shared_ptr<spdlog::logger> logger)
    : APICommon(configuration, 
        fcgiAcceptMutex,
        logger) 
{
    _monitorCheckInSeconds =  JSONUtils::asInt(_configuration["ffmpeg"], "monitorCheckInSeconds", 5);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->monitorCheckInSeconds: " + to_string(_monitorCheckInSeconds)
    );

    _liveRecorderChunksIngestionCheckInSeconds =  JSONUtils::asInt(_configuration["ffmpeg"], "liveRecorderChunksIngestionCheckInSeconds", 5);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveRecorderChunksIngestionCheckInSeconds: " + to_string(_liveRecorderChunksIngestionCheckInSeconds)
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
	/*
    _mmsAPIUser = _configuration["api"].get("user", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->user: " + _mmsAPIUser
    );
    _mmsAPIPassword = _configuration["api"].get("password", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->password: " + "..."
    );
	*/
    _mmsAPIIngestionURI = _configuration["api"].get("ingestionURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->ingestionURI: " + _mmsAPIIngestionURI
    );
    _mmsAPITimeoutInSeconds = JSONUtils::asInt(_configuration["api"], "timeoutInSeconds", 120);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->timeoutInSeconds: " + to_string(_mmsAPITimeoutInSeconds)
    );

	_encodingMutex = encodingMutex;
	_encodingsCapability = encodingsCapability;

	_liveProxyMutex = liveProxyMutex;
	_liveProxiesCapability = liveProxiesCapability;

	_liveRecordingMutex = liveRecordingMutex;
	_liveRecordingsCapability = liveRecordingsCapability;

	_monitorThreadShutdown = false;
	_liveRecorderChunksIngestionThreadShutdown = false;

	_encodingCompletedMutex = encodingCompletedMutex;
	_encodingCompletedMap = encodingCompletedMap;
	_lastEncodingCompletedCheck = lastEncodingCompletedCheck;

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
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool, bool, bool,bool,bool,bool,bool>& userKeyWorkspaceAndFlags,
		string apiKey,
        unsigned long contentLength,
        string requestBody,
        unordered_map<string, string>& requestDetails
)
{
	chrono::system_clock::time_point start = chrono::system_clock::now();

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
        
        lock_guard<mutex> locker(*_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: *_encodingsCapability)
        {
            if (!encoding->_running)
            {
                encodingFound = true;
                selectedEncoding = encoding;
                
                break;
            }
        }

        if (!encodingFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingAvailable().what();
            
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(noEncodingAvailableMessage);
            return;
        }
        
        try
        {            
            selectedEncoding->_running = true;
            selectedEncoding->_childPid = 0;
            
            _logger->info(__FILEREF__ + "Creating encodeContent thread"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread encodeContentThread(&FFMPEGEncoder::encodeContent, this, selectedEncoding, encodingJobKey, requestBody);
            encodeContentThread.detach();
        }
        catch(exception e)
        {
            selectedEncoding->_running = false;
            selectedEncoding->_childPid = 0;

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
        
        lock_guard<mutex> locker(*_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: *_encodingsCapability)
        {
            if (!encoding->_running)
            {
                encodingFound = true;
                selectedEncoding = encoding;
                
                break;
            }
        }

        if (!encodingFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingAvailable().what();
            
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(noEncodingAvailableMessage);
            return;
        }
        
        try
        {            
            selectedEncoding->_running = true;
            selectedEncoding->_childPid = 0;

            _logger->info(__FILEREF__ + "Creating encodeContent thread"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread overlayImageOnVideoThread(&FFMPEGEncoder::overlayImageOnVideo, this, selectedEncoding, encodingJobKey, requestBody);
            overlayImageOnVideoThread.detach();
        }
        catch(exception e)
        {
            selectedEncoding->_running = false;
            selectedEncoding->_childPid = 0;

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
        
        lock_guard<mutex> locker(*_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: *_encodingsCapability)
        {
            if (!encoding->_running)
            {
                encodingFound = true;
                selectedEncoding = encoding;
                
                break;
            }
        }

        if (!encodingFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingAvailable().what();
            
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(noEncodingAvailableMessage);
            return;
        }
        
        try
        {            
            selectedEncoding->_running = true;
            selectedEncoding->_childPid = 0;

            _logger->info(__FILEREF__ + "Creating encodeContent thread"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread overlayTextOnVideoThread(&FFMPEGEncoder::overlayTextOnVideo, this, selectedEncoding, encodingJobKey, requestBody);
            overlayTextOnVideoThread.detach();
        }
        catch(exception e)
        {
            selectedEncoding->_running = false;
            selectedEncoding->_childPid = 0;
            
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
        
        lock_guard<mutex> locker(*_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: *_encodingsCapability)
        {
            if (!encoding->_running)
            {
                encodingFound = true;
                selectedEncoding = encoding;
                
                break;
            }
        }

        if (!encodingFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingAvailable().what();
            
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(noEncodingAvailableMessage);
            return;
        }
        
        try
        {            
            selectedEncoding->_running = true;
            selectedEncoding->_childPid = 0;

            _logger->info(__FILEREF__ + "Creating generateFrames thread"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread generateFramesThread(&FFMPEGEncoder::generateFrames, this, selectedEncoding, encodingJobKey, requestBody);
            generateFramesThread.detach();
        }
        catch(exception e)
        {
            selectedEncoding->_running = false;
            selectedEncoding->_childPid = 0;

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
        
        lock_guard<mutex> locker(*_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: *_encodingsCapability)
        {
            if (!encoding->_running)
            {
                encodingFound = true;
                selectedEncoding = encoding;
                
                break;
            }
        }

        if (!encodingFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingAvailable().what();
            
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(noEncodingAvailableMessage);
            return;
        }
        
        try
        {            
            selectedEncoding->_running = true;
            selectedEncoding->_childPid = 0;

            _logger->info(__FILEREF__ + "Creating slideShow thread"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread slideShowThread(&FFMPEGEncoder::slideShow, this, selectedEncoding, encodingJobKey, requestBody);
            slideShowThread.detach();
        }
        catch(exception e)
        {
            selectedEncoding->_running = false;
            selectedEncoding->_childPid = 0;

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
        
        lock_guard<mutex> locker(*_liveRecordingMutex);

        shared_ptr<LiveRecording>	selectedLiveRecording;
        bool						liveRecordingFound = false;
        for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
        {
            if (!liveRecording->_running)
            {
                liveRecordingFound = true;
                selectedLiveRecording = liveRecording;
                
                break;
            }
        }

        if (!liveRecordingFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingAvailable().what();
            
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(noEncodingAvailableMessage);
            return;
        }
        
        try
        {            
            selectedLiveRecording->_running = true;
            selectedLiveRecording->_childPid = 0;

            _logger->info(__FILEREF__ + "Creating liveRecorder thread"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread liveRecorderThread(&FFMPEGEncoder::liveRecorder, this, selectedLiveRecording, encodingJobKey, requestBody);
            liveRecorderThread.detach();
        }
        catch(exception e)
        {
            selectedLiveRecording->_running = false;
            selectedLiveRecording->_childPid = 0;

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
    else if (method == "liveProxy" || method == "liveGrid")
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
        
        lock_guard<mutex> locker(*_liveProxyMutex);

		// look for a free LiveProxyAndGrid structure
        shared_ptr<LiveProxyAndGrid>    selectedLiveProxy;
        bool                    liveProxyFound = false;
        for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
        {
            if (!liveProxy->_running)
            {
                liveProxyFound = true;
                selectedLiveProxy = liveProxy;
                
                break;
            }
        }

        if (!liveProxyFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingAvailable().what();
            
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(noEncodingAvailableMessage);
            return;
        }
        
        try
        {
            selectedLiveProxy->_running = true;
            selectedLiveProxy->_childPid = 0;

			if (method == "liveProxy")
			{
				_logger->info(__FILEREF__ + "Creating liveProxy thread"
					+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread liveProxyThread(&FFMPEGEncoder::liveProxy, this, selectedLiveProxy, encodingJobKey, requestBody);
				liveProxyThread.detach();
			}
			else // if (method == "liveGrid")
			{
				_logger->info(__FILEREF__ + "Creating liveGrid thread"
					+ ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
					+ ", requestBody: " + requestBody
				);
				thread liveGridThread(&FFMPEGEncoder::liveGrid, this, selectedLiveProxy, encodingJobKey, requestBody);
				liveGridThread.detach();
			}
        }
        catch(exception e)
        {
            selectedLiveProxy->_running = false;
            selectedLiveProxy->_childPid = 0;
            
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
        
        lock_guard<mutex> locker(*_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: *_encodingsCapability)
        {
            if (!encoding->_running)
            {
                encodingFound = true;
                selectedEncoding = encoding;
                
                break;
            }
        }

        if (!encodingFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingAvailable().what();
            
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(noEncodingAvailableMessage);
            return;
        }
        
        try
        {            
            selectedEncoding->_running = true;
            selectedEncoding->_childPid = 0;

            _logger->info(__FILEREF__ + "Creating encodeContent thread"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread videoSpeedThread(&FFMPEGEncoder::videoSpeed, this, selectedEncoding, encodingJobKey, requestBody);
            videoSpeedThread.detach();
        }
        catch(exception e)
        {
            selectedEncoding->_running = false;
            selectedEncoding->_childPid = 0;
            
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
        
        lock_guard<mutex> locker(*_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: *_encodingsCapability)
        {
            if (!encoding->_running)
            {
                encodingFound = true;
                selectedEncoding = encoding;
                
                break;
            }
        }

        if (!encodingFound)
        {
            string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey)
				+ ", " + NoEncodingAvailable().what();
            
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            // throw runtime_error(noEncodingAvailableMessage);
            return;
        }
        
        try
        {            
            selectedEncoding->_running = true;
            selectedEncoding->_childPid = 0;

            _logger->info(__FILEREF__ + "Creating encodeContent thread"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread pictureInPictureThread(&FFMPEGEncoder::pictureInPicture, this, selectedEncoding, encodingJobKey, requestBody);
            pictureInPictureThread.detach();
        }
        catch(exception e)
        {
            selectedEncoding->_running = false;
            selectedEncoding->_childPid = 0;
            
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
        
		bool                    encodingFound = false;
		shared_ptr<Encoding>    selectedEncoding;

		bool                    liveProxyFound = false;
		shared_ptr<LiveProxyAndGrid>	selectedLiveProxy;

		bool                    liveRecordingFound = false;
		shared_ptr<LiveRecording>    selectedLiveRecording;

		bool                    encodingCompleted = false;
		shared_ptr<EncodingCompleted>    selectedEncodingCompleted;

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

		if (!encodingCompleted)
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

				if (!liveProxyFound)
				{
					lock_guard<mutex> locker(*_liveRecordingMutex);

					for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
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

		_logger->info(__FILEREF__ + "Encoding Status"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodingFound: " + to_string(encodingFound)
				+ ", liveProxyFound: " + to_string(liveProxyFound)
				+ ", liveRecordingFound: " + to_string(liveRecordingFound)
				+ ", encodingCompleted: " + to_string(encodingCompleted)
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

		if (!encodingCompleted)
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
			/*
			string errorMessage = method + ": " + FFMpegEncodingStatusNotAvailable().what()
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingCompleted: " + to_string(encodingCompleted)
					;
			_logger->info(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			// throw e;
			return;
			*/
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

			for (shared_ptr<Encoding> encoding: *_encodingsCapability)
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
			lock_guard<mutex> locker(*_liveProxyMutex);

			for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			{
				if (liveProxy->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					pidToBeKilled = liveProxy->_childPid;
               
					break;
				}
			}
		}

		if (!encodingFound)
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
			{
				if (liveRecording->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					pidToBeKilled = liveRecording->_childPid;
               
					break;
				}
			}
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
			ProcessUtility::killProcess(pidToBeKilled);
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
    else
    {
        string errorMessage = string("No API is matched")
            + ", requestURI: " +requestURI
            + ", requestMethod: " +requestMethod;
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 400, errorMessage);

        throw runtime_error(errorMessage);
    }

	chrono::system_clock::time_point endRequest = chrono::system_clock::now();

	if (chrono::system_clock::now() - *_lastEncodingCompletedCheck >=
			chrono::seconds(_encodingCompletedRetentionInSeconds))
	{
		*_lastEncodingCompletedCheck = chrono::system_clock::now();
		encodingCompletedRetention();
	}

	chrono::system_clock::time_point endEncodingCompletedRetention = chrono::system_clock::now();

	_logger->info(__FILEREF__ + "FFMPEGEncoder request"
		+ ", method: " + method
		+ ", @MMS statistics@ - duration request processing (secs): @"
			+ to_string(chrono::duration_cast<chrono::seconds>(endRequest - start).count()) + "@"
		+ ", @MMS statistics@ - duration encodingCompleted retention processing (secs): @"
			+ to_string(chrono::duration_cast<chrono::seconds>(endEncodingCompletedRetention - endRequest).count()) + "@"
	);
}

void FFMPEGEncoder::encodeContent(
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
        encoding->_encodingJobKey = encodingJobKey;
		encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent(encodingJobKey);

        /*
        {
            "mmsSourceAssetPathName": "...",
            "durationInMilliSeconds": 111,
            "encodedFileName": "...",
            "stagingEncodedAssetPathName": "...",
            "encodingProfileDetails": {
                ....
            },
            "contentType": "...",
            "physicalPathKey": 1111,
            "workspaceDirectoryName": "...",
            "relativePath": "...",
            "encodingJobKey": 1111,
            "ingestionJobKey": 1111,
        }
        */
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

        string mmsSourceAssetPathName = encodingMedatada.get("mmsSourceAssetPathName", "XXX").asString();
        int64_t durationInMilliSeconds = JSONUtils::asInt64(encodingMedatada, "durationInMilliSeconds", -1);
        // string encodedFileName = encodingMedatada.get("encodedFileName", "XXX").asString();
        string stagingEncodedAssetPathName = encodingMedatada.get("stagingEncodedAssetPathName", "XXX").asString();
		/*
        string encodingProfileDetails;
        {
            Json::StreamWriterBuilder wbuilder;
            
            encodingProfileDetails = Json::writeString(wbuilder, encodingMedatada["encodingProfileDetails"]);
        }
		*/
		Json::Value encodingProfileDetailsRoot = encodingMedatada["encodingProfileDetails"];
        MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(encodingMedatada.get("contentType", "XXX").asString());
        int64_t physicalPathKey = JSONUtils::asInt64(encodingMedatada, "physicalPathKey", -1);
        string workspaceDirectoryName = encodingMedatada.get("workspaceDirectoryName", "XXX").asString();
        string relativePath = encodingMedatada.get("relativePath", "XXX").asString();
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
                physicalPathKey,
                workspaceDirectoryName,
                relativePath,
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::overlayImageOnVideo(
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
        encoding->_encodingJobKey = encodingJobKey;
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::overlayTextOnVideo(
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
        encoding->_encodingJobKey = encodingJobKey;
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::generateFrames(
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
        encoding->_encodingJobKey = encodingJobKey;
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
    }
}

void FFMPEGEncoder::slideShow(
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
        encoding->_encodingJobKey = encodingJobKey;
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
        double durationOfEachSlideInSeconds = JSONUtils::asDouble(slideShowMedatada, "durationOfEachSlideInSeconds", 0);
        int outputFrameRate = JSONUtils::asInt(slideShowMedatada, "outputFrameRate", -1);
        string slideShowMediaPathName = slideShowMedatada.get("slideShowMediaPathName", "XXX").asString();
        
        vector<string> sourcePhysicalPaths;
        Json::Value sourcePhysicalPathsRoot(Json::arrayValue);
        sourcePhysicalPathsRoot = slideShowMedatada["sourcePhysicalPaths"];
        for (int sourcePhysicalPathIndex = 0; sourcePhysicalPathIndex < sourcePhysicalPathsRoot.size(); ++sourcePhysicalPathIndex)
        {
            string sourcePhysicalPathName = sourcePhysicalPathsRoot.get(sourcePhysicalPathIndex, "XXX").asString();

            sourcePhysicalPaths.push_back(sourcePhysicalPathName);
        }

        encoding->_ffmpeg->generateSlideshowMediaToIngest(ingestionJobKey, encodingJobKey,
                sourcePhysicalPaths, durationOfEachSlideInSeconds,
                outputFrameRate, slideShowMediaPathName,
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
    }
}

void FFMPEGEncoder::liveRecorder(
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

    try
    {
        liveRecording->_killedBecauseOfNotWorking = false;
        liveRecording->_encodingJobKey = encodingJobKey;
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
        string userAgent = liveRecorderMedatada.get("userAgent", "XXX").asString();

		// this is the global shared path where the chunks would be moved for the ingestion
        liveRecording->_stagingContentsPath = liveRecorderMedatada.get("stagingContentsPath", "XXX").asString();
        liveRecording->_segmentListFileName = liveRecorderMedatada.get("segmentListFileName", "XXX").asString();
        liveRecording->_recordedFileNamePrefix = liveRecorderMedatada.get("recordedFileNamePrefix", "XXX").asString();

		// _encodingParametersRoot has to be the last field to be set because liveRecorderChunksIngestion()
		//		checks this field is set before to see if there are chunks to be ingested
		liveRecording->_encodingParametersRoot = liveRecorderMedatada["encodingParametersRoot"];
		liveRecording->_liveRecorderParametersRoot = liveRecorderMedatada["liveRecorderParametersRoot"];
		liveRecording->_channelLabel =  liveRecording->_liveRecorderParametersRoot.get("ConfigurationLabel", "").asString();

		bool actAsServer =  liveRecording->_liveRecorderParametersRoot.get("ActAsServer", false).asBool();
		int listenTimeoutInSeconds = liveRecording->_liveRecorderParametersRoot.get("ListenTimeout", -1).asInt();

        // string liveURL = liveRecording->_encodingParametersRoot.get("liveURL", "XXX").asString();
        string liveURL = liveRecorderMedatada.get("liveURL", "").asString();
        time_t utcRecordingPeriodStart = JSONUtils::asInt64(liveRecording->_encodingParametersRoot, "utcRecordingPeriodStart", -1);
        time_t utcRecordingPeriodEnd = JSONUtils::asInt64(liveRecording->_encodingParametersRoot, "utcRecordingPeriodEnd", -1);
        int segmentDurationInSeconds = JSONUtils::asInt(liveRecording->_encodingParametersRoot, "segmentDurationInSeconds", -1);
        string outputFileFormat = liveRecording->_encodingParametersRoot.get("outputFileFormat", "XXX").asString();

		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName
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

		liveRecording->_recordingStart = chrono::system_clock::now();

		liveRecording->_ffmpeg->liveRecorder(
			liveRecording->_ingestionJobKey,
			encodingJobKey,
			liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
			liveRecording->_recordedFileNamePrefix,
			actAsServer,
			liveURL,
			listenTimeoutInSeconds,
			userAgent,
			utcRecordingPeriodStart,
			utcRecordingPeriodEnd,
			segmentDurationInSeconds,
			outputFileFormat,
			&(liveRecording->_childPid)
		);

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

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, liveRecording->_errorMessage, killedByUser,
				urlForbidden, urlNotFound);

		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
					exceptionInCaseOfError);
		}
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";

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
    }
    catch(FFMpegURLForbidden e)
    {
        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;

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
    }
    catch(FFMpegURLNotFound e)
    {
        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;

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
    }
    catch(runtime_error e)
    {
        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;

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
    }
    catch(exception e)
    {
        liveRecording->_running = false;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_ingestionJobKey		= 0;
        liveRecording->_childPid = 0;
		liveRecording->_channelLabel		= "";
		liveRecording->_killedBecauseOfNotWorking = false;

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

		if (FileIO::fileExisting(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName))
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", segmentListPathName: " + liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName
			);
			bool exceptionInCaseOfError = false;
			FileIO::remove(liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
					exceptionInCaseOfError);
		}
    }
}

void FFMPEGEncoder::liveRecorderChunksIngestionThread()
{

	while(!_liveRecorderChunksIngestionThreadShutdown)
	{
		try
		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
			{
				if (liveRecording->_running)
				{
					_logger->info(__FILEREF__ + "liveRecorder_processLastGeneratedLiveRecorderFiles ..."
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
					);

					chrono::system_clock::time_point now = chrono::system_clock::now();

					try
					{
						if (liveRecording->_encodingParametersRoot != Json::nullValue)
						{
							bool highAvailability;
							bool main;
							int segmentDurationInSeconds;
							string outputFileFormat;
							{
								string field = "highAvailability";
								highAvailability = JSONUtils::asBool(liveRecording->_encodingParametersRoot, field, false);

								field = "main";
								main = JSONUtils::asBool(liveRecording->_encodingParametersRoot, field, false);

								field = "segmentDurationInSeconds";
								segmentDurationInSeconds = JSONUtils::asInt(liveRecording->_encodingParametersRoot, field, 0);

								field = "outputFileFormat";                                                                
								outputFileFormat = liveRecording->_encodingParametersRoot.get(field, "XXX").asString();                   
							}

							pair<string, int> lastRecordedAssetInfo = liveRecorder_processLastGeneratedLiveRecorderFiles(
								liveRecording->_ingestionJobKey,
								liveRecording->_encodingJobKey,
								highAvailability, main, segmentDurationInSeconds, outputFileFormat,                                                                              
								liveRecording->_encodingParametersRoot,
								liveRecording->_liveRecorderParametersRoot,

								liveRecording->_transcoderStagingContentsPath,
								liveRecording->_stagingContentsPath,
								liveRecording->_segmentListFileName,
								liveRecording->_recordedFileNamePrefix,
								liveRecording->_lastRecordedAssetFileName,
								liveRecording->_lastRecordedAssetDurationInSeconds);

							liveRecording->_lastRecordedAssetFileName			= lastRecordedAssetInfo.first;
							liveRecording->_lastRecordedAssetDurationInSeconds	= lastRecordedAssetInfo.second;
						}
					}
					catch(runtime_error e)
					{
						string errorMessage = string ("liveRecorder_processLastGeneratedLiveRecorderFiles failed")
							+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}
					catch(exception e)
					{
						string errorMessage = string ("liveRecorder_processLastGeneratedLiveRecorderFiles failed")
							+ ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
							+ ", liveRecording->_encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
							+ ", e.what(): " + e.what()
						;

						_logger->error(__FILEREF__ + errorMessage);
					}

					_logger->info(__FILEREF__ + "liveRecorder_processLastGeneratedLiveRecorderFiles"
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
						+ ", @MMS statistics@ - elapsed time: @" + to_string(
							chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - now).count()
						) + "@"
					);
				}
			}
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

pair<string, int> FFMPEGEncoder::liveRecorder_processLastGeneratedLiveRecorderFiles(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	bool highAvailability, bool main, int segmentDurationInSeconds, string outputFileFormat,
	Json::Value encodingParametersRoot,
	Json::Value liveRecorderParametersRoot,
	string transcoderStagingContentsPath,
	string stagingContentsPath,
	string segmentListFileName,
	string recordedFileNamePrefix,
	string lastRecordedAssetFileName,
	int lastRecordedAssetDurationInSeconds)
{

	// it is assigned to lastRecordedAssetFileName because in case no new files are present,
	// the same lastRecordedAssetFileName has to be returned
	string newLastRecordedAssetFileName = lastRecordedAssetFileName;
	int newLastRecordedAssetDurationInSeconds = lastRecordedAssetDurationInSeconds;
    try
    {
		_logger->info(__FILEREF__ + "liveRecorder_processLastGeneratedLiveRecorderFiles"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", highAvailability: " + to_string(highAvailability)
			+ ", main: " + to_string(main)
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
				uniqueName = to_string(JSONUtils::asInt64(encodingParametersRoot, "confKey", 0));
				uniqueName += " - ";
				uniqueName += to_string(utcCurrentRecordedFileCreationTime);
			}

			// UserData
			Json::Value userDataRoot;
			{
				if (JSONUtils::isMetadataPresent(liveRecorderParametersRoot, "UserData"))
					userDataRoot = liveRecorderParametersRoot["UserData"];

				Json::Value mmsDataRoot;
				mmsDataRoot["dataType"] = "liveRecordingChunk";
				mmsDataRoot["liveURLConfKey"] = JSONUtils::asInt64(encodingParametersRoot, "confKey", 0);
				mmsDataRoot["main"] = main;
				if (!highAvailability)
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
				// ConfigurationLabel is the label associated to the live URL
				addContentTitle = liveRecorderParametersRoot.get("ConfigurationLabel", "").asString();

				addContentTitle += " - ";

				{
					tm		tmDateTime;
					char	strCurrentRecordedFileTime [64];

					// from utc to local time
					localtime_r (&utcCurrentRecordedFileCreationTime, &tmDateTime);

					sprintf (strCurrentRecordedFileTime,
						"%04d-%02d-%02d %02d:%02d:%02d",
						tmDateTime. tm_year + 1900,
						tmDateTime. tm_mon + 1,
						tmDateTime. tm_mday,
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

				if (!main)
					addContentTitle += " (BCK)";
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
						addContentTitle, uniqueName, highAvailability, userDataRoot, outputFileFormat,
						liveRecorderParametersRoot);
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
        _logger->error(__FILEREF__ + "liveRecorder_processLastGeneratedLiveRecorderFiles failed"
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
        _logger->error(__FILEREF__ + "liveRecorder_processLastGeneratedLiveRecorderFiles failed"
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
	bool highAvailability,
	Json::Value userDataRoot,
	string fileFormat,
	Json::Value liveRecorderParametersRoot)
{
	try
	{
		// moving chunk from transcoder staging path to shared staging path
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

		if (!highAvailability)
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

void FFMPEGEncoder::liveProxy(
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

    try
    {
		liveProxy->_killedBecauseOfNotWorking = false;
		liveProxy->_errorMessage = "";
        liveProxy->_encodingJobKey = encodingJobKey;
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

		string liveURL = liveProxyMetadata.get("liveURL", "").asString();
		string userAgent = liveProxyMetadata.get("userAgent", "").asString();
		int maxWidth = JSONUtils::asInt(liveProxyMetadata, "maxWidth", -1);
		string otherInputOptions = liveProxyMetadata.get("otherInputOptions", "").asString();
		string otherOutputOptions = liveProxyMetadata.get("otherOutputOptions", "").asString();
		liveProxy->_outputType = liveProxyMetadata.get("outputType", "").asString();
		int segmentDurationInSeconds = JSONUtils::asInt(liveProxyMetadata, "segmentDurationInSeconds", 10);
		int playlistEntriesNumber = JSONUtils::asInt(liveProxyMetadata, "playlistEntriesNumber", 6);
		liveProxy->_channelLabel = liveProxyMetadata.get("configurationLabel", "").asString();
		string manifestDirectoryPath = liveProxyMetadata.get("manifestDirectoryPath", "").asString();
		string manifestFileName = liveProxyMetadata.get("manifestFileName", "").asString();

		liveProxy->_ingestedParametersRoot = liveProxyMetadata["liveProxyIngestedParametersRoot"];
		string rtmpUrl = liveProxy->_ingestedParametersRoot.get("RtmpUrl", "").asString();
		bool actAsServer = liveProxy->_ingestedParametersRoot.get("ActAsServer", false).asBool();
		int listenTimeoutInSeconds = liveProxy->_ingestedParametersRoot.get("ListenTimeout", -1).asInt();

		Json::Value encodingProfileDetailsRoot = Json::nullValue;
        MMSEngineDBFacade::ContentType contentType;
		int64_t encodingProfileKey;
        if (JSONUtils::isMetadataPresent(liveProxyMetadata, "encodingProfileDetails"))
		{
			encodingProfileDetailsRoot = liveProxyMetadata["encodingProfileDetails"];
			contentType = MMSEngineDBFacade::toContentType(liveProxyMetadata.get("contentType", "").asString());
			encodingProfileKey = JSONUtils::asInt64(liveProxyMetadata, "encofingProfileKey", -1);
		}

		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_manifestFilePathNames.push_back(manifestDirectoryPath + "/" + manifestFileName);

		if (liveProxy->_outputType == "HLS" || liveProxy->_outputType == "DASH")
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

			liveProxy->_proxyStart = chrono::system_clock::now();

			liveProxy->_ffmpeg->liveProxyByHTTPStreaming(
				liveProxy->_ingestionJobKey,
				encodingJobKey,
				maxWidth,
				actAsServer,
				liveURL,
				listenTimeoutInSeconds,
				userAgent,
				otherOutputOptions,
				encodingProfileDetailsRoot,
                encodingProfileDetailsRoot != Json::nullValue ? contentType == MMSEngineDBFacade::ContentType::Video : false,
				liveProxy->_outputType,
				segmentDurationInSeconds,
				playlistEntriesNumber,
				manifestDirectoryPath,
				manifestFileName,
				&(liveProxy->_childPid));
		}
		else
		{
			liveProxy->_proxyStart = chrono::system_clock::now();

			liveProxy->_ffmpeg->liveProxyByStream(
				liveProxy->_ingestionJobKey,
				encodingJobKey,
				maxWidth,
				actAsServer,
				liveURL,
				listenTimeoutInSeconds,
				userAgent,
				otherInputOptions,
				otherOutputOptions,
				encodingProfileDetailsRoot,
                encodingProfileDetailsRoot != Json::nullValue ? contentType == MMSEngineDBFacade::ContentType::Video : false,
				rtmpUrl,
				&(liveProxy->_childPid));
		}

        liveProxy->_running = false;
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
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
		liveProxy->_channelLabel = "";
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
    }
    catch(FFMpegURLForbidden e)
    {
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(FFMpegURLNotFound e)
    {
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::liveGrid(
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
        liveProxy->_encodingJobKey = encodingJobKey;
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
		string userAgent = liveGridMetadata.get("userAgent", "").asString();
		Json::Value encodingProfileDetailsRoot = liveGridMetadata["encodingProfileDetails"];
		int gridColumns = JSONUtils::asInt(liveGridMetadata, "gridColumns", 0);
		int gridWidth = JSONUtils::asInt(liveGridMetadata, "gridWidth", 0);
		int gridHeight = JSONUtils::asInt(liveGridMetadata, "gridHeight", 0);
		liveProxy->_outputType = liveGridMetadata.get("outputType", "").asString();
		string srtURL = liveGridMetadata.get("srtURL", "").asString();
		int segmentDurationInSeconds = JSONUtils::asInt(liveGridMetadata, "segmentDurationInSeconds", 10);
		int playlistEntriesNumber = JSONUtils::asInt(liveGridMetadata, "playlistEntriesNumber", 6);
		string manifestDirectoryPath = liveGridMetadata.get("manifestDirectoryPath", "").asString();
		string manifestFileName = liveGridMetadata.get("manifestFileName", "").asString();
		liveProxy->_channelLabel = manifestFileName;

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

		// if (liveProxy->_outputType == "HLS") // || liveProxy->_outputType == "DASH")
		{
			if (liveProxy->_outputType == "HLS"
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
				liveProxy->_outputType,
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
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
		liveProxy->_channelLabel = "";
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
    }
    catch(FFMpegURLForbidden e)
    {
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(FFMpegURLNotFound e)
    {
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        liveProxy->_running = false;
		liveProxy->_ingestionJobKey = 0;
        liveProxy->_childPid = 0;
		liveProxy->_manifestFilePathNames.clear();
		liveProxy->_outputType = "";
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
			for (shared_ptr<LiveProxyAndGrid> liveProxy: *_liveProxiesCapability)
			{
				if (liveProxy->_running)
				{
					liveProxyAndGridRunningCounter++;

					_logger->info(__FILEREF__ + "liveProxyMonitor..."
						+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
					);

					chrono::system_clock::time_point now = chrono::system_clock::now();

					// First health check
					//		HLS/DASH:	kill if manifest file does not exist or was not updated in the last 30 seconds
					//		rtmp(Proxy)/SRT(Grid):	kill if it was found 'Non-monotonous DTS in output stream' and 'incorrect timestamps'
					if (liveProxy->_outputType == "HLS" || liveProxy->_outputType == "DASH")
					{
						try
						{
							// First health check (HLS/DASH) looking the manifests path name timestamp

							chrono::system_clock::time_point now = chrono::system_clock::now();
							int64_t liveProxyLiveTimeInMinutes =
								chrono::duration_cast<chrono::minutes>(now - liveProxy->_proxyStart).count();

							// check id done after 3 minutes LiveProxy started, in order to be sure
							// the manifest file was already created
							if (liveProxyLiveTimeInMinutes > 3)
							{
								for (string manifestFilePathName: liveProxy->_manifestFilePathNames)
								{
									bool liveProxyWorking = true;

									if(!FileIO::fileExisting(manifestFilePathName))
									{
										liveProxyWorking = false;

										_logger->error(__FILEREF__ + "liveProxyMonitor. Manifest file does not exist"
											+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
											+ ", manifestFilePathName: " + manifestFilePathName
										);
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
										}
									}

									if (!liveProxyWorking)
									{
										_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveProxyMonitor. Live Proxy is not working (manifest file is missing or was not updated). LiveProxy (ffmpeg) is killed in order to be started again"
											+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
											+ ", manifestFilePathName: " + manifestFilePathName
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
													" restarted because of 'manifest file is missing or was not updated'";
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

										break;
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
						try
						{
							// First health check (rtmp), looks the log and check there is no message like
							//	[flv @ 0x562afdc507c0] Non-monotonous DTS in output stream 0:1; previous: 95383372, current: 1163825; changing to 95383372. This may result in incorrect timestamps in the output file.
							//	This message causes proxy not working
							if (liveProxy->_ffmpeg->nonMonotonousDTSInOutputLog())
							{
								_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveProxyMonitor (rtmp). Live Proxy is logging 'Non-monotonous DTS in output stream/incorrect timestamps'. LiveProxy (ffmpeg) is killed in order to be started again"
									+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
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
											" restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps'";
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
					if (liveProxy->_outputType == "HLS" || liveProxy->_outputType == "DASH")
					{
						try
						{
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
							{
								for (string manifestFilePathName: liveProxy->_manifestFilePathNames)
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
													if (liveProxy->_outputType == "DASH"
															&&
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

										_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveProxyMonitor. Live Proxy is not working (no segments were generated). LiveProxy (ffmpeg) is killed in order to be started again"
											+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
											+ ", manifestFilePathName: " + manifestFilePathName
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
													" restarted because of 'no segments were generated'";
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

						try
						{
							// Second health check (HLS/DASH), looks if the frame is increasing
							int secondsToWaitBetweenSamples = 3;
							if (!liveProxy->_ffmpeg->isFrameIncreasing(secondsToWaitBetweenSamples))
							{
								_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveProxyMonitor (HLS/DASH). Live Proxy frame is not increasing'. LiveProxy (ffmpeg) is killed in order to be started again"
									+ ", ingestionJobKey: " + to_string(liveProxy->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(liveProxy->_encodingJobKey)
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
											" restarted because of 'frame is not increasing'";
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
											" restarted because of 'frame is not increasing'";
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
					if (liveProxy->_outputType == "HLS" || liveProxy->_outputType == "DASH")
					{
					}
					else
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
											" restarted because of 'HTTP error 403 Forbidden'";
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

			int liveRecordingRunningCounter = 0;
			int liveRecordingNotRunningCounter = 0;
			for (shared_ptr<LiveRecording> liveRecording: *_liveRecordingsCapability)
			{
				if (liveRecording->_running)
				{
					liveRecordingRunningCounter++;

					_logger->info(__FILEREF__ + "liveRecordingMonitor..."
						+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
					);

					chrono::system_clock::time_point now = chrono::system_clock::now();

					// First health check
					//		kill if 1840699_408620.liveRecorder.list file does not exist or was not updated in the last (2 * segment duration in secs) seconds
					try
					{
						// looking the manifests path name timestamp

						chrono::system_clock::time_point now = chrono::system_clock::now();
						int64_t liveRecordingLiveTimeInMinutes =
							chrono::duration_cast<chrono::minutes>(now - liveRecording->_recordingStart).count();

						int segmentDurationInSeconds;
						string field = "segmentDurationInSeconds";
						segmentDurationInSeconds = JSONUtils::asInt(
							liveRecording->_encodingParametersRoot, field, 0);

						// check is done after 5 minutes + segmentDurationInSeconds LiveRecording started,
						// in order to be sure the file was already created
						if (liveRecordingLiveTimeInMinutes > (segmentDurationInSeconds / 60) + 5)
						{
							string segmentListPathName = liveRecording->_transcoderStagingContentsPath
								+ liveRecording->_segmentListFileName;

							{
								bool liveRecordingWorking = true;

								if(!FileIO::fileExisting(segmentListPathName))
								{
									liveRecordingWorking = false;

									_logger->error(__FILEREF__ + "liveRecordingMonitor. Segment list file does not exist"
										+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
										+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
										+ ", segmentListPathName: " + segmentListPathName
									);
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
										liveRecordingWorking = false;

										_logger->error(__FILEREF__ + "liveRecordingMonitor. Segment list file was not updated "
											+ "in the last " + to_string(maxLastSegmentListFileUpdateInSeconds) + " seconds"
											+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
											+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
											+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
											+ ", segmentListPathName: " + segmentListPathName
											+ ", lastSegmentListFileUpdateInSeconds: " + to_string(lastSegmentListFileUpdateInSeconds) + " seconds ago"
										);
									}
								}

								if (!liveRecordingWorking)
								{
									_logger->error(__FILEREF__ + "ProcessUtility::killProcess. liveRecordingMonitor. Live Recording is not working (segment list file is missing or was not updated). LiveRecording (ffmpeg) is killed in order to be started again"
										+ ", ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
										+ ", encodingJobKey: " + to_string(liveRecording->_encodingJobKey)
										+ ", liveRecordingLiveTimeInMinutes: " + to_string(liveRecordingLiveTimeInMinutes)
										+ ", segmentListPathName: " + segmentListPathName
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
												" restarted because of 'segment list file is missing or was not updated'";
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

									break;
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
			string errorMessage = string ("monitor failed")
				+ ", e.what(): " + e.what()
			;

			_logger->error(__FILEREF__ + errorMessage);
		}
		catch(exception e)
		{
			string errorMessage = string ("monitor failed")
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

void FFMPEGEncoder::videoSpeed(
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
        encoding->_encodingJobKey = encodingJobKey;
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::pictureInPicture(
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
        encoding->_encodingJobKey = encodingJobKey;
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
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
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

	chrono::system_clock::time_point now = chrono::system_clock::now();

	for(map<int64_t, shared_ptr<EncodingCompleted>>::iterator it = _encodingCompletedMap->begin();
			it != _encodingCompletedMap->end(); )
	{
		if(now - (it->second->_timestamp) >= chrono::seconds(_encodingCompletedRetentionInSeconds))
			it = _encodingCompletedMap->erase(it);
		else
			it++;
	}

	_logger->info(__FILEREF__ + "encodingCompletedRetention"
			+ ", encodingCompletedMap size: " + to_string(_encodingCompletedMap->size())
			);
}

