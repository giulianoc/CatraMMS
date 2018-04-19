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
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/System.h"
#include "FFMPEGEncoder.h"

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
    logger->info(__FILEREF__ + "Configuration item"
        + ", log->level: " + logLevel
    );
    if (logLevel == "debug")
        spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "info")
        spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "err")
        spdlog::set_level(spdlog::level::err); // trace, debug, info, warn, err, critical, off
    string pattern =  configuration["log"]["encoder"].get("pattern", "XXX").asString();
    logger->info(__FILEREF__ + "Configuration item"
        + ", log->pattern: " + pattern
    );
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

    logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
    shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, logger);

    logger->info(__FILEREF__ + "Creating MMSStorage"
            );
    shared_ptr<MMSStorage> mmsStorage = make_shared<MMSStorage>(
            configuration, mmsEngineDBFacade, logger);
    
    FCGX_Init();

    mutex fcgiAcceptMutex;

    FFMPEGEncoder ffmpegEncoder(configuration, 
            mmsEngineDBFacade,
            mmsStorage,
            &fcgiAcceptMutex,
            logger);

    return ffmpegEncoder();
}

FFMPEGEncoder::FFMPEGEncoder(Json::Value configuration, 
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        shared_ptr<MMSStorage> mmsStorage,
        mutex* fcgiAcceptMutex,
        shared_ptr<spdlog::logger> logger)
    : APICommon(configuration, 
        mmsEngineDBFacade,
        mmsStorage,
        fcgiAcceptMutex,
        logger) 
{
    _maxEncodingsCapability =  _configuration["ffmpeg"].get("maxEncodingsCapability", 0).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->maxEncodingsCapability: " + to_string(_maxEncodingsCapability)
    );

    for (int encodingIndex = 0; encodingIndex < _maxEncodingsCapability; encodingIndex++)
    {
        shared_ptr<Encoding>    encoding = make_shared<Encoding>();
        encoding->_running   = false;
        encoding->_ffmpeg   = make_shared<FFMpeg>(_configuration, _logger);

        _encodingsCapability.push_back(encoding);
    }
}

FFMPEGEncoder::~FFMPEGEncoder() {
}

void FFMPEGEncoder::getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Workspace>,bool,bool>& workspaceAndFlags,
        unsigned long contentLength
)
{
    _logger->error(__FILEREF__ + "FFMPEGEncoder application is able to manage ONLY NON-Binary requests");
    
    string errorMessage = string("Internal server error");
    _logger->error(__FILEREF__ + errorMessage);

    sendError(500, errorMessage);

    throw runtime_error(errorMessage);
}

void FFMPEGEncoder::manageRequestAndResponse(
        FCGX_Request& request,
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Workspace>,bool,bool>& workspaceAndFlags,
        unsigned long contentLength,
        string requestBody,
        string xCatraMMSResumeHeader,
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

    if (method == "encodeContent")
    {
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        int64_t encodingJobKey = stol(encodingJobKeyIt->second);
        
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
            // same string declared in EncoderVideoAudioProxy.cpp
            string noEncodingAvailableMessage("__NO-ENCODING-AVAILABLE__");
            
            _logger->error(__FILEREF__ + noEncodingAvailableMessage);

            sendError(request, 400, noEncodingAvailableMessage);

            throw runtime_error(noEncodingAvailableMessage);
        }
        
        try
        {            
            _logger->info(__FILEREF__ + "Creating encodeContent thread"
                + ", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey)
                + ", requestBody: " + requestBody
            );
            thread encodeContentThread(&FFMPEGEncoder::encodeContent, this, selectedEncoding, encodingJobKey, requestBody);
            encodeContentThread.detach();
            
            // encodeContent(request, selectedEncoding, requestBody);
            
            string responseBody = string("{ ")
                    + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                    + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                    + "}";

            sendSuccess(request, 200, responseBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "encodeContentThread failed"
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

        int64_t encodingJobKey = stol(encodingJobKeyIt->second);
        
        lock_guard<mutex> locker(_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: _encodingsCapability)
        {
            if (encoding->_encodingJobKey == encodingJobKey)
            {
                encodingFound = true;
                selectedEncoding = encoding;
                
                break;
            }
        }

        string responseBody;
        if (!encodingFound)
        {
            responseBody = string("{ ")
                + "\"encodingJobKey\": " + to_string(encodingJobKey) + " "
                + ", \"encodingFinished\": true "
                + "}";
        }
        else
        {
            responseBody = string("{ ")
                + "\"encodingJobKey\": " + to_string(selectedEncoding->_encodingJobKey) + " "
                + ", \"encodingFinished\": false "
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

        lock_guard<mutex> locker(_encodingMutex);

        shared_ptr<Encoding>    selectedEncoding;
        bool                    encodingFound = false;
        for (shared_ptr<Encoding> encoding: _encodingsCapability)
        {
            if (encoding->_encodingJobKey == stol(encodingJobKeyIt->second))
            {
                encodingFound = true;
                selectedEncoding = encoding;
                
                break;
            }
        }

        if (!encodingFound)
        {
            // same string declared in EncoderVideoAudioProxy.cpp
            string noEncodingJobKeyFound("__NO-ENCODINGJOBKEY-FOUND__");
            
            _logger->error(__FILEREF__ + noEncodingJobKeyFound);

            sendError(request, 400, noEncodingJobKeyFound);

            throw runtime_error(noEncodingJobKeyFound);
        }

        int encodingProgress;
        try
        {
            encodingProgress = selectedEncoding->_ffmpeg->getEncodingProgress();
        }
        catch(FFMpegEncodingStatusNotAvailable e)
        {
            string errorMessage = string("_ffmpeg->getEncodingProgress failed")
                + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw e;
        }
        catch(exception e)
        {
            string errorMessage = string("_ffmpeg->getEncodingProgress failed")
                + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw e;
        }
        
        string responseBody = string("{ ")
                + "\"encodingProgress\": " + to_string(encodingProgress) + " "
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
}

void FFMPEGEncoder::encodeContent(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody)
{
    string api = "encodeContent";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        encoding->_running = true;
        encoding->_encodingJobKey = encodingJobKey;
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
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        string mmsSourceAssetPathName = encodingMedatada.get("mmsSourceAssetPathName", "XXX").asString();
        int64_t durationInMilliSeconds = encodingMedatada.get("durationInMilliSeconds", -1).asInt64();
        string encodedFileName = encodingMedatada.get("encodedFileName", "XXX").asString();
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
                encodedFileName,
                stagingEncodedAssetPathName,
                encodingProfileDetails,
                contentType == MMSEngineDBFacade::ContentType::Video,
                physicalPathKey,
                workspaceDirectoryName,
                relativePath,
                encodingJobKey,
                ingestionJobKey);        
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

//        string responseBody = string("{ ")
//                + "\"ingestionJobKey\": " + to_string(ingestionJobKey) + " "
//                + ", \"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
//                + "}";

        // sendSuccess(request, 200, responseBody);
        
        encoding->_running = false;
        
        _logger->info(__FILEREF__ + "Encode content finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );
    }
    catch(runtime_error e)
    {
        encoding->_running = false;

        string errorMessage = string ("API failed")
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);
        /*
        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);
        */

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        encoding->_running = false;

        string errorMessage = string("API failed")
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        ;

        _logger->error(__FILEREF__ + errorMessage);
        /*
        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);
         */

        throw runtime_error(errorMessage);
    }
}
