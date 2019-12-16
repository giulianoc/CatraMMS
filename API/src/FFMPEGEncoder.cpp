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

#include <fstream>
#include <sstream>
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/System.h"
#include "catralibraries/Convert.h"
#include "catralibraries/FileIO.h"
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
    const char* configurationPathName = getenv("MMS_CONFIGPATHNAME");
    if (configurationPathName == nullptr)
    {
        cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;
        
        return 1;
    }
    
    Json::Value configuration = APICommon::loadConfigurationFile(configurationPathName);
    
    string logPathName =  configuration["log"]["encoder"].get("pathName", "XXX").asString();
    bool stdout =  configuration["log"]["encoder"].get("stdout", "XXX").asBool();
    
    std::vector<spdlog::sink_ptr> sinks;
    auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logPathName.c_str(), 11, 20);
    sinks.push_back(dailySink);
    if (stdout)
    {
        auto stdoutSink = spdlog::sinks::stdout_sink_mt::instance();
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

    size_t dbPoolSize = configuration["database"].get("ffmpegEncoderPoolSize", 5).asInt();
    logger->info(__FILEREF__ + "Configuration item"
        + ", database->poolSize: " + to_string(dbPoolSize)
    );
    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, dbPoolSize, logger);

	{
		// here the MMSStorage is instantiated just because it will create
		// the local directories of the transcoder
		logger->info(__FILEREF__ + "Creating MMSStorage"
			);
		shared_ptr<MMSStorage> mmsStorage = make_shared<MMSStorage>(
			configuration, mmsEngineDBFacade, logger);
	}

    FCGX_Init();

    mutex fcgiAcceptMutex;

    FFMPEGEncoder ffmpegEncoder(configuration, 
            mmsEngineDBFacade,
            &fcgiAcceptMutex,
            logger);

    return ffmpegEncoder();
}

