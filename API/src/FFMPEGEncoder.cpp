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

int main(int argc, char** argv) 
{
    const char* configurationPathName = getenv("MMS_CONFIGPATHNAME");
    if (configurationPathName == nullptr)
    {
        cerr << "MMS FFMPEGEncoder: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;
        
        return 1;
    }
    
    FFMPEGEncoder ffmpegEncoder(configurationPathName);

    return ffmpegEncoder.listen();
}

FFMPEGEncoder::FFMPEGEncoder(const char* configurationPathName): APICommon(configurationPathName) 
{
    _maxEncodingsCapability =  _configuration["ffmpeg"].get("maxEncodingsCapability", 0).asInt();

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
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
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
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength,
        string requestBody
)
{
    
    auto methodIt = queryParameters.find("method");
    if (methodIt == queryParameters.end())
    {
        string errorMessage = string("The 'method' parameter is not found");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(400, errorMessage);

        throw runtime_error(errorMessage);
    }
    string method = methodIt->second;

    if (method == "encodeContent")
    {
        bool isAdminAPI = get<1>(customerAndFlags);
        if (!isAdminAPI)
        {
            string errorMessage = string("APIKey flags does not have the ADMIN permission"
                    ", isAdminAPI: " + to_string(isAdminAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
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
            string errorMessage = string("All encodings are running, no encoding available")
            ;
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        thread encodeContentThread(&FFMPEGEncoder::encodeContent, this, 
                                selectedEncoding, requestBody);
        encodeContentThread.detach();
        
        // to make sure thread is able to set encoding->running to true
        this_thread::sleep_for(chrono::seconds(1));
    }
    else if (method == "encodingProgress")
    {
        bool isAdminAPI = get<1>(customerAndFlags);
        if (!isAdminAPI)
        {
            string errorMessage = string("APIKey flags does not have the ADMIN permission"
                    ", isAdminAPI: " + to_string(isAdminAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'encodingJobKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

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
            string errorMessage = string("No encoding found")
                    + ", encodingJobKey: " + encodingJobKeyIt->second
            ;
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);
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

            sendError(400, errorMessage);

            throw e;
        }
        catch(exception e)
        {
            string errorMessage = string("_ffmpeg->getEncodingProgress failed")
                + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw e;
        }
        
        string responseBody = string("{ ")
                + "\"encodingProgress\": " + to_string(encodingProgress) + " "
                + "}";
        
        sendSuccess(200, responseBody);
    }
    else
    {
        string errorMessage = string("No API is matched")
            + ", requestURI: " +requestURI
            + ", requestMethod: " +requestMethod;
        _logger->error(__FILEREF__ + errorMessage);

        sendError(400, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void FFMPEGEncoder::encodeContent(
        shared_ptr<Encoding> encoding,
        string requestBody)
{
    string api = "encodeContent";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        encoding->_running = true;
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
            "customerDirectoryName": "...",
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
                        + ", requestBody: " + requestBody;
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
        string customerDirectoryName = encodingMedatada.get("customerDirectoryName", "XXX").asString();
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
                customerDirectoryName,
                relativePath,
                encodingJobKey,
                ingestionJobKey);        
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

        string responseBody = string("{ ")
                + "\"ingestionJobKey\": " + to_string(ingestionJobKey) + " "
                + "\"ffmpegEncoderHost\": \"" + System::getHostName() + "\" "
                + "}";

        sendSuccess(200, responseBody);
        
        encoding->_running = false;
    }
    catch(runtime_error e)
    {
        encoding->_running = false;

        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        encoding->_running = false;

        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(500, errorMessage);

        throw runtime_error(errorMessage);
    }
}