FFMPEGEncoder::FFMPEGEncoder(Json::Value configuration, 
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        mutex* fcgiAcceptMutex,
        shared_ptr<spdlog::logger> logger)
    : APICommon(configuration, 
        mmsEngineDBFacade,
        fcgiAcceptMutex,
        logger) 
{
    _maxEncodingsCapability =  _configuration["ffmpeg"].get("maxEncodingsCapability", 0).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->maxEncodingsCapability: " + to_string(_maxEncodingsCapability)
    );

    _maxLiveProxiesCapability =  _configuration["ffmpeg"].get("maxLiveProxiesCapability", 0).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->maxLiveProxiesCapability: " + to_string(_maxLiveProxiesCapability)
    );

    _maxLiveRecordingsCapability =  _configuration["ffmpeg"].get("maxLiveRecordingsCapability", 0).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->maxLiveRecordingsCapability: " + to_string(_maxEncodingsCapability)
    );

    _liveRecorderChunksIngestionCheckInSeconds =  _configuration["ffmpeg"].get("liveRecorderChunksIngestionCheckInSeconds", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveRecorderChunksIngestionCheckInSeconds: " + to_string(_liveRecorderChunksIngestionCheckInSeconds)
    );

    _encodingCompletedRetentionInSeconds = _configuration["ffmpeg"].get("encodingCompletedRetentionInSeconds", 0).asInt();
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
    _mmsAPIPort = _configuration["api"].get("port", "").asInt();
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

    for (int encodingIndex = 0; encodingIndex < _maxEncodingsCapability; encodingIndex++)
    {
        shared_ptr<Encoding>    encoding = make_shared<Encoding>();
        encoding->_running   = false;
        encoding->_childPid		= 0;
        encoding->_ffmpeg   = make_shared<FFMpeg>(_configuration, _logger);

        _encodingsCapability.push_back(encoding);
    }

    for (int liveProxyIndex = 0; liveProxyIndex < _maxLiveProxiesCapability; liveProxyIndex++)
    {
        shared_ptr<LiveProxy>    liveProxy = make_shared<LiveProxy>();
        liveProxy->_running   = false;
        liveProxy->_childPid		= 0;
        liveProxy->_ffmpeg   = make_shared<FFMpeg>(_configuration, _logger);

        _liveProxiesCapability.push_back(liveProxy);
    }

    for (int liveRecordingIndex = 0; liveRecordingIndex < _maxLiveRecordingsCapability; liveRecordingIndex++)
    {
        shared_ptr<LiveRecording>    liveRecording = make_shared<LiveRecording>();
        liveRecording->_running   = false;
        liveRecording->_ingestionJobKey		= 0;
		liveRecording->_encodingParametersRoot = Json::nullValue;
        liveRecording->_childPid		= 0;
        liveRecording->_ffmpeg   = make_shared<FFMpeg>(_configuration, _logger);

        _liveRecordingsCapability.push_back(liveRecording);
    }

	{
		_liveRecorderChunksIngestionThreadShutdown = false;
		thread liveRecorderChunksIngestion(&FFMPEGEncoder::liveRecorderChunksIngestionThread, this);
		liveRecorderChunksIngestion.detach();
	}

	_lastEncodingCompletedCheck = chrono::system_clock::now();
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
        
        lock_guard<mutex> locker(_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
        
        lock_guard<mutex> locker(_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
        
        lock_guard<mutex> locker(_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
        
        lock_guard<mutex> locker(_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
        
        lock_guard<mutex> locker(_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
        
        lock_guard<mutex> locker(_liveRecordingMutex);

        shared_ptr<LiveRecording>	selectedLiveRecording;
        bool						liveRecordingFound = false;
        for (shared_ptr<LiveRecording> liveRecording: _liveRecordingsCapability)
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
    else if (method == "liveProxy")
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
        
        lock_guard<mutex> locker(_liveProxyMutex);

        shared_ptr<LiveProxy>    selectedLiveProxy;
        bool                    liveProxyFound = false;
        for (shared_ptr<LiveProxy> liveProxy: _liveProxiesCapability)
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

            _logger->info(__FILEREF__ + "Creating liveProxy thread"
                + ", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread liveProxyThread(&FFMPEGEncoder::liveProxy, this, selectedLiveProxy, encodingJobKey, requestBody);
            liveProxyThread.detach();
        }
        catch(exception e)
        {
            selectedLiveProxy->_running = false;
            selectedLiveProxy->_childPid = 0;
            
            _logger->error(__FILEREF__ + "liveProxyThread failed"
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
        
        lock_guard<mutex> locker(_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
        
        lock_guard<mutex> locker(_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
		shared_ptr<LiveProxy>	selectedLiveProxy;

		bool                    liveRecordingFound = false;
		shared_ptr<LiveRecording>    selectedLiveRecording;

		bool                    encodingCompleted = false;
		shared_ptr<EncodingCompleted>    selectedEncodingCompleted;

		{
			lock_guard<mutex> locker(_encodingCompletedMutex);

			map<int64_t, shared_ptr<EncodingCompleted>>::iterator it =
				_encodingCompletedMap.find(encodingJobKey);
			if (it != _encodingCompletedMap.end())
			{
				encodingCompleted = true;
				selectedEncodingCompleted = it->second;
			}
		}

		if (!encodingCompleted)
		{
			lock_guard<mutex> locker(_encodingMutex);

			for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
				lock_guard<mutex> locker(_liveProxyMutex);

				for (shared_ptr<LiveProxy> liveProxy: _liveProxiesCapability)
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
					lock_guard<mutex> locker(_liveRecordingMutex);

					for (shared_ptr<LiveRecording> liveRecording: _liveRecordingsCapability)
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
				responseBody = string("{ ")
					+ "\"encodingJobKey\": " + to_string(selectedEncodingCompleted->_encodingJobKey)
					+ ", \"pid\": 0 "
					+ ", \"killedByUser\": " + (selectedEncodingCompleted->_killedByUser ? "true" : "false")
					+ ", \"urlForbidden\": " + (selectedEncodingCompleted->_urlForbidden ? "true" : "false")
					+ ", \"urlNotFound\": " + (selectedEncodingCompleted->_urlNotFound ? "true" : "false")
					+ ", \"completedWithError\": " + (selectedEncodingCompleted->_completedWithError ? "true" : "false")
					+ ", \"encodingFinished\": true "
					+ "}";
			else if (encodingFound)
				responseBody = string("{ ")
					+ "\"encodingJobKey\": " + to_string(selectedEncoding->_encodingJobKey)
					+ ", \"pid\": " + to_string(selectedEncoding->_childPid)
					+ ", \"killedByUser\": false"
					+ ", \"urlForbidden\": false"
					+ ", \"urlNotFound\": false"
					+ ", \"encodingFinished\": " + (selectedEncoding->_running ? "false " : "true ")
					+ "}";
			else if (liveProxyFound)
				responseBody = string("{ ")
					+ "\"encodingJobKey\": " + to_string(selectedLiveProxy->_encodingJobKey)
					+ ", \"pid\": " + to_string(selectedLiveProxy->_childPid)
					+ ", \"killedByUser\": false"
					+ ", \"urlForbidden\": false"
					+ ", \"urlNotFound\": false"
					+ ", \"encodingFinished\": " + (selectedLiveProxy->_running ? "false " : "true ")
					+ "}";
			else // if (liveRecording)
				responseBody = string("{ ")
					+ "\"encodingJobKey\": " + to_string(selectedLiveRecording->_encodingJobKey)
					+ ", \"pid\": " + to_string(selectedLiveRecording->_childPid)
					+ ", \"killedByUser\": false"
					+ ", \"urlForbidden\": false"
					+ ", \"urlNotFound\": false"
					+ ", \"encodingFinished\": " + (selectedLiveRecording->_running ? "false " : "true ")
					+ "}";
        }

        sendSuccess(request, 200, responseBody);
    }
    else if (method == "encodingProgress")
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

		bool                    encodingCompleted = false;
		shared_ptr<EncodingCompleted>    selectedEncodingCompleted;

		shared_ptr<Encoding>    selectedEncoding;
		bool                    encodingFound = false;

		shared_ptr<LiveProxy>	selectedLiveProxy;
		bool					liveProxyFound = false;

		{
			lock_guard<mutex> locker(_encodingCompletedMutex);

			map<int64_t, shared_ptr<EncodingCompleted>>::iterator it =
				_encodingCompletedMap.find(encodingJobKey);
			if (it != _encodingCompletedMap.end())
			{
				encodingCompleted = true;
				selectedEncodingCompleted = it->second;
			}
		}

		if (!encodingCompleted)
		{
			lock_guard<mutex> locker(_encodingMutex);

			for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
				lock_guard<mutex> locker(_liveProxyMutex);

				for (shared_ptr<LiveProxy> liveProxy: _liveProxiesCapability)
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
				encodingProgress = selectedLiveProxy->_ffmpeg->getEncodingProgress();
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
			lock_guard<mutex> locker(_encodingMutex);

			for (shared_ptr<Encoding> encoding: _encodingsCapability)
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
			lock_guard<mutex> locker(_liveProxyMutex);

			for (shared_ptr<LiveProxy> liveProxy: _liveProxiesCapability)
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
			lock_guard<mutex> locker(_liveRecordingMutex);

			for (shared_ptr<LiveRecording> liveRecording: _liveRecordingsCapability)
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

		_logger->info(__FILEREF__ + "Found Encoding to kill"
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

	if (chrono::system_clock::now() - _lastEncodingCompletedCheck >=
			chrono::seconds(_encodingCompletedRetentionInSeconds))
	{
		_lastEncodingCompletedCheck = chrono::system_clock::now();
		encodingCompletedRetention();
	}
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
        int64_t durationInMilliSeconds = encodingMedatada.get("durationInMilliSeconds", -1).asInt64();
        // string encodedFileName = encodingMedatada.get("encodedFileName", "XXX").asString();
        string stagingEncodedAssetPathName = encodingMedatada.get("stagingEncodedAssetPathName", "XXX").asString();
        string encodingProfileDetails;
        {
            Json::StreamWriterBuilder wbuilder;
            
            encodingProfileDetails = Json::writeString(wbuilder, encodingMedatada["encodingProfileDetails"]);
        }
        MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(encodingMedatada.get("contentType", "XXX").asString());
        int64_t physicalPathKey = encodingMedatada.get("physicalPathKey", -1).asInt64();
        string workspaceDirectoryName = encodingMedatada.get("workspaceDirectoryName", "XXX").asString();
        string relativePath = encodingMedatada.get("relativePath", "XXX").asString();
        int64_t encodingJobKey = encodingMedatada.get("encodingJobKey", -1).asInt64();
        int64_t ingestionJobKey = encodingMedatada.get("ingestionJobKey", -1).asInt64();

		// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
        encoding->_ffmpeg->encodeContent(
                mmsSourceAssetPathName,
                durationInMilliSeconds,
                // encodedFileName,
                stagingEncodedAssetPathName,
                encodingProfileDetails,
                contentType == MMSEngineDBFacade::ContentType::Video,
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
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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
        int64_t videoDurationInMilliSeconds = overlayMedatada.get("videoDurationInMilliSeconds", -1).asInt64();
        string mmsSourceImageAssetPathName = overlayMedatada.get("mmsSourceImageAssetPathName", "XXX").asString();
        string imagePosition_X_InPixel = overlayMedatada.get("imagePosition_X_InPixel", "XXX").asString();
        string imagePosition_Y_InPixel = overlayMedatada.get("imagePosition_Y_InPixel", "XXX").asString();

        // string encodedFileName = overlayMedatada.get("encodedFileName", "XXX").asString();
        string stagingEncodedAssetPathName = overlayMedatada.get("stagingEncodedAssetPathName", "XXX").asString();
        int64_t encodingJobKey = overlayMedatada.get("encodingJobKey", -1).asInt64();
        int64_t ingestionJobKey = overlayMedatada.get("ingestionJobKey", -1).asInt64();

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
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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
        int64_t videoDurationInMilliSeconds = overlayTextMedatada.get("videoDurationInMilliSeconds", -1).asInt64();

        string text = overlayTextMedatada.get("text", "XXX").asString();
        
        string textPosition_X_InPixel;
        string field = "textPosition_X_InPixel";
        if (_mmsEngineDBFacade->isMetadataPresent(overlayTextMedatada, field))
            textPosition_X_InPixel = overlayTextMedatada.get(field, "XXX").asString();
        
        string textPosition_Y_InPixel;
        field = "textPosition_Y_InPixel";
        if (_mmsEngineDBFacade->isMetadataPresent(overlayTextMedatada, field))
            textPosition_Y_InPixel = overlayTextMedatada.get(field, "XXX").asString();
        
        string fontType;
        field = "fontType";
        if (_mmsEngineDBFacade->isMetadataPresent(overlayTextMedatada, field))
            fontType = overlayTextMedatada.get(field, "XXX").asString();

        int fontSize = -1;
        field = "fontSize";
        if (_mmsEngineDBFacade->isMetadataPresent(overlayTextMedatada, field))
            fontSize = overlayTextMedatada.get(field, -1).asInt();

        string fontColor;
        field = "fontColor";
        if (_mmsEngineDBFacade->isMetadataPresent(overlayTextMedatada, field))
            fontColor = overlayTextMedatada.get(field, "XXX").asString();

        int textPercentageOpacity = -1;
        field = "textPercentageOpacity";
        if (_mmsEngineDBFacade->isMetadataPresent(overlayTextMedatada, field))
            textPercentageOpacity = overlayTextMedatada.get(field, -1).asInt();

        bool boxEnable = false;
        field = "boxEnable";
        if (_mmsEngineDBFacade->isMetadataPresent(overlayTextMedatada, field))
            boxEnable = overlayTextMedatada.get(field, 0).asBool();

        string boxColor;
        field = "boxColor";
        if (_mmsEngineDBFacade->isMetadataPresent(overlayTextMedatada, field))
            boxColor = overlayTextMedatada.get(field, "XXX").asString();
        
        int boxPercentageOpacity = -1;
        field = "boxPercentageOpacity";
        if (_mmsEngineDBFacade->isMetadataPresent(overlayTextMedatada, field))
            boxPercentageOpacity = overlayTextMedatada.get("boxPercentageOpacity", -1).asInt();

        // string encodedFileName = overlayTextMedatada.get("encodedFileName", "XXX").asString();
        string stagingEncodedAssetPathName = overlayTextMedatada.get("stagingEncodedAssetPathName", "XXX").asString();
        int64_t encodingJobKey = overlayTextMedatada.get("encodingJobKey", -1).asInt64();
        int64_t ingestionJobKey = overlayTextMedatada.get("ingestionJobKey", -1).asInt64();

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
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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
        double startTimeInSeconds = generateFramesMedatada.get("startTimeInSeconds", -1).asDouble();
        int maxFramesNumber = generateFramesMedatada.get("maxFramesNumber", -1).asInt();
        string videoFilter = generateFramesMedatada.get("videoFilter", "XXX").asString();
        int periodInSeconds = generateFramesMedatada.get("periodInSeconds", -1).asInt();
        bool mjpeg = generateFramesMedatada.get("mjpeg", -1).asBool();
        int imageWidth = generateFramesMedatada.get("imageWidth", -1).asInt();
        int imageHeight = generateFramesMedatada.get("imageHeight", -1).asInt();
        int64_t ingestionJobKey = generateFramesMedatada.get("ingestionJobKey", -1).asInt64();
        string mmsSourceVideoAssetPathName = generateFramesMedatada.get("mmsSourceVideoAssetPathName", "XXX").asString();
        int64_t videoDurationInMilliSeconds = generateFramesMedatada.get("videoDurationInMilliSeconds", -1).asInt64();

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
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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
        
        int64_t ingestionJobKey = slideShowMedatada.get("ingestionJobKey", -1).asInt64();
        double durationOfEachSlideInSeconds = slideShowMedatada.get("durationOfEachSlideInSeconds", -1).asDouble();
        int outputFrameRate = slideShowMedatada.get("outputFrameRate", -1).asInt();
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
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(exception e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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
        liveRecording->_encodingJobKey = encodingJobKey;
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

        liveRecording->_ingestionJobKey = liveRecorderMedatada.get("ingestionJobKey", -1).asInt64();

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

        string liveURL = liveRecording->_encodingParametersRoot.get("liveURL", "XXX").asString();
        time_t utcRecordingPeriodStart = liveRecording->_encodingParametersRoot.get("utcRecordingPeriodStart", -1).asInt64();
        time_t utcRecordingPeriodEnd = liveRecording->_encodingParametersRoot.get("utcRecordingPeriodEnd", -1).asInt64();
        int segmentDurationInSeconds = liveRecording->_encodingParametersRoot.get("segmentDurationInSeconds", -1).asInt();
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
		utcRecordingPeriodStart -= segmentDurationInSeconds;

		liveRecording->_ffmpeg->liveRecorder(
			liveRecording->_ingestionJobKey,
			encodingJobKey,
			liveRecording->_transcoderStagingContentsPath + liveRecording->_segmentListFileName,
			liveRecording->_recordedFileNamePrefix,
			liveURL, userAgent,
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
        
        _logger->info(__FILEREF__ + "liveRecorded finished"
            + ", liveRecording->_ingestionJobKey: " + to_string(liveRecording->_ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );

        liveRecording->_ingestionJobKey		= 0;

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string ("API failed")
			+ ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;
        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= true;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= true;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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
			lock_guard<mutex> locker(_liveRecordingMutex);

			for (shared_ptr<LiveRecording> liveRecording: _liveRecordingsCapability)
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
								highAvailability = liveRecording->_encodingParametersRoot.get(field, 0).asBool();

								field = "main";
								main = liveRecording->_encodingParametersRoot.get(field, 0).asBool();

								field = "segmentDurationInSeconds";
								segmentDurationInSeconds = liveRecording->_encodingParametersRoot.get(field, 0).asInt();

								field = "outputFileFormat";                                                                
								outputFileFormat = liveRecording->_encodingParametersRoot.get(field, "XXX").asString();                   
							}

							pair<string, int> lastRecordedAssetInfo = liveRecorder_processLastGeneratedLiveRecorderFiles(
								liveRecording->_ingestionJobKey,
								liveRecording->_encodingJobKey,
								highAvailability, main, segmentDurationInSeconds, outputFileFormat,                                                                              
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
						+ ", elapsed time: " + to_string(
							chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - now).count()
						)
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

pair<string, int> FFMPEGEncoder::liveRecorder_processLastGeneratedLiveRecorderFiles(
	int64_t ingestionJobKey, int64_t encodingJobKey,
	bool highAvailability, bool main, int segmentDurationInSeconds, string outputFileFormat,
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

			// UserData
			Json::Value userDataRoot;
			{
				if (_mmsEngineDBFacade->isMetadataPresent(liveRecorderParametersRoot, "UserData"))
				{
					userDataRoot = liveRecorderParametersRoot["UserData"];
				}

				Json::Value mmsDataRoot;
				mmsDataRoot["dataType"] = "liveRecordingChunk";
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

				userDataRoot["mmsData"] = mmsDataRoot;
			}

			// Title
			string addContentTitle;
			{
				// ConfigurationLabel is the label associated to the live URL
				addContentTitle = liveRecorderParametersRoot.get("ConfigurationLabel", "XXX").asString();

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
						addContentTitle, userDataRoot, outputFileFormat,
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
				+ ", movingDuration (millisecs): "
					+ to_string(chrono::duration_cast<chrono::milliseconds>(endMoving - startMoving).count())
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
		int64_t utcPreviousChunkStartTime = mmsDataRoot.get("utcPreviousChunkStartTime", -1).asInt64();
		int64_t utcChunkStartTime = mmsDataRoot.get("utcChunkStartTime", -1).asInt64();
		int64_t utcChunkEndTime = mmsDataRoot.get("utcChunkEndTime", -1).asInt64();

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
    		if (_mmsEngineDBFacade->isMetadataPresent(liveRecorderParametersRoot, field))
			{
				// internalMMSRootPresent = true;

				Json::Value internalMMSRoot = liveRecorderParametersRoot[field];

				field = "userKey";
				userKey = internalMMSRoot.get(field, -1).asInt64();

				field = "apiKey";
				apiKey = internalMMSRoot.get(field, "").asString();

				field = "OnSuccess";
    			if (_mmsEngineDBFacade->isMetadataPresent(internalMMSRoot, field))
					addContentRoot[field] = internalMMSRoot[field];

				field = "OnError";
    			if (_mmsEngineDBFacade->isMetadataPresent(internalMMSRoot, field))
					addContentRoot[field] = internalMMSRoot[field];

				field = "OnComplete";
    			if (_mmsEngineDBFacade->isMetadataPresent(internalMMSRoot, field))
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

		field = "Parameters";
		addContentRoot[field] = addContentParametersRoot;


		Json::Value workflowRoot;

		field = "Label";
		workflowRoot[field] = addContentTitle;

		field = "Type";
		workflowRoot[field] = "Workflow";

		{
			Json::Value variablesWorkflowRoot;

			field = "CurrentUtcChunkStartTime";
			variablesWorkflowRoot[field] = utcChunkStartTime;

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
			field = "CurrentUtcChunkStartTime_HHMISS";
			variablesWorkflowRoot[field] = string(currentUtcChunkStartTime_HHMISS);

			field = "PreviousUtcChunkStartTime";
			variablesWorkflowRoot[field] = utcPreviousChunkStartTime;

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
		while (sResponse.back() == 10 || sResponse.back() == 13)
			sResponse.pop_back();

		long responseCode = curlpp::infos::ResponseCode::get(request);
		if (responseCode == 201)
		{
			string message = __FILEREF__ + "Ingested recorded response"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", ingestingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endIngesting - startIngesting).count())
				+ ", workflowMetadata: " + workflowMetadata
				+ ", sResponse: " + sResponse
				;
			_logger->info(message);
		}
		else
		{
			string message = __FILEREF__ + "Ingested recorded response"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
				+ ", ingestingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endIngesting - startIngesting).count())
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
	shared_ptr<LiveProxy> liveProxy,
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

		int64_t ingestionJobKey = liveProxyMetadata.get("ingestionJobKey", -1).asInt64();

		string liveURL = liveProxyMetadata.get("liveURL", -1).asString();
		string userAgent = liveProxyMetadata.get("userAgent", -1).asString();
		string outputType = liveProxyMetadata.get("outputType", -1).asString();
		int segmentDurationInSeconds = liveProxyMetadata.get("segmentDurationInSeconds", -1).asInt();
		string cdnURL = liveProxyMetadata.get("cdnURL", "").asString();
		string m3u8FilePathName = liveProxyMetadata.get("m3u8FilePathName", -1).asString();

		if (outputType == "HLS")
		{
			size_t m3u8FilePathIndex = m3u8FilePathName.find_last_of("/");
			if (m3u8FilePathIndex == string::npos)
			{
				string errorMessage = string("m3u8FilePathName not well format")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
                    + ", m3u8FilePathName: " + m3u8FilePathName
                    ;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string m3u8DirectoryPathName = m3u8FilePathName.substr(0, m3u8FilePathIndex);

			if (!FileIO::directoryExisting(m3u8DirectoryPathName))
			{
				bool noErrorIfExists = true;
				bool recursive = true;

				_logger->info(__FILEREF__ + "Creating directory (if needed)"
					+ ", m3u8DirectoryPathName: " + m3u8DirectoryPathName
				);
				FileIO::createDirectory(m3u8DirectoryPathName,
					S_IRUSR | S_IWUSR | S_IXUSR |
					S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
			}
			else
			{
				// clean directory removing files

				FileIO::DirectoryEntryType_t detDirectoryEntryType;
				shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (
					m3u8DirectoryPathName + "/");

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

						{
							bool exceptionInCaseOfError = false;

							string segmentPathNameToBeRemoved =                                                   
								m3u8DirectoryPathName + "/" + directoryEntry;                                     
							_logger->info(__FILEREF__ + "Remove"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
							FileIO::remove(segmentPathNameToBeRemoved, exceptionInCaseOfError);
						}
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

						// throw e;
					}
				}

				FileIO::closeDirectory (directory);
			}

			// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
			liveProxy->_ffmpeg->liveProxyByHLS(
				ingestionJobKey,
				encodingJobKey,
				liveURL, userAgent,
				segmentDurationInSeconds,
				m3u8FilePathName,
				&(liveProxy->_childPid));
		}
		else
		{
			liveProxy->_ffmpeg->liveProxyByCDN(
				ingestionJobKey,
				encodingJobKey,
				liveURL, userAgent,
				cdnURL,
				&(liveProxy->_childPid));
		}

        liveProxy->_running = false;
        liveProxy->_childPid = 0;
        
        _logger->info(__FILEREF__ + "_ffmpeg->liveProxyByHLS finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", m3u8FilePathName: " + m3u8FilePathName
        );

		bool completedWithError			= false;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        liveProxy->_running = false;
        liveProxy->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(liveProxy->_encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(runtime_error e)
    {
        liveProxy->_running = false;
        liveProxy->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        liveProxy->_running = false;
        liveProxy->_childPid = 0;

        string errorMessage = string("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
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
        int64_t videoDurationInMilliSeconds = videoSpeedMetadata.get("videoDurationInMilliSeconds", -1).asInt64();

        string videoSpeedType = videoSpeedMetadata.get("videoSpeedType", "XXX").asString();
        int videoSpeedSize = videoSpeedMetadata.get("videoSpeedSize", 3).asInt();
        
        string stagingEncodedAssetPathName = videoSpeedMetadata.get("stagingEncodedAssetPathName", "XXX").asString();
        int64_t encodingJobKey = videoSpeedMetadata.get("encodingJobKey", -1).asInt64();
        int64_t ingestionJobKey = videoSpeedMetadata.get("ingestionJobKey", -1).asInt64();

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
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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
        int64_t mainVideoDurationInMilliSeconds = pictureInPictureMetadata.get("mainVideoDurationInMilliSeconds", -1).asInt64();

        string mmsOverlayVideoAssetPathName = pictureInPictureMetadata.get("mmsOverlayVideoAssetPathName", "XXX").asString();
        int64_t overlayVideoDurationInMilliSeconds = pictureInPictureMetadata.get("overlayVideoDurationInMilliSeconds", -1).asInt64();

        bool soundOfMain = pictureInPictureMetadata.get("soundOfMain", -1).asBool();

        string overlayPosition_X_InPixel = pictureInPictureMetadata.get("overlayPosition_X_InPixel", "XXX").asString();
        string overlayPosition_Y_InPixel = pictureInPictureMetadata.get("overlayPosition_Y_InPixel", "XXX").asString();
        string overlay_Width_InPixel = pictureInPictureMetadata.get("overlay_Width_InPixel", "XXX").asString();
        string overlay_Height_InPixel = pictureInPictureMetadata.get("overlay_Height_InPixel", "XXX").asString();
        
        string stagingEncodedAssetPathName = pictureInPictureMetadata.get("stagingEncodedAssetPathName", "XXX").asString();
        int64_t encodingJobKey = pictureInPictureMetadata.get("encodingJobKey", -1).asInt64();
        int64_t ingestionJobKey = pictureInPictureMetadata.get("ingestionJobKey", -1).asInt64();

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
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
	catch(FFMpegEncodingKilledByUser e)
	{
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= false;
		bool killedByUser				= true;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encoding->_encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
    }
    catch(runtime_error e)
    {
        encoding->_running = false;
        encoding->_childPid = 0;

        string errorMessage = string ("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
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

        string errorMessage = string("API failed")
                    + ", encodingJobKey: " + to_string(encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);

		bool completedWithError			= true;
		bool killedByUser				= false;
		bool urlForbidden				= false;
		bool urlNotFound				= false;
		addEncodingCompleted(encodingJobKey,
				completedWithError, killedByUser,
				urlForbidden, urlNotFound);
        // this method run on a detached thread, we will not generate exception
        // The ffmpeg method will make sure the encoded file is removed 
        // (this is checked in EncoderVideoAudioProxy)
        // throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::addEncodingCompleted(
        int64_t encodingJobKey, bool completedWithError,
		bool killedByUser, bool urlForbidden, bool urlNotFound)
{
	lock_guard<mutex> locker(_encodingCompletedMutex);

	shared_ptr<EncodingCompleted> encodingCompleted = make_shared<EncodingCompleted>();

	encodingCompleted->_encodingJobKey		= encodingJobKey;
	encodingCompleted->_completedWithError	= completedWithError;
	encodingCompleted->_killedByUser		= killedByUser;
	encodingCompleted->_urlForbidden		= urlForbidden;
	encodingCompleted->_urlNotFound			= urlNotFound;
	encodingCompleted->_timestamp			= chrono::system_clock::now();

	_encodingCompletedMap.insert(make_pair(encodingCompleted->_encodingJobKey, encodingCompleted));

	_logger->info(__FILEREF__ + "addEncodingCompleted"
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", encodingCompletedRetention.size: " + to_string(_encodingCompletedMap.size())
			);
}

void FFMPEGEncoder::removeEncodingCompletedIfPresent(int64_t encodingJobKey)
{

	lock_guard<mutex> locker(_encodingCompletedMutex);

	map<int64_t, shared_ptr<EncodingCompleted>>::iterator it =
		_encodingCompletedMap.find(encodingJobKey);
	if (it != _encodingCompletedMap.end())
	{
		_encodingCompletedMap.erase(it);

		_logger->info(__FILEREF__ + "removeEncodingCompletedIfPresent"
			+ ", encodingCompletedRetention.size: " + to_string(_encodingCompletedMap.size())
			);
	}
}

void FFMPEGEncoder::encodingCompletedRetention()
{

	lock_guard<mutex> locker(_encodingCompletedMutex);

	chrono::system_clock::time_point now = chrono::system_clock::now();

	for(map<int64_t, shared_ptr<EncodingCompleted>>::iterator it = _encodingCompletedMap.begin();
			it != _encodingCompletedMap.end(); )
	{
		if(now - (it->second->_timestamp) >= chrono::seconds(_encodingCompletedRetentionInSeconds))
			it = _encodingCompletedMap.erase(it);
		else
			it++;
	}

	_logger->info(__FILEREF__ + "encodingCompletedRetention"
			+ ", encodingCompletedRetention.size: " + to_string(_encodingCompletedMap.size())
			);
}

