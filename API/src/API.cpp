/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   API.cpp
 * Author: giuliano
 * 
 * Created on February 18, 2018, 1:27 AM
 */

#include <fstream>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "Validator.h"
#include "API.h"

int main(int argc, char** argv) 
{

    const char* configurationPathName = getenv("MMS_CONFIGPATHNAME");
    if (configurationPathName == nullptr)
    {
        cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;
        
        return 1;
    }
    
    Json::Value configuration = APICommon::loadConfigurationFile(configurationPathName);
    
    string logPathName =  configuration["log"]["api"].get("pathName", "XXX").asString();
    bool stdout =  configuration["log"]["api"].get("stdout", "XXX").asBool();
    
    std::vector<spdlog::sink_ptr> sinks;
    auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logPathName.c_str(), 11, 20);
    sinks.push_back(dailySink);
    if (stdout)
    {
        auto stdoutSink = spdlog::sinks::stdout_sink_mt::instance();
        sinks.push_back(stdoutSink);
    }
    auto logger = std::make_shared<spdlog::logger>("API", begin(sinks), end(sinks));
    // shared_ptr<spdlog::logger> logger = spdlog::stdout_logger_mt("API");
    // shared_ptr<spdlog::logger> logger = spdlog::daily_logger_mt("API", logPathName.c_str(), 11, 20);
    
    // trigger flush if the log severity is error or higher
    logger->flush_on(spdlog::level::trace);
    
    string logLevel =  configuration["log"]["api"].get("level", "XXX").asString();
    logger->info(__FILEREF__ + "Configuration item"
        + ", log->level: " + logLevel
    );
    if (logLevel == "debug")
        spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "info")
        spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off
    else if (logLevel == "err")
        spdlog::set_level(spdlog::level::err); // trace, debug, info, warn, err, critical, off
    string pattern =  configuration["log"]["api"].get("pattern", "XXX").asString();
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

    int threadsNumber = configuration["api"].get("threadsNumber", 1).asInt();
    logger->info(__FILEREF__ + "Configuration item"
        + ", api->threadsNumber: " + to_string(threadsNumber)
    );

    mutex fcgiAcceptMutex;
    API::FileUploadProgressData fileUploadProgressData;
    
    vector<shared_ptr<API>> apis;
    vector<thread> apiThreads;
    
    for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
    {
        shared_ptr<API> api = make_shared<API>(configuration, 
                mmsEngineDBFacade,
                mmsStorage,
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

    return 0;
}

API::API(Json::Value configuration, 
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            mutex* fcgiAcceptMutex,
            FileUploadProgressData* fileUploadProgressData,
            shared_ptr<spdlog::logger> logger)
    :APICommon(configuration, 
            mmsEngineDBFacade,
            mmsStorage,
            fcgiAcceptMutex,
            logger) 
{
    string encodingPriority =  _configuration["api"].get("encodingPriorityWorkspaceDefaultValue", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->encodingPriorityWorkspaceDefaultValue: " + encodingPriority
    );
    if (encodingPriority == "low")
        _encodingPriorityWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPriority::Low;
    else
        _encodingPriorityWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPriority::Low;

    string encodingPeriod =  _configuration["api"].get("encodingPeriodWorkspaceDefaultValue", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->encodingPeriodWorkspaceDefaultValue: " + encodingPeriod
    );
    if (encodingPeriod == "daily")
        _encodingPeriodWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;
    else
        _encodingPeriodWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;

    _maxIngestionsNumberWorkspaceDefaultValue = _configuration["api"].get("maxIngestionsNumberWorkspaceDefaultValue", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->maxIngestionsNumberWorkspaceDefaultValue: " + to_string(_maxIngestionsNumberWorkspaceDefaultValue)
    );
    _maxStorageInGBWorkspaceDefaultValue = _configuration["api"].get("maxStorageInGBWorkspaceDefaultValue", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->maxStorageInGBWorkspaceDefaultValue: " + to_string(_maxStorageInGBWorkspaceDefaultValue)
    );

    Json::Value api = _configuration["api"];
    // _binaryBufferLength             = api["binary"].get("binaryBufferLength", "XXX").asInt();
    // _logger->info(__FILEREF__ + "Configuration item"
    //    + ", api->binary->binaryBufferLength: " + to_string(_binaryBufferLength)
    // );
    _progressUpdatePeriodInSeconds  = api["binary"].get("progressUpdatePeriodInSeconds", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->progressUpdatePeriodInSeconds: " + to_string(_progressUpdatePeriodInSeconds)
    );
    _webServerPort  = api["binary"].get("webServerPort", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->webServerPort: " + to_string(_webServerPort)
    );
    _maxProgressCallFailures  = api["binary"].get("maxProgressCallFailures", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
    );
    _progressURI  = api["binary"].get("progressURI", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->progressURI: " + _progressURI
    );
    
    _defaultTTLInSeconds  = api["delivery"].get("defaultTTLInSeconds", 60).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->defaultTTLInSeconds: " + to_string(_defaultTTLInSeconds)
    );

    _defaultMaxRetries  = api["delivery"].get("defaultMaxRetries", 60).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->defaultMaxRetries: " + to_string(_defaultMaxRetries)
    );

    _defaultRedirect  = api["delivery"].get("defaultRedirect", 60).asBool();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->defaultRedirect: " + to_string(_defaultRedirect)
    );
    
    _deliveryProtocol  = api["delivery"].get("deliveryProtocol", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->deliveryProtocol: " + _deliveryProtocol
    );
    _deliveryHost  = api["delivery"].get("deliveryHost", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->deliveryHost: " + _deliveryHost
    );
    
    _fileUploadProgressData     = fileUploadProgressData;
    _fileUploadProgressThreadShutdown       = false;
}

API::~API() {
}

void API::stopUploadFileProgressThread()
{
    _fileUploadProgressThreadShutdown       = true;
    
    this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));
}

void API::getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool>& userKeyWorkspaceAndFlags,
        unsigned long contentLength
)
{
    _logger->error(__FILEREF__ + "API application is able to manage ONLY NON-Binary requests");
    
    string errorMessage = string("Internal server error");
    _logger->error(__FILEREF__ + errorMessage);

    sendError(500, errorMessage);

    throw runtime_error(errorMessage);
}

void API::manageRequestAndResponse(
        FCGX_Request& request,
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool>& userKeyWorkspaceAndFlags,
        unsigned long contentLength,
        string requestBody,
        string xCatraMMSResumeHeader,
        unordered_map<string, string>& requestDetails
)
{
    
    _logger->info(__FILEREF__ + "Received manageRequestAndResponse"
        + ", requestURI: " + requestURI
        + ", requestMethod: " + requestMethod
        + ", requestBody: " + requestBody
        + ", contentLength: " + to_string(contentLength)
        + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
    );

    auto methodIt = queryParameters.find("method");
    if (methodIt == queryParameters.end())
    {
        string errorMessage = string("The 'method' parameter is not found");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 400, errorMessage);

        throw runtime_error(errorMessage);
    }
    string method = methodIt->second;

    if (method == "binaryAuthorization")
    {
        // since we are here, for sure user is authorized
        
        // retrieve the HTTP_X_ORIGINAL_METHOD to retrieve the progress id (set in the nginx server configuration)
        auto progressIdIt = requestDetails.find("HTTP_X_ORIGINAL_METHOD");
        auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
        if (progressIdIt != requestDetails.end() && originalURIIt != requestDetails.end())
        {
            int ingestionJobKeyIndex = originalURIIt->second.find_last_of("/");
            if (ingestionJobKeyIndex != string::npos)
            {
                try
                {
                    struct FileUploadProgressData::RequestData requestData;

                    requestData._progressId = progressIdIt->second;
                    requestData._ingestionJobKey = stoll(originalURIIt->second.substr(ingestionJobKeyIndex + 1));
                    requestData._lastPercentageUpdated = 0;
                    requestData._callFailures = 0;

                    // Content-Range: bytes 0-99999/100000
                    requestData._contentRangePresent = false;
                    requestData._contentRangeStart  = -1;
                    requestData._contentRangeEnd  = -1;
                    requestData._contentRangeSize  = -1;
                    auto contentRangeIt = requestDetails.find("HTTP_CONTENT_RANGE");
                    if (contentRangeIt != requestDetails.end())
                    {
                        string contentRange = contentRangeIt->second;
                        try
                        {
                            parseContentRange(contentRange,
                                requestData._contentRangeStart,
                                requestData._contentRangeEnd,
                                requestData._contentRangeSize);

                            requestData._contentRangePresent = true;                
                        }
                        catch(exception e)
                        {
                            string errorMessage = string("Content-Range is not well done. Expected format: 'Content-Range: bytes <start>-<end>/<size>'")
                                + ", contentRange: " + contentRange
                            ;
                            _logger->error(__FILEREF__ + errorMessage);

                            sendError(request, 500, errorMessage);

                            throw runtime_error(errorMessage);            
                        }
                    }

                    _logger->info(__FILEREF__ + "Content-Range details"
                        + ", contentRangePresent: " + to_string(requestData._contentRangePresent)
                        + ", contentRangeStart: " + to_string(requestData._contentRangeStart)
                        + ", contentRangeEnd: " + to_string(requestData._contentRangeEnd)
                        + ", contentRangeSize: " + to_string(requestData._contentRangeSize)
                    );

                    lock_guard<mutex> locker(_fileUploadProgressData->_mutex);                    

                    _fileUploadProgressData->_filesUploadProgressToBeMonitored.push_back(requestData);
                    _logger->info(__FILEREF__ + "Added upload file progress to be monitored"
                        + ", _progressId: " + requestData._progressId
                    );
                }
                catch (exception e)
                {
                    _logger->error(__FILEREF__ + "ProgressId not found"
                        + ", progressIdIt->second: " + progressIdIt->second
                    );
                }
            }
        }        
        
        string responseBody;
        sendSuccess(request, 200, responseBody);
    }
    else if (method == "deliveryAuthorization")
    {        
        // retrieve the HTTP_X_ORIGINAL_METHOD to retrieve the token to be checked (set in the nginx server configuration)
        try
        {
            auto tokenIt = requestDetails.find("HTTP_X_ORIGINAL_METHOD");
            auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
            if (tokenIt != requestDetails.end() && originalURIIt != requestDetails.end()
                    )
            {
                int64_t token = stoll(tokenIt->second);

                string contentURI = originalURIIt->second;
                size_t endOfURIIndex = contentURI.find_last_of("?");
                if (endOfURIIndex == string::npos)
                {
                    string errorMessage = string("Wrong URI format")
                        + ", contentURI: " + contentURI
                            ;
                    _logger->info(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);
                }
                contentURI = contentURI.substr(0, endOfURIIndex);

                if (_mmsEngineDBFacade->checkDeliveryAuthorization(token, contentURI))
                {
                    _logger->info(__FILEREF__ + "token authorized"
                        + ", token: " + to_string(token)
                    );

                    string responseBody;
                    sendSuccess(request, 200, responseBody);
                }
                else
                {
                    string errorMessage = string("Not authorized: token invalid")
                        + ", token: " + to_string(token)
                            ;
                    _logger->info(__FILEREF__ + errorMessage);

                    string responseBody;
                    sendError(request, 403, errorMessage);
                }
            }        
            else
            {
                string errorMessage = string("Not authorized: token parameter not present")
                        ;
                _logger->info(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);
            }
        }
        catch(exception e)
        {
            string errorMessage = string("Not authorized: exception retrieving the token");
            _logger->info(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);
        }
    }
    else if (method == "registerUser")
    {
        /*
        bool isAdminAPI = get<2>(userKeyWorkspaceAndFlags);
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
        
        registerUser(request, requestBody);
    }
    else if (method == "confirmUser")
    {
        /*
        bool isAdminAPI = get<2>(userKeyWorkspaceAndFlags);
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
        
        confirmUser(request, queryParameters);
    }
    else if (method == "createDeliveryAuthorization")
    {
        /*
        bool isAdminAPI = get<2>(userKeyWorkspaceAndFlags);
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

        string clientIPAddress;
        auto remoteAddrIt = requestDetails.find("REMOTE_ADDR");
        if (remoteAddrIt != requestDetails.end())
            clientIPAddress = remoteAddrIt->second;
            
        createDeliveryAuthorization(request, get<0>(userKeyWorkspaceAndFlags),
                get<1>(userKeyWorkspaceAndFlags),
                clientIPAddress, queryParameters);
    }
    else if (method == "ingestion")
    {
        /*
        bool isUserAPI = get<2>(userKeyWorkspaceAndFlags);
        if (!isUserAPI)
        {
            string errorMessage = string("APIKey flags does not have the USER permission"
                    ", isUserAPI: " + to_string(isUserAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        */
        
        ingestion(request, get<1>(userKeyWorkspaceAndFlags), queryParameters, requestBody);
    }
    else if (method == "ingestionStatus")
    {
        /*
        bool isUserAPI = get<2>(userKeyWorkspaceAndFlags);
        if (!isUserAPI)
        {
            string errorMessage = string("APIKey flags does not have the USER permission"
                    ", isUserAPI: " + to_string(isUserAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        */
        
        ingestionStatus(request, get<1>(userKeyWorkspaceAndFlags), queryParameters, requestBody);
    }
    else if (method == "contentList")
    {
        /*
        bool isUserAPI = get<2>(userKeyWorkspaceAndFlags);
        if (!isUserAPI)
        {
            string errorMessage = string("APIKey flags does not have the USER permission"
                    ", isUserAPI: " + to_string(isUserAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        */
        
        contentList(request, get<1>(userKeyWorkspaceAndFlags), queryParameters, requestBody);
    }
    else if (method == "uploadedBinary")
    {
        /*
        bool isUserAPI = get<2>(userKeyWorkspaceAndFlags);
        if (!isUserAPI)
        {
            string errorMessage = string("APIKey flags does not have the USER permission"
                    ", isUserAPI: " + to_string(isUserAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        */
                
        uploadedBinary(request, requestMethod, xCatraMMSResumeHeader,
            queryParameters, userKeyWorkspaceAndFlags, // contentLength,
                requestDetails);
    }
    else if (method == "addEncodingProfilesSet")
    {
        /*
        bool isUserAPI = get<2>(userKeyWorkspaceAndFlags);
        if (!isUserAPI)
        {
            string errorMessage = string("APIKey flags does not have the USER permission"
                    ", isUserAPI: " + to_string(isUserAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        */
                
        addEncodingProfilesSet(request, get<1>(userKeyWorkspaceAndFlags),
            queryParameters, requestBody);
    }
    else if (method == "addEncodingProfile")
    {
        /*
        bool isUserAPI = get<2>(userKeyWorkspaceAndFlags);
        if (!isUserAPI)
        {
            string errorMessage = string("APIKey flags does not have the USER permission"
                    ", isUserAPI: " + to_string(isUserAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        */
                
        addEncodingProfile(request, get<1>(userKeyWorkspaceAndFlags),
            queryParameters, requestBody);
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

void API::fileUploadProgressCheck()
{

    while (!_fileUploadProgressThreadShutdown)
    {
        this_thread::sleep_for(chrono::seconds(_progressUpdatePeriodInSeconds));
        
        lock_guard<mutex> locker(_fileUploadProgressData->_mutex);

        for (auto itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.begin(); 
                itr != _fileUploadProgressData->_filesUploadProgressToBeMonitored.end(); )
        {            
            bool iteratorAlreadyUpdated = false;
                        
            if (itr->_callFailures >= _maxProgressCallFailures)
            {
                _logger->error(__FILEREF__ + "fileUploadProgressCheck: remove entry because of too many call failures"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", _maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
                );
                itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr);	// returns iterator to the next element
                
                continue;
            }
            
            try 
            {
                string progressURL = string("http://localhost:") + to_string(_webServerPort) + _progressURI;
                string progressIdHeader = string("X-Progress-ID: ") + itr->_progressId;

                _logger->info(__FILEREF__ + "Call for upload progress"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", progressURL: " + progressURL
                    + ", progressIdHeader: " + progressIdHeader
                );

                curlpp::Cleanup cleaner;
                curlpp::Easy request;
                ostringstream response;

                list<string> header;
                header.push_back(progressIdHeader);

                // Setting the URL to retrive.
                request.setOpt(new curlpp::options::Url(progressURL));
                request.setOpt(new curlpp::options::HttpHeader(header));
                request.setOpt(new curlpp::options::WriteStream(&response));
                request.perform();

                string sResponse = response.str();

                // LF and CR create problems to the json parser...
                while (sResponse.back() == 10 || sResponse.back() == 13)
                    sResponse.pop_back();
                
                _logger->info(__FILEREF__ + "Call for upload progress response"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", sResponse: " + sResponse
                );

                try
                {
                    Json::Value uploadProgressResponse;

                    Json::CharReaderBuilder builder;
                    Json::CharReader* reader = builder.newCharReader();
                    string errors;
                    
                    bool parsingSuccessful = reader->parse(sResponse.c_str(),
                            sResponse.c_str() + sResponse.size(), 
                            &uploadProgressResponse, &errors);
                    delete reader;

                    if (!parsingSuccessful)
                    {
                        string errorMessage = __FILEREF__ + "failed to parse the response body"
                                + ", errors: " + errors
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }

                    // { "state" : "uploading", "received" : 731195032, "size" : 745871360 }
                    // At the end: { "state" : "done" }
                    // In case of error: { "state" : "error", "status" : 500 }
                    string state = uploadProgressResponse.get("state", "XXX").asString();
                    if (state == "done")
                    {
                        double relativeProgress = 100.0;
                        double relativeUploadingPercentage = 100.0;
                        
                        int64_t absoluteReceived = -1;
                        if (itr->_contentRangePresent)
                            absoluteReceived    = itr->_contentRangeEnd;
                        int64_t absoluteSize = -1;
                        if (itr->_contentRangePresent)
                            absoluteSize    = itr->_contentRangeSize;

                        double absoluteProgress;
                        if (itr->_contentRangePresent)
                            absoluteProgress = ((double) absoluteReceived / (double) absoluteSize) * 100;
                            
                        // this is to have one decimal in the percentage
                        double absoluteUploadingPercentage;
                        if (itr->_contentRangePresent)
                            absoluteUploadingPercentage = ((double) ((int) (absoluteProgress * 10))) / 10;

                        if (itr->_contentRangePresent)
                        {
                            _logger->info(__FILEREF__ + "Upload just finished"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
                                + ", relativeProgress: " + to_string(relativeProgress)
                                + ", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage)
                                + ", absoluteProgress: " + to_string(absoluteProgress)
                                + ", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
                                + ", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
                            );
                        }
                        else
                        {
                            _logger->info(__FILEREF__ + "Upload just finished"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
                                + ", relativeProgress: " + to_string(relativeProgress)
                                + ", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage)
                                + ", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
                            );
                        }

                        if (itr->_contentRangePresent)
                        {
                            _logger->info(__FILEREF__ + "Update IngestionJob"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
                                + ", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
                            );                            
                            _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                itr->_ingestionJobKey, absoluteUploadingPercentage);
                        }
                        else
                        {
                            _logger->info(__FILEREF__ + "Update IngestionJob"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
                                + ", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage)
                            );                            
                            _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                itr->_ingestionJobKey, relativeUploadingPercentage);
                        }

                        itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr);	// returns iterator to the next element
                        
                        iteratorAlreadyUpdated = true;
                    }
                    else if (state == "error")
                    {
                        _logger->error(__FILEREF__ + "fileUploadProgressCheck: remove entry because state is 'error'"
                            + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                            + ", progressId: " + itr->_progressId
                            + ", callFailures: " + to_string(itr->_callFailures)
                            + ", _maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
                        );
                        itr = _fileUploadProgressData->_filesUploadProgressToBeMonitored.erase(itr);	// returns iterator to the next element

                        iteratorAlreadyUpdated = true;
                    }
                    else if (state == "uploading")
                    {
                        int64_t relativeReceived = uploadProgressResponse.get("received", "XXX").asInt64();
                        int64_t absoluteReceived = -1;
                        if (itr->_contentRangePresent)
                            absoluteReceived    = relativeReceived + itr->_contentRangeStart;
                        int64_t relativeSize = uploadProgressResponse.get("size", "XXX").asInt64();
                        int64_t absoluteSize = -1;
                        if (itr->_contentRangePresent)
                            absoluteSize    = itr->_contentRangeSize;

                        double relativeProgress = ((double) relativeReceived / (double) relativeSize) * 100;
                        double absoluteProgress;
                        if (itr->_contentRangePresent)
                            absoluteProgress = ((double) absoluteReceived / (double) absoluteSize) * 100;
                            
                        // this is to have one decimal in the percentage
                        double relativeUploadingPercentage = ((double) ((int) (relativeProgress * 10))) / 10;
                        double absoluteUploadingPercentage;
                        if (itr->_contentRangePresent)
                            absoluteUploadingPercentage = ((double) ((int) (absoluteProgress * 10))) / 10;

                        if (itr->_contentRangePresent)
                        {
                            _logger->info(__FILEREF__ + "Upload still running"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
                                + ", relativeProgress: " + to_string(relativeProgress)
                                + ", absoluteProgress: " + to_string(absoluteProgress)
                                + ", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
                                + ", relativeReceived: " + to_string(relativeReceived)
                                + ", absoluteReceived: " + to_string(absoluteReceived)
                                + ", relativeSize: " + to_string(relativeSize)
                                + ", absoluteSize: " + to_string(absoluteSize)
                                + ", relativeUploadingPercentage: " + to_string(relativeUploadingPercentage)
                                + ", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
                            );
                        }
                        else
                        {
                            _logger->info(__FILEREF__ + "Upload still running"
                                + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                + ", progressId: " + itr->_progressId
                                + ", progress: " + to_string(relativeProgress)
                                + ", lastPercentageUpdated: " + to_string(itr->_lastPercentageUpdated)
                                + ", received: " + to_string(relativeReceived)
                                + ", size: " + to_string(relativeSize)
                                + ", uploadingPercentage: " + to_string(relativeUploadingPercentage)
                            );
                        }

                        if (itr->_contentRangePresent)
                        {
                            if (itr->_lastPercentageUpdated != absoluteUploadingPercentage)
                            {
                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                    + ", progressId: " + itr->_progressId
                                    + ", absoluteUploadingPercentage: " + to_string(absoluteUploadingPercentage)
                                );                            
                                _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                    itr->_ingestionJobKey, absoluteUploadingPercentage);

                                itr->_lastPercentageUpdated = absoluteUploadingPercentage;
                            }
                        }
                        else
                        {
                            if (itr->_lastPercentageUpdated != relativeUploadingPercentage)
                            {
                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                                    + ", progressId: " + itr->_progressId
                                    + ", uploadingPercentage: " + to_string(relativeUploadingPercentage)
                                );                            
                                _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                    itr->_ingestionJobKey, relativeUploadingPercentage);

                                itr->_lastPercentageUpdated = relativeUploadingPercentage;
                            }
                        }
                    }
                    else
                    {
                        string errorMessage = string("file upload progress. State is wrong")
                            + ", state: " + state
                            + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                            + ", progressId: " + itr->_progressId
                            + ", callFailures: " + to_string(itr->_callFailures)
                            + ", progressURL: " + progressURL
                            + ", progressIdHeader: " + progressIdHeader
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                catch(...)
                {
                    string errorMessage = string("response Body json is not well format")
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            catch (curlpp::LogicError & e) 
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (LogicError)"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", exception: " + e.what()
                );

                itr->_callFailures = itr->_callFailures + 1;
            }
            catch (curlpp::RuntimeError & e) 
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (RuntimeError)"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", exception: " + e.what()
                );

                itr->_callFailures = itr->_callFailures + 1;
            }
            catch (runtime_error e)
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (runtime_error)"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", exception: " + e.what()
                );

                itr->_callFailures = itr->_callFailures + 1;
            }
            catch (exception e)
            {
                _logger->error(__FILEREF__ + "Call for upload progress failed (exception)"
                    + ", ingestionJobKey: " + to_string(itr->_ingestionJobKey)
                    + ", progressId: " + itr->_progressId
                    + ", callFailures: " + to_string(itr->_callFailures)
                    + ", exception: " + e.what()
                );

                itr->_callFailures = itr->_callFailures + 1;
            }

            if (!iteratorAlreadyUpdated)
                itr++;
        }
    }
}

void API::registerUser(
        FCGX_Request& request,
        string requestBody)
{
    string api = "registerUser";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        string name;
        string email;
        string password;
        string workspaceName;
        string country;
        MMSEngineDBFacade::EncodingPriority encodingPriority;
        MMSEngineDBFacade::EncodingPeriod encodingPeriod;
        int maxIngestionsNumber;
        int maxStorageInGB;

        Json::Value metadataRoot;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &metadataRoot, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = string("Json metadata failed during the parsing")
                        + ", errors: " + errors
                        + ", json data: " + requestBody
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(exception e)
        {
            string errorMessage = string("Json metadata failed during the parsing"
                    ", json data: " + requestBody
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        {
            vector<string> mandatoryFields = {
                "WorkspaceName",
                "Name",
                "EMail",
                "Password",
                "Country"
            };
            for (string field: mandatoryFields)
            {
                if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
                {
                    string errorMessage = string("Json field is not present or it is null")
                            + ", Json field: " + field;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }

            workspaceName = metadataRoot.get("WorkspaceName", "XXX").asString();
            email = metadataRoot.get("EMail", "XXX").asString();
            password = metadataRoot.get("Password", "XXX").asString();
            name = metadataRoot.get("Name", "XXX").asString();
            country = metadataRoot.get("Country", "XXX").asString();
        }

        encodingPriority = _encodingPriorityWorkspaceDefaultValue;
        /*
        // encodingPriority
        {
            string field = "EncodingPriority";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "encodingPriority is not present, set the default value"
                    + ", _encodingPriorityWorkspaceDefaultValue: " + MMSEngineDBFacade::toString(_encodingPriorityWorkspaceDefaultValue)
                );

                encodingPriority = _encodingPriorityWorkspaceDefaultValue;
            }
            else
            {
                string sEncodingPriority = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    encodingPriority = MMSEngineDBFacade::toEncodingPriority(sEncodingPriority);
                }
                catch(exception e)
                {
                    string errorMessage = string("Json value is wrong. Correct values are: Low, Medium or High")
                            + ", Field: " + field
                            + ", Value: " + sEncodingPriority
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
        */

        encodingPeriod = _encodingPeriodWorkspaceDefaultValue;
        maxIngestionsNumber = _maxIngestionsNumberWorkspaceDefaultValue;
        /*
        // EncodingPeriod
        {
            string field = "EncodingPeriod";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "encodingPeriod is not present, set the default value"
                    + ", _encodingPeriodWorkspaceDefaultValue: " + MMSEngineDBFacade::toString(_encodingPeriodWorkspaceDefaultValue)
                );

                encodingPeriod = _encodingPeriodWorkspaceDefaultValue;
            }
            else
            {
                string sEncodingPeriod = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    encodingPeriod = MMSEngineDBFacade::toEncodingPeriod(sEncodingPeriod);
                }
                catch(exception e)
                {
                    string errorMessage = string("Json value is wrong. Correct values are: Daily, Weekly, Monthly or Yearly")
                            + ", Field: " + field
                            + ", Value: " + sEncodingPeriod
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        // MaxIngestionsNumber
        {
            string field = "MaxIngestionsNumber";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "MaxIngestionsNumber is not present, set the default value"
                    + ", _maxIngestionsNumberWorkspaceDefaultValue: " + to_string(_maxIngestionsNumberWorkspaceDefaultValue)
                );

                maxIngestionsNumber = _maxIngestionsNumberWorkspaceDefaultValue;
            }
            else
            {
                string sMaxIngestionsNumber = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    maxIngestionsNumber = stol(sMaxIngestionsNumber);
                }
                catch(exception e)
                {
                    string errorMessage = string("Json value is wrong, a number is expected")
                            + ", Field: " + field
                            + ", Value: " + sMaxIngestionsNumber
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
        */

        maxStorageInGB = _maxStorageInGBWorkspaceDefaultValue;
        /*
        // MaxStorageInGB
        {
            string field = "MaxStorageInGB";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "MaxStorageInGB is not present, set the default value"
                    + ", _maxStorageInGBWorkspaceDefaultValue: " + to_string(_maxStorageInGBWorkspaceDefaultValue)
                );

                maxStorageInGB = _maxStorageInGBWorkspaceDefaultValue;
            }
            else
            {
                string sMaxStorageInGB = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    maxStorageInGB = stol(sMaxStorageInGB);
                }
                catch(exception e)
                {
                    string errorMessage = string("Json value is wrong, a number is expected")
                            + ", Field: " + field
                            + ", Value: " + sMaxStorageInGB
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
        */

        try
        {
            string workspaceDirectoryName;

            workspaceDirectoryName.resize(workspaceName.size());

            transform(
                workspaceName.begin(), 
                workspaceName.end(), 
                workspaceDirectoryName.begin(), 
                [](unsigned char c){
                    if (isalpha(c)) 
                        return c; 
                    else 
                        return (unsigned char) '_'; } 
            );

            _logger->info(__FILEREF__ + "Registering User"
                + ", workspaceName: " + workspaceName
                + ", email: " + email
            );
            
            tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode = 
                _mmsEngineDBFacade->registerUser(
                    name, 
                    email, 
                    password,
                    country, 
                    workspaceName,
                    workspaceDirectoryName,
                    MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,  // MMSEngineDBFacade::WorkspaceType workspaceType
                    "",                             // string deliveryURL,
                    encodingPriority,               //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
                    encodingPeriod,                 //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
                    maxIngestionsNumber,            // long maxIngestionsNumber,
                    maxStorageInGB,                 // long maxStorageInGB,
                    "",                             // string languageCode,
                    chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
                );

            _logger->info(__FILEREF__ + "Registered User"
                + ", workspaceName: " + workspaceName
                + ", email: " + email
                + ", userKey: " + to_string(get<1>(workspaceKeyUserKeyAndConfirmationCode))
                + ", confirmationCode: " + get<2>(workspaceKeyUserKeyAndConfirmationCode)
            );
            
            string responseBody = string("{ ")
                + "\"userKey\": " + to_string(get<1>(workspaceKeyUserKeyAndConfirmationCode)) + " "
                + "}";
            sendSuccess(request, 201, responseBody);
            
            string to = "giulianoc@catrasoftware.it";
            string subject = "Confirmation code";
            
            vector<string> emailBody;
            emailBody.push_back("<p>Hi John,</p>");
            emailBody.push_back(string("<p>This is the confirmation code ") + get<2>(workspaceKeyUserKeyAndConfirmationCode) + "</p>");
            emailBody.push_back(string("<p>for the workspace key ") + to_string(get<0>(workspaceKeyUserKeyAndConfirmationCode)) + "</p>");
            emailBody.push_back("<p>Bye!</p>");

            sendEmail(to, subject, emailBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::confirmUser(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters)
{
    string api = "confirmUser";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto confirmationCodeIt = queryParameters.find("confirmationeCode");
        if (confirmationCodeIt == queryParameters.end())
        {
            string errorMessage = string("The 'confirmationeCode' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        try
        {
            string apiKey = _mmsEngineDBFacade->confirmUser(confirmationCodeIt->second);

            string responseBody = string("{ ")
                + "\"apiKey\": \"" + apiKey + "\" "
                + "}";
            sendSuccess(request, 201, responseBody);
            
            string to = "giulianoc@catrasoftware.it";
            string subject = "Welcome";
            
            vector<string> emailBody;
            emailBody.push_back("<p>Hi John,</p>");
            emailBody.push_back(string("<p>Your registration is now completed and you can start working with ...</p>"));
            emailBody.push_back("<p>Bye!</p>");

            sendEmail(to, subject, emailBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::createDeliveryAuthorization(
        FCGX_Request& request,
        int64_t userKey,
        shared_ptr<Workspace> requestWorkspace,
        string clientIPAddress,
        unordered_map<string, string> queryParameters)
{
    string api = "createDeliveryAuthorization";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        int64_t physicalPathKey;
        auto physicalPathKeyIt = queryParameters.find("physicalPathKey");
        if (physicalPathKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'physicalPathKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        physicalPathKey = stoll(physicalPathKeyIt->second);

        int ttlInSeconds = _defaultTTLInSeconds;
        auto ttlInSecondsIt = queryParameters.find("ttlInSeconds");
        if (ttlInSecondsIt != queryParameters.end())
        {
            ttlInSeconds = stol(ttlInSecondsIt->second);
        }

        int maxRetries = _defaultMaxRetries;
        auto maxRetriesIt = queryParameters.find("maxRetries");
        if (maxRetriesIt != queryParameters.end())
        {
            maxRetries = stol(maxRetriesIt->second);
        }
        
        bool redirect = _defaultRedirect;
        auto redirectIt = queryParameters.find("redirect");
        if (redirectIt != queryParameters.end())
        {
            if (redirectIt->second == "false")
                redirect = false;
            else if (redirectIt->second == "true")
                redirect = true;
        }
        
        try
        {
            tuple<int,shared_ptr<Workspace>,string,string> storageDetails =
                _mmsEngineDBFacade->getStorageDetails(physicalPathKey);

            int mmsPartitionNumber;
            shared_ptr<Workspace> contentWorkspace;
            string relativePath;
            string fileName;
            tie(mmsPartitionNumber, contentWorkspace, relativePath, fileName) = storageDetails;
    
            if (contentWorkspace->_workspaceKey != requestWorkspace->_workspaceKey)
            {
                string errorMessage = string ("Workspace of the content and Workspace of the requester is different")
                        + ", contentWorkspace->_workspaceKey: " + to_string(contentWorkspace->_workspaceKey)
                        + ", requestWorkspace->_workspaceKey: " + to_string(requestWorkspace->_workspaceKey)
                        ;
                _logger->error(__FILEREF__ + errorMessage);
                
                throw runtime_error(errorMessage);
            }
            
            string outputFileName = "zzzz";

            string deliveryURI;
            {
                char pMMSPartitionName [64];


                // .../storage/MMSRepository/MMS_0000/CatraSoft/000/000/550/815_source.jpg 

                sprintf(pMMSPartitionName, "/MMS_%04d/", mmsPartitionNumber);

                deliveryURI = 
                    + pMMSPartitionName
                    + contentWorkspace->_directoryName
                    + relativePath
                    + fileName
                ;
            }
            int64_t authorizationKey = _mmsEngineDBFacade->createDeliveryAuthorization(
                userKey,
                clientIPAddress,
                physicalPathKey,
                deliveryURI,
                ttlInSeconds,
                maxRetries);

            string deliveryURL = 
                    _deliveryProtocol
                    + "://" 
                    + _deliveryHost
                    + deliveryURI
                    + "?token=" + to_string(authorizationKey)
                    + "&outputFileName=" + outputFileName
            ;
            
            if (redirect)
            {
                sendRedirect(request, deliveryURL);
            }
            else
            {
                string responseBody = string("{ ")
                    + "\"deliveryURL\": \"" + deliveryURL + "\""
                    + ", \"outputFileName\": \"" + outputFileName + "\""
                    + ", \"authorizationKey\": " + to_string(authorizationKey)
                    + ", \"ttlInSeconds\": " + to_string(ttlInSeconds)
                    + ", \"maxRetries\": " + to_string(maxRetries)
                    + " }";
                sendSuccess(request, 201, responseBody);
            }
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::ingestionStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestionStatus";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        auto ingestionRootKeyIt = queryParameters.find("ingestionRootKey");
        if (ingestionRootKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestionRootKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        
        {
            Json::Value ingestionStatusRoot = _mmsEngineDBFacade->getIngestionJobStatus(
                    workspace->_workspaceKey,
                    stoll(ingestionRootKeyIt->second),
                    ingestionJobKeyIt->second == "" ? -1 : stoll(ingestionJobKeyIt->second)
                    );

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, ingestionStatusRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::contentList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "contentList";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t mediaItemKey = -1;
        auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
        if (mediaItemKeyIt != queryParameters.end())
        {
            mediaItemKey = stoll(mediaItemKeyIt->second);
        }

        int start = 0;
        auto startIt = queryParameters.find("start");
        if (startIt != queryParameters.end())
        {
            start = stoll(startIt->second);
        }

        int rows = 10;
        auto rowsIt = queryParameters.find("rows");
        if (rowsIt != queryParameters.end())
        {
            rows = stoll(rowsIt->second);
        }
        
        bool contentTypePresent = false;
        MMSEngineDBFacade::ContentType contentType;
        auto contentTypeIt = queryParameters.find("contentType");
        if (contentTypeIt != queryParameters.end())
        {
            contentType = MMSEngineDBFacade::toContentType(contentTypeIt->second);
            
            contentTypePresent = true;
        }
        
        bool startAndEndIngestionDatePresent = false;
        string startIngestionDate;
        string endIngestionDate;
        auto startIngestionDateIt = queryParameters.find("startIngestionDate");
        auto endIngestionDateIt = queryParameters.find("endIngestionDate");
        if (startIngestionDateIt != queryParameters.end() && endIngestionDateIt != queryParameters.end())
        {
            startIngestionDate = startIngestionDateIt->second;
            endIngestionDate = endIngestionDateIt->second;
            
            startAndEndIngestionDatePresent = true;
        }

        {
            
            Json::Value ingestionStatusRoot = _mmsEngineDBFacade->getContentList(
                    workspace->_workspaceKey, mediaItemKey,
                    start, rows,
                    contentTypePresent, contentType,
                    startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, ingestionStatusRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::ingestion(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestion";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        Json::Value requestBodyRoot;
        try
        {
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &requestBodyRoot, &errors);
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

            string field = "Variables";
            if (_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                Json::Value variablesRoot = requestBodyRoot[field];
                if (variablesRoot.begin() != variablesRoot.end())
                {
                    string localRequestBody = requestBody;
                    
                    _logger->info(__FILEREF__ + "variables processing...");
                    
                    for(Json::Value::iterator it = variablesRoot.begin(); it != variablesRoot.end(); ++it)
                    {
                        Json::Value key = it.key();
                        Json::Value value = (*it);
                        
                        Json::StreamWriterBuilder wbuilder;
                        string sKey = Json::writeString(wbuilder, key);
                        if (sKey.length() > 2)  // to remove the first and last "
                            sKey = sKey.substr(1, sKey.length() - 2);
                        string sValue = Json::writeString(wbuilder, value);        
                        if (sValue.length() > 2)    // to remove the first and last "
                            sValue = sValue.substr(1, sValue.length() - 2);
                        
                        // string variableToBeReplaced = string("\\$\\{") + sKey + "\\}";
                        // localRequestBody = regex_replace(localRequestBody, regex(variableToBeReplaced), sValue);
                        string variableToBeReplaced = string("${") + sKey + "}";
                        _logger->info(__FILEREF__ + "requestBody, replace"
                            + ", variableToBeReplaced: " + variableToBeReplaced
                            + ", sValue: " + sValue
                        );
                        size_t index = 0;
                        while (true) 
                        {
                             // Locate the substring to replace.
                             index = localRequestBody.find(variableToBeReplaced, index);
                             if (index == string::npos) 
                                 break;

                             // Make the replacement.
                             localRequestBody.replace(index, variableToBeReplaced.length(), sValue);

                             // Advance index forward so the next iteration doesn't pick it up as well.
                             index += sValue.length();
                        }
                    }
                    
                    _logger->info(__FILEREF__ + "requestBody after the replacement of the variables"
                        + ", localRequestBody: " + localRequestBody
                    );
                    
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(localRequestBody.c_str(),
                                localRequestBody.c_str() + localRequestBody.size(), 
                                &requestBodyRoot, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = __FILEREF__ + "failed to parse the localRequestBody"
                                    + ", errors: " + errors
                                    + ", localRequestBody: " + localRequestBody
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                }
            }
        }
        catch(runtime_error e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        string responseBody;    
        shared_ptr<MySQLConnection> conn;

        try
        {       
            // used to save <label of the task> ---> vector of ingestionJobKey. A vector is used in case the same label is used more times
            // It is used when ReferenceLabel is used.
            unordered_map<string, vector<int64_t>> mapLabelAndIngestionJobKey;
            
            conn = _mmsEngineDBFacade->beginIngestionJobs();

            Validator validator(_logger, _mmsEngineDBFacade);
            validator.validateRootMetadata(workspace->_workspaceKey, requestBodyRoot);
        
            string field = "Type";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            string rootType = requestBodyRoot.get(field, "XXX").asString();

            string rootLabel;
            field = "Label";
            if (_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                rootLabel = requestBodyRoot.get(field, "XXX").asString();
            }    
            
            int64_t ingestionRootKey = _mmsEngineDBFacade->addIngestionRoot(conn,
                workspace->_workspaceKey, rootType, rootLabel);
    
            field = "Task";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            Json::Value taskRoot = requestBodyRoot[field];                        
            
            field = "Type";
            if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            string taskType = taskRoot.get(field, "XXX").asString();
            
            if (taskType == "GroupOfTasks")
            {                
                vector<int64_t> dependOnIngestionJobKeysExecution;
                int localDependOnSuccess = 0;   // it is not important since dependOnIngestionJobKey is -1
                ingestionGroupOfTasks(conn, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysExecution, localDependOnSuccess,
                        dependOnIngestionJobKeysExecution,
                        mapLabelAndIngestionJobKey, responseBody); 
            }
            else
            {
                vector<int64_t> dependOnIngestionJobKeysExecution;
                int localDependOnSuccess = 0;   // it is not important since dependOnIngestionJobKey is -1
                ingestionSingleTask(conn, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysExecution, localDependOnSuccess,
                        dependOnIngestionJobKeysExecution, mapLabelAndIngestionJobKey,
                        responseBody);            
            }

            bool commit = true;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);
            
            string beginOfResponseBody = string("{ ")
                + "\"workflow\": { "
                    + "\"ingestionRootKey\": " + to_string(ingestionRootKey)
                    + ", \"label\": \"" + rootLabel + "\" "
                    + "}, "
                    + "\"tasks\": [ ";
            responseBody.insert(0, beginOfResponseBody);
            responseBody += " ] }";
        }
        catch(runtime_error e)
        {
            bool commit = false;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            bool commit = false;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

vector<int64_t> API::ingestionSingleTask(shared_ptr<MySQLConnection> conn,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey, Json::Value taskRoot, 
        vector<int64_t> dependOnIngestionJobKeysExecution, int dependOnSuccess,
        vector<int64_t> dependOnIngestionJobKeysReferences,
        unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
        string& responseBody)
{
    string field = "Type";
    string type = taskRoot.get(field, "XXX").asString();

    string taskLabel;
    field = "Label";
    if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
    {
        taskLabel = taskRoot.get(field, "XXX").asString();
    }
    
    field = "Parameters";
    Json::Value parametersRoot;
    bool parametersSectionPresent = false;
    if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
    {
        parametersRoot = taskRoot[field];
        
        parametersSectionPresent = true;
    }
    
    string encodingProfilesSetKeyField = "EncodingProfilesSetKey";
    string encodingProfilesSetLabelField = "EncodingProfilesSetLabel";
    if (type == "Encode"
            && parametersSectionPresent
            && 
            (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetKeyField)
            || _mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetLabelField)
            )
    )
    {
        // to manage the encode of 'profiles set' we will replace the single Task with
        // a GroupOfTasks where every task is just for one profile
        
        string encodingProfilesSetReference;
        
        vector<int64_t> encodingProfilesSetKeys;
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetKeyField))
        {
            int64_t encodingProfilesSetKey = parametersRoot.get(encodingProfilesSetKeyField, "XXX").asInt64();
        
            encodingProfilesSetReference = to_string(encodingProfilesSetKey);
            
            encodingProfilesSetKeys = 
                _mmsEngineDBFacade->getEncodingProfileKeysBySetKey(
                workspace->_workspaceKey, encodingProfilesSetKey);
        }
        else // if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetLabelField))
        {
            string encodingProfilesSetLabel = parametersRoot.get(encodingProfilesSetLabelField, "XXX").asString();
        
            encodingProfilesSetReference = encodingProfilesSetLabel;
            
            encodingProfilesSetKeys = 
                _mmsEngineDBFacade->getEncodingProfileKeysBySetLabel(
                    workspace->_workspaceKey, encodingProfilesSetLabel);
        }
        
        if (encodingProfilesSetKeys.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No EncodingProfileKey into the EncodingProfilesSetKey"
                    + ", EncodingProfilesSetKey/EncodingProfilesSetLabel: " + encodingProfilesSetReference;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string encodingPriority;
        field = "EncodingPriority";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string sRequestedEncodingPriority = parametersRoot.get(field, "XXX").asString();
            MMSEngineDBFacade::EncodingPriority requestedEncodingPriority = 
                    MMSEngineDBFacade::toEncodingPriority(sRequestedEncodingPriority);
            
            if (static_cast<int>(requestedEncodingPriority) > workspace->_maxEncodingPriority)
                encodingPriority = MMSEngineDBFacade::toString(
                        static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority));
            else
                encodingPriority = MMSEngineDBFacade::toString(requestedEncodingPriority);
        }
        else
        {
            encodingPriority = MMSEngineDBFacade::toString(
                    static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority));
        }
        
            
        Json::Value newTasksRoot(Json::arrayValue);
        
        for (int64_t encodingProfileKey: encodingProfilesSetKeys)
        {
            Json::Value newTaskRoot;
            string localLabel = taskLabel + " - EncodingProfileKey " + to_string(encodingProfileKey);

            field = "Label";
            newTaskRoot[field] = localLabel;
            
            field = "Type";
            newTaskRoot[field] = "Encode";
            
            Json::Value newParametersRoot;
            
            field = "References";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                newParametersRoot[field] = parametersRoot[field];
            }
            
            field = "EncodingProfileKey";
            newParametersRoot[field] = encodingProfileKey;
            
            field = "EncodingPriority";
            newParametersRoot[field] = encodingPriority;
            
            field = "Parameters";
            newTaskRoot[field] = newParametersRoot;
            
            newTasksRoot.append(newTaskRoot);
        }
        
        Json::Value newParametersTasksGroupRoot;

        field = "ExecutionType";
        newParametersTasksGroupRoot[field] = "parallel";

        field = "Tasks";
        newParametersTasksGroupRoot[field] = newTasksRoot;
        
        Json::Value newTasksGroupRoot;

        field = "Type";
        newTasksGroupRoot[field] = "GroupOfTasks";

        field = "Parameters";
        newTasksGroupRoot[field] = newParametersTasksGroupRoot;
        
        field = "OnSuccess";
        if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            newTasksGroupRoot[field] = taskRoot[field];
        }

        field = "OnError";
        if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            newTasksGroupRoot[field] = taskRoot[field];
        }

        field = "OnComplete";
        if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            newTasksGroupRoot[field] = taskRoot[field];
        }
        
        return ingestionGroupOfTasks(conn, workspace, ingestionRootKey, newTasksGroupRoot, 
                dependOnIngestionJobKeysExecution, dependOnSuccess,
                dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                responseBody); 
    }

    bool referencesSectionPresent = false;
    Json::Value referencesRoot(Json::arrayValue);
    if (parametersSectionPresent)
    {
        field = "References";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            referencesRoot = parametersRoot[field];

            referencesSectionPresent = true;
        }
    }
    
    if (referencesSectionPresent)
    {
        bool referencesChanged = false;
        
        for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); ++referenceIndex)
        {
            Json::Value referenceRoot = referencesRoot[referenceIndex];
            
            field = "ReferenceLabel";
            if (_mmsEngineDBFacade->isMetadataPresent(referenceRoot, field))
            {
                string referenceLabel = referenceRoot.get(field, "XXX").asString();
                
                if (referenceLabel == "")
                {
                    string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be empty"
                            + ", referenceLabel: " + referenceLabel;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                
                vector<int64_t> ingestionJobKeys = mapLabelAndIngestionJobKey[referenceLabel];
                
                if (ingestionJobKeys.size() == 0)
                {
                    string errorMessage = __FILEREF__ + "The 'referenceLabel' value is not found"
                            + ", referenceLabel: " + referenceLabel;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                else if (ingestionJobKeys.size() > 1)
                {
                    string errorMessage = __FILEREF__ + "The 'referenceLabel' value cannot be used in more than one Task"
                            + ", referenceLabel: " + referenceLabel
                            + ", ingestionJobKeys.size(): " + to_string(ingestionJobKeys.size())
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }

                field = "ReferenceIngestionJobKey";
                referenceRoot[field] = ingestionJobKeys.back();
                
                referencesRoot[referenceIndex] = referenceRoot;
                
                field = "References";
                parametersRoot[field] = referencesRoot;
            
                referencesChanged = true;
                
                // The workflow specifies expliticily a reference (input for the task).
                // Probable this is because the Reference is not part of the
                // 'dependOnIngestionJobKeysReferences' parameter that it is generally
                // same of 'dependOnIngestionJobKeysExecution'.
                // For this reason we have to make sure this Reference is inside
                // dependOnIngestionJobKeysExecution in order to avoid the Task starts
                // when the input is not yet ready
                vector<int64_t>::iterator itrIngestionJobKey = find(
                        dependOnIngestionJobKeysExecution.begin(), dependOnIngestionJobKeysExecution.end(), 
                        ingestionJobKeys.back());
                if (itrIngestionJobKey == dependOnIngestionJobKeysExecution.end())
                    dependOnIngestionJobKeysExecution.push_back(ingestionJobKeys.back());
            }
        }
        
        /*
        if (referencesChanged)
        {
            {
                Json::StreamWriterBuilder wbuilder;

                taskMetadata = Json::writeString(wbuilder, parametersRoot);        
            }

            // commented because already logged in mmsEngineDBFacade
            // _logger->info(__FILEREF__ + "update IngestionJob"
            //     + ", localDependOnIngestionJobKey: " + to_string(localDependOnIngestionJobKey)
            //    + ", taskMetadata: " + taskMetadata
            // );

            _mmsEngineDBFacade->updateIngestionJobMetadataContent(conn, localDependOnIngestionJobKeyExecution, taskMetadata);
        }
        */
    }
    else if (dependOnIngestionJobKeysReferences.size() > 0)
    {
        for (int referenceIndex = 0; referenceIndex < dependOnIngestionJobKeysReferences.size(); ++referenceIndex)
        {
            Json::Value referenceRoot;
            string addedField = "ReferenceIngestionJobKey";
            referenceRoot[addedField] = dependOnIngestionJobKeysReferences.at(referenceIndex);
            
            referencesRoot.append(referenceRoot);
        }
        
        field = "Parameters";
        string arrayField = "References";
        parametersRoot[arrayField] = referencesRoot;
        if (!parametersSectionPresent)
        {
            taskRoot[field] = parametersRoot;
        }

        /*        
        {
            Json::StreamWriterBuilder wbuilder;

            taskMetadata = Json::writeString(wbuilder, parametersRoot);        
        }
        
        // commented because already logged in mmsEngineDBFacade
        // _logger->info(__FILEREF__ + "update IngestionJob"
        //     + ", localDependOnIngestionJobKey: " + to_string(localDependOnIngestionJobKey)
        //    + ", taskMetadata: " + taskMetadata
        // );

        _mmsEngineDBFacade->updateIngestionJobMetadataContent(conn, localDependOnIngestionJobKeyExecution, taskMetadata);
        */
    }

    string taskMetadata;

    if (parametersSectionPresent)
    {                
        Json::StreamWriterBuilder wbuilder;

        taskMetadata = Json::writeString(wbuilder, parametersRoot);        
    }
    
    _logger->info(__FILEREF__ + "add IngestionJob"
        + ", taskLabel: " + taskLabel
        + ", taskMetadata: " + taskMetadata
        + ", IngestionType: " + type
        + ", dependOnIngestionJobKeysExecution.size(): " + to_string(dependOnIngestionJobKeysExecution.size())
        + ", dependOnSuccess: " + to_string(dependOnSuccess)
    );

    int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(conn,
            workspace->_workspaceKey, ingestionRootKey, taskLabel, taskMetadata, MMSEngineDBFacade::toIngestionType(type), 
            dependOnIngestionJobKeysExecution, dependOnSuccess);
    
    if (taskLabel != "")
        (mapLabelAndIngestionJobKey[taskLabel]).push_back(localDependOnIngestionJobKeyExecution);
    
    if (responseBody != "")
        responseBody += ", ";    
    responseBody +=
            (string("{ ")
            + "\"ingestionJobKey\": " + to_string(localDependOnIngestionJobKeyExecution) + ", "
            + "\"label\": \"" + taskLabel + "\" "
            + "}");

    vector<int64_t> localDependOnIngestionJobKeysExecution;
    vector<int64_t> localDependOnIngestionJobKeysReferences;
    localDependOnIngestionJobKeysExecution.push_back(localDependOnIngestionJobKeyExecution);
    localDependOnIngestionJobKeysReferences.push_back(localDependOnIngestionJobKeyExecution);
    
    ingestionEvents(conn, workspace, ingestionRootKey, taskRoot, 
            localDependOnIngestionJobKeysExecution, 
            localDependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
            responseBody);
    
    
    return localDependOnIngestionJobKeysExecution;
}

vector<int64_t> API::ingestionGroupOfTasks(shared_ptr<MySQLConnection> conn,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
        Json::Value groupOfTasksRoot, 
        vector<int64_t> dependOnIngestionJobKeysExecution, int dependOnSuccess,
        vector<int64_t> dependOnIngestionJobKeysReferences,
        unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey, string& responseBody)
{
    
    string field = "Parameters";
    Json::Value parametersRoot;
    if (!_mmsEngineDBFacade->isMetadataPresent(groupOfTasksRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    parametersRoot = groupOfTasksRoot[field];

    bool parallelTasks;
    
    field = "ExecutionType";
    if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    string executionType = parametersRoot.get(field, "XXX").asString();
    if (executionType == "parallel")
        parallelTasks = true;
    else if (executionType == "sequential")
        parallelTasks = false;
    else
    {
        string errorMessage = __FILEREF__ + "executionType field is wrong"
                + ", executionType: " + executionType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    field = "Tasks";
    if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    Json::Value tasksRoot = parametersRoot[field];
                
    if (tasksRoot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No Tasks are present inside the GroupOfTasks item";
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    vector<int64_t> newDependOnIngestionJobKeysExecution;
    vector<int64_t> newDependOnIngestionJobKeysReferences;
    vector<int64_t> lastDependOnIngestionJobKeysExecution;
    for (int taskIndex = 0; taskIndex < tasksRoot.size(); ++taskIndex)
    {
        Json::Value taskRoot = tasksRoot[taskIndex];

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();
            
        vector<int64_t> localIngestionTaskDependOnIngestionJobKeyExecution;
        if (parallelTasks)
        {            
            if (taskType == "GroupOfTasks")
            {
                localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
                    conn, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysExecution, dependOnSuccess, 
                    dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                    responseBody);
            }
            else
            {
                localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
                    conn, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysExecution, dependOnSuccess, 
                    dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                    responseBody);
            }
        }
        else
        {
            if (taskIndex == 0)
            {
                if (taskType == "GroupOfTasks")
                {
                    localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
                        conn, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysExecution, dependOnSuccess, 
                        dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                        responseBody);
                }
                else
                {
                    localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
                        conn, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysExecution, dependOnSuccess,
                        dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                        responseBody);
                }
            }
            else
            {
                int localDependOnSuccess = -1;
                
                if (taskType == "GroupOfTasks")
                {
                    localIngestionTaskDependOnIngestionJobKeyExecution = ingestionGroupOfTasks(
                        conn, workspace, ingestionRootKey, taskRoot, 
                        lastDependOnIngestionJobKeysExecution, localDependOnSuccess, 
                        dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                        responseBody);
                }
                else
                {
                    localIngestionTaskDependOnIngestionJobKeyExecution = ingestionSingleTask(
                        conn, workspace, ingestionRootKey, taskRoot, 
                        lastDependOnIngestionJobKeysExecution, localDependOnSuccess,
                        dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                        responseBody);
                }
            }
            
            lastDependOnIngestionJobKeysExecution = localIngestionTaskDependOnIngestionJobKeyExecution;
        }

        for (int64_t localDependOnIngestionJobKey: localIngestionTaskDependOnIngestionJobKeyExecution)
        {
            newDependOnIngestionJobKeysExecution.push_back(localDependOnIngestionJobKey);
            newDependOnIngestionJobKeysReferences.push_back(localDependOnIngestionJobKey);
        }
    }

    ingestionEvents(conn, workspace, ingestionRootKey, groupOfTasksRoot, 
            newDependOnIngestionJobKeysExecution, 
            newDependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
            responseBody);
    
    return newDependOnIngestionJobKeysExecution;
}    

void API::ingestionEvents(shared_ptr<MySQLConnection> conn,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
        Json::Value taskOrGroupOfTasksRoot, 
        vector<int64_t> dependOnIngestionJobKeysExecution, 
        vector<int64_t> dependOnIngestionJobKeysReferences,
        unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
        string& responseBody)
{

    string field = "OnSuccess";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onSuccessRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!_mmsEngineDBFacade->isMetadataPresent(onSuccessRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value taskRoot = onSuccessRoot[field];                        

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            int localDependOnSuccess = 1;
            ingestionGroupOfTasks(conn, workspace, ingestionRootKey,
                    taskRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess, 
                    dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
        else
        {
            int localDependOnSuccess = 1;
            ingestionSingleTask(conn, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess, 
                    dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
    }

    field = "OnError";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onErrorRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!_mmsEngineDBFacade->isMetadataPresent(onErrorRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value taskRoot = onErrorRoot[field];                        

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            int localDependOnSuccess = 0;
            ingestionGroupOfTasks(conn, workspace, ingestionRootKey,
                    taskRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess, 
                    dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
        else
        {
            int localDependOnSuccess = 0;
            ingestionSingleTask(conn, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess, 
                    dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
    }    
    
    field = "OnComplete";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onCompleteRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!_mmsEngineDBFacade->isMetadataPresent(onCompleteRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value taskRoot = onCompleteRoot[field];                        

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            int localDependOnSuccess = -1;
            ingestionGroupOfTasks(conn, workspace, ingestionRootKey,
                    taskRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess, 
                    dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
        else
        {
            int localDependOnSuccess = -1;
            ingestionSingleTask(conn, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess, 
                    dependOnIngestionJobKeysReferences, mapLabelAndIngestionJobKey,
                    responseBody);            
        }
    }    
}

/*
void API::ingestion(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestion";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        Json::Value requestBodyRoot;
        try
        {
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &requestBodyRoot, &errors);
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

            string field = "Variables";
            if (_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                Json::Value variablesRoot = requestBodyRoot[field];
                if (variablesRoot.begin() != variablesRoot.end())
                {
                    string localRequestBody = requestBody;
                    
                    _logger->info(__FILEREF__ + "variables processing...");
                    
                    for(Json::Value::iterator it = variablesRoot.begin(); it != variablesRoot.end(); ++it)
                    {
                        Json::Value key = it.key();
                        Json::Value value = (*it);
                        
                        Json::StreamWriterBuilder wbuilder;
                        string sKey = Json::writeString(wbuilder, key);
                        if (sKey.length() > 2)  // to remove the first and last "
                            sKey = sKey.substr(1, sKey.length() - 2);
                        string sValue = Json::writeString(wbuilder, value);        
                        if (sValue.length() > 2)    // to remove the first and last "
                            sValue = sValue.substr(1, sValue.length() - 2);
                        
                        // string variableToBeReplaced = string("\\$\\{") + sKey + "\\}";
                        // localRequestBody = regex_replace(localRequestBody, regex(variableToBeReplaced), sValue);
                        string variableToBeReplaced = string("${") + sKey + "}";
                        _logger->info(__FILEREF__ + "requestBody, replace"
                            + ", variableToBeReplaced: " + variableToBeReplaced
                            + ", sValue: " + sValue
                        );
                        size_t index = 0;
                        while (true) 
                        {
                             // Locate the substring to replace.
                             index = localRequestBody.find(variableToBeReplaced, index);
                             if (index == string::npos) 
                                 break;

                             // Make the replacement.
                             localRequestBody.replace(index, variableToBeReplaced.length(), sValue);

                             // Advance index forward so the next iteration doesn't pick it up as well.
                             index += sValue.length();
                        }
                    }
                    
                    _logger->info(__FILEREF__ + "requestBody after the replacement of the variables"
                        + ", localRequestBody: " + localRequestBody
                    );
                    
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(localRequestBody.c_str(),
                                localRequestBody.c_str() + localRequestBody.size(), 
                                &requestBodyRoot, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = __FILEREF__ + "failed to parse the localRequestBody"
                                    + ", errors: " + errors
                                    + ", localRequestBody: " + localRequestBody
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                }
            }
        }
        catch(runtime_error e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        string responseBody;    
        shared_ptr<MySQLConnection> conn;

        try
        {            
            conn = _mmsEngineDBFacade->beginIngestionJobs();

            Validator validator(_logger, _mmsEngineDBFacade);
            validator.validateRootMetadata(workspace->_workspaceKey, requestBodyRoot);
        
            string field = "Type";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            string rootType = requestBodyRoot.get(field, "XXX").asString();

            string rootLabel;
            field = "Label";
            if (_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                rootLabel = requestBodyRoot.get(field, "XXX").asString();
            }    
            
            int64_t ingestionRootKey = _mmsEngineDBFacade->addIngestionRoot(conn,
                workspace->_workspaceKey, rootType, rootLabel);
    
            string taskField = "Task";
            string groupOfTasksField = "GroupOfTasks";
            if (_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, taskField))
            {
                Json::Value taskRoot = requestBodyRoot[taskField];                        

                vector<int64_t> dependOnIngestionJobKeysExecution;
                int localDependOnSuccess = 0;   // it is not important since dependOnIngestionJobKey is -1
                ingestionTask(conn, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysExecution, localDependOnSuccess,
                        dependOnIngestionJobKeysExecution,
                        responseBody);            
            }
            else if (_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, groupOfTasksField))
            {
                Json::Value groupOfTasksRoot = requestBodyRoot[groupOfTasksField];
                
                vector<int64_t> dependOnIngestionJobKeysExecution;
                int localDependOnSuccess = 0;   // it is not important since dependOnIngestionJobKey is -1
                ingestionGroupOfTasks(conn, workspace, ingestionRootKey, groupOfTasksRoot, 
                        dependOnIngestionJobKeysExecution, localDependOnSuccess,
                        dependOnIngestionJobKeysExecution,
                        responseBody); 
            }
            else
            {
                string errorMessage = __FILEREF__ + "Both Fields are not present or are null"
                        + ", Field: " + taskField
                        + ", Field: " + groupOfTasksField
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            bool commit = true;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);
            
            string beginOfResponseBody = string("{ ")
                + "\"workflow\": { "
                    + "\"ingestionRootKey\": " + to_string(ingestionRootKey)
                    + ", \"label\": \"" + rootLabel + "\" "
                    + "}, "
                    + "\"tasks\": [ ";
            responseBody.insert(0, beginOfResponseBody);
            responseBody += " ] }";
        }
        catch(runtime_error e)
        {
            bool commit = false;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            bool commit = false;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

vector<int64_t> API::ingestionTask(shared_ptr<MySQLConnection> conn,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey, Json::Value taskRoot, 
        vector<int64_t> dependOnIngestionJobKeysExecution, int dependOnSuccess,
        vector<int64_t> dependOnIngestionJobKeysReferences,
        string& responseBody)
{
    string label;
    string field = "Label";
    if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
    {
        label = taskRoot.get(field, "XXX").asString();
    }

    field = "Type";
    string type = taskRoot.get(field, "XXX").asString();

    
    string taskMetadata;

    string parametersField = "Parameters";
    Json::Value parametersRoot;
    if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, parametersField))
    {
        parametersRoot = taskRoot[parametersField];
        
        Json::StreamWriterBuilder wbuilder;

        taskMetadata = Json::writeString(wbuilder, parametersRoot);        
    }
    
    string encodingProfilesSetKeyField = "EncodingProfilesSetKey";
    string encodingProfilesSetLabelField = "EncodingProfilesSetLabel";
    if (type == "Encode"
            && _mmsEngineDBFacade->isMetadataPresent(taskRoot, parametersField)
            && 
            (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetKeyField)
            || _mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetLabelField)
            )
    )
    {
        // to manage the encode of 'profiles set' we will replace the single Task with
        // a GroupOfTasks where every task is just for one profile
        
        string encodingProfilesSetReference;
        
        vector<int64_t> encodingProfilesSetKeys;
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetKeyField))
        {
            int64_t encodingProfilesSetKey = parametersRoot.get(encodingProfilesSetKeyField, "XXX").asInt64();
        
            encodingProfilesSetReference = to_string(encodingProfilesSetKey);
            
            encodingProfilesSetKeys = 
                _mmsEngineDBFacade->getEncodingProfileKeysBySetKey(
                workspace->_workspaceKey, encodingProfilesSetKey);
        }
        else // if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, encodingProfilesSetLabelField))
        {
            string encodingProfilesSetLabel = parametersRoot.get(encodingProfilesSetLabelField, "XXX").asString();
        
            encodingProfilesSetReference = encodingProfilesSetLabel;
            
            encodingProfilesSetKeys = 
                _mmsEngineDBFacade->getEncodingProfileKeysBySetLabel(
                    workspace->_workspaceKey, encodingProfilesSetLabel);
        }
        
        if (encodingProfilesSetKeys.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No EncodingProfileKey into the EncodingProfilesSetKey"
                    + ", EncodingProfilesSetKey/EncodingProfilesSetLabel: " + encodingProfilesSetReference;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string encodingPriority;
        field = "EncodingPriority";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string sRequestedEncodingPriority = parametersRoot.get(field, "XXX").asString();
            MMSEngineDBFacade::EncodingPriority requestedEncodingPriority = 
                    MMSEngineDBFacade::toEncodingPriority(sRequestedEncodingPriority);
            
            if (static_cast<int>(requestedEncodingPriority) > workspace->_maxEncodingPriority)
                encodingPriority = MMSEngineDBFacade::toString(
                        static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority));
            else
                encodingPriority = MMSEngineDBFacade::toString(requestedEncodingPriority);
        }
        else
        {
            encodingPriority = MMSEngineDBFacade::toString(
                    static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority));
        }
        
            
        Json::Value newTasksRoot(Json::arrayValue);
        
        for (int64_t encodingProfileKey: encodingProfilesSetKeys)
        {
            Json::Value newTaskRoot;
            string localLabel = label + " - EncodingProfileKey " + to_string(encodingProfileKey);

            field = "Label";
            newTaskRoot[field] = localLabel;
            
            field = "Type";
            newTaskRoot[field] = "Encode";
            
            Json::Value newParametersRoot;
            
            field = "References";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                newParametersRoot[field] = parametersRoot[field];
            }
            
            field = "EncodingProfileKey";
            newParametersRoot[field] = encodingProfileKey;
            
            field = "EncodingPriority";
            newParametersRoot[field] = encodingPriority;
            
            field = "Parameters";
            newTaskRoot[field] = newParametersRoot;
            
            newTasksRoot.append(newTaskRoot);
        }
        
        Json::Value newGroupOfTasksRoot;

        field = "ExecutionType";
        newGroupOfTasksRoot[field] = "parallel";
        
        field = "Tasks";
        newGroupOfTasksRoot[field] = newTasksRoot;
        
        field = "OnSuccess";
        if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            newGroupOfTasksRoot[field] = taskRoot[field];
        }

        field = "OnError";
        if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            newGroupOfTasksRoot[field] = taskRoot[field];
        }

        field = "OnComplete";
        if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            newGroupOfTasksRoot[field] = taskRoot[field];
        }
        
        return ingestionGroupOfTasks(conn, workspace, ingestionRootKey, newGroupOfTasksRoot, 
                dependOnIngestionJobKeysExecution, dependOnSuccess,
                dependOnIngestionJobKeysReferences,
                responseBody); 
    }

    _logger->info(__FILEREF__ + "add IngestionJob"
        + ", label: " + label
        + ", taskMetadata: " + taskMetadata
        + ", IngestionType: " + type
        + ", dependOnIngestionJobKeysExecution.size(): " + to_string(dependOnIngestionJobKeysExecution.size())
        + ", dependOnSuccess: " + to_string(dependOnSuccess)
    );

    int64_t localDependOnIngestionJobKeyExecution = _mmsEngineDBFacade->addIngestionJob(conn,
            workspace->_workspaceKey, ingestionRootKey, label, taskMetadata, MMSEngineDBFacade::toIngestionType(type), 
            dependOnIngestionJobKeysExecution, dependOnSuccess);
    
    if (dependOnIngestionJobKeysExecution.size() > 0)
    {
        Json::Value referencesRoot(Json::arrayValue);
        
        for (int referenceIndex = 0; referenceIndex < dependOnIngestionJobKeysReferences.size(); ++referenceIndex)
        {
            Json::Value referenceRoot;
            string addedField = "ReferenceIngestionJobKey";
            referenceRoot[addedField] = dependOnIngestionJobKeysReferences.at(referenceIndex);
            
            referencesRoot.append(referenceRoot);
        }
        
        field = "Parameters";
        string arrayField = "References";
        taskRoot[field][arrayField] = referencesRoot;
        
        {
            Json::StreamWriterBuilder wbuilder;

            field = "Parameters";
            taskMetadata = Json::writeString(wbuilder, taskRoot[field]);        
        }
        
        // commented because already logged in mmsEngineDBFacade
        // _logger->info(__FILEREF__ + "update IngestionJob"
        //     + ", localDependOnIngestionJobKey: " + to_string(localDependOnIngestionJobKey)
        //    + ", taskMetadata: " + taskMetadata
        // );

        _mmsEngineDBFacade->updateIngestionJobMetadataContent(conn, localDependOnIngestionJobKeyExecution, taskMetadata);
    }

    if (responseBody != "")
        responseBody += ", ";    
    responseBody +=
            (string("{ ")
            + "\"ingestionJobKey\": " + to_string(localDependOnIngestionJobKeyExecution) + ", "
            + "\"label\": \"" + label + "\" "
            + "}");

    vector<int64_t> localDependOnIngestionJobKeysExecution;
    vector<int64_t> localDependOnIngestionJobKeysReferences;
    localDependOnIngestionJobKeysExecution.push_back(localDependOnIngestionJobKeyExecution);
    localDependOnIngestionJobKeysReferences.push_back(localDependOnIngestionJobKeyExecution);
    
    ingestionEvents(conn, workspace, ingestionRootKey, taskRoot, 
            localDependOnIngestionJobKeysExecution, 
            localDependOnIngestionJobKeysReferences, responseBody);
    
    
    return localDependOnIngestionJobKeysExecution;
}

vector<int64_t> API::ingestionGroupOfTasks(shared_ptr<MySQLConnection> conn,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
        Json::Value groupOfTasksRoot, 
        vector<int64_t> dependOnIngestionJobKeysExecution, int dependOnSuccess,
        vector<int64_t> dependOnIngestionJobKeysReferences,
        string& responseBody)
{
    bool parallelTasks;
    
    string field = "ExecutionType";
    if (!_mmsEngineDBFacade->isMetadataPresent(groupOfTasksRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string executionType = groupOfTasksRoot.get(field, "XXX").asString();
    if (executionType == "parallel")
        parallelTasks = true;
    else if (executionType == "sequential")
        parallelTasks = false;
    else
    {
        string errorMessage = __FILEREF__ + "executionType field is wrong"
                + ", executionType: " + executionType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    field = "Tasks";
    if (!_mmsEngineDBFacade->isMetadataPresent(groupOfTasksRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    Json::Value tasksRoot = groupOfTasksRoot[field];
                
    if (tasksRoot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No Tasks are present inside the GroupOfTasks item";
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    vector<int64_t> newDependOnIngestionJobKeysExecution;
    vector<int64_t> newDependOnIngestionJobKeysReferences;
    vector<int64_t> lastDependOnIngestionJobKeysExecution;
    for (int taskIndex = 0; taskIndex < tasksRoot.size(); ++taskIndex)
    {
        Json::Value taskRoot = tasksRoot[taskIndex];
        
        vector<int64_t> localIngestionTaskDependOnIngestionJobKeyExecution;
        if (parallelTasks)
        {
            localIngestionTaskDependOnIngestionJobKeyExecution = ingestionTask(
                    conn, workspace, ingestionRootKey, taskRoot, 
                    dependOnIngestionJobKeysExecution, dependOnSuccess, 
                    dependOnIngestionJobKeysReferences,
                    responseBody);
        }
        else
        {
            if (taskIndex == 0)
            {
                localIngestionTaskDependOnIngestionJobKeyExecution = ingestionTask(
                        conn, workspace, ingestionRootKey, taskRoot, 
                        dependOnIngestionJobKeysExecution, dependOnSuccess,
                        dependOnIngestionJobKeysReferences,
                        responseBody);
            }
            else
            {
                int localDependOnSuccess = -1;
                
                localIngestionTaskDependOnIngestionJobKeyExecution = ingestionTask(
                        conn, workspace, ingestionRootKey, taskRoot, 
                        lastDependOnIngestionJobKeysExecution, localDependOnSuccess,
                        dependOnIngestionJobKeysReferences,
                        responseBody);
            }
            
            lastDependOnIngestionJobKeysExecution = localIngestionTaskDependOnIngestionJobKeyExecution;
        }

        for (int64_t localDependOnIngestionJobKey: localIngestionTaskDependOnIngestionJobKeyExecution)
        {
            newDependOnIngestionJobKeysExecution.push_back(localDependOnIngestionJobKey);
            newDependOnIngestionJobKeysReferences.push_back(localDependOnIngestionJobKey);
        }
    }

    ingestionEvents(conn, workspace, ingestionRootKey, groupOfTasksRoot, 
            newDependOnIngestionJobKeysExecution, 
            newDependOnIngestionJobKeysReferences, responseBody);
    
    return newDependOnIngestionJobKeysExecution;
}    

void API::ingestionEvents(shared_ptr<MySQLConnection> conn,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey, Json::Value taskOrGroupOfTasksRoot, 
        vector<int64_t> dependOnIngestionJobKeysExecution, 
        vector<int64_t> dependOnIngestionJobKeysReferences,
        string& responseBody)
{

    string field = "OnSuccess";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onSuccessRoot = taskOrGroupOfTasksRoot[field];
        
        string taskField = "Task";
        string groupOfTasksField = "GroupOfTasks";
        if (_mmsEngineDBFacade->isMetadataPresent(onSuccessRoot, taskField))
        {
            Json::Value onSuccessTaskRoot = onSuccessRoot[taskField];                        

            int localDependOnSuccess = 1;
            ingestionTask(conn, workspace, ingestionRootKey, onSuccessTaskRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess, 
                    dependOnIngestionJobKeysReferences,
                    responseBody);            
        }
        else if (_mmsEngineDBFacade->isMetadataPresent(onSuccessRoot, groupOfTasksField))
        {
            Json::Value onSuccessGroupOfTasksRoot = onSuccessRoot[groupOfTasksField];                        

            int localDependOnSuccess = 1;
            ingestionGroupOfTasks(conn, workspace, ingestionRootKey,
                    onSuccessGroupOfTasksRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess, 
                    dependOnIngestionJobKeysReferences,
                    responseBody);            
        }
    }

    field = "OnError";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onErrorRoot = taskOrGroupOfTasksRoot[field];
        
        string taskField = "Task";
        string groupOfTasksField = "GroupOfTasks";
        if (_mmsEngineDBFacade->isMetadataPresent(onErrorRoot, taskField))
        {
            Json::Value onErrorTaskRoot = onErrorRoot[taskField];                        

            int localDependOnSuccess = 0;
            ingestionTask(conn, workspace, ingestionRootKey, onErrorTaskRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess,
                    dependOnIngestionJobKeysReferences,
                    responseBody);            
        }
        else if (_mmsEngineDBFacade->isMetadataPresent(onErrorRoot, groupOfTasksField))
        {
            Json::Value onErrorGroupOfTasksRoot = onErrorRoot[groupOfTasksField];                        

            int localDependOnSuccess = 0;
            ingestionGroupOfTasks(conn, workspace, ingestionRootKey,
                    onErrorGroupOfTasksRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess,
                    dependOnIngestionJobKeysReferences,
                    responseBody);            
        }
    }    
    
    field = "OnComplete";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onCompleteRoot = taskOrGroupOfTasksRoot[field];
        
        string taskField = "Task";
        string groupOfTasksField = "GroupOfTasks";
        if (_mmsEngineDBFacade->isMetadataPresent(onCompleteRoot, taskField))
        {
            Json::Value onCompleteTaskRoot = onCompleteRoot[taskField];                        

            int localDependOnSuccess = -1;
            ingestionTask(conn, workspace, ingestionRootKey, onCompleteTaskRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess,
                    dependOnIngestionJobKeysReferences,
                    responseBody);            
        }
        else if (_mmsEngineDBFacade->isMetadataPresent(onCompleteRoot, groupOfTasksField))
        {
            Json::Value onCompleteGroupOfTasksRoot = onCompleteRoot[groupOfTasksField];                        

            int localDependOnSuccess = -1;
            ingestionGroupOfTasks(conn, workspace, ingestionRootKey,
                    onCompleteGroupOfTasksRoot, 
                    dependOnIngestionJobKeysExecution, localDependOnSuccess,
                    dependOnIngestionJobKeysReferences,
                    responseBody);            
        }
    }    
}
*/

void API::uploadedBinary(
        FCGX_Request& request,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool> userKeyWorkspaceAndFlags,
        // unsigned long contentLength,
        unordered_map<string, string>& requestDetails
)
{
    string api = "uploadedBinary";

    // char* buffer = nullptr;

    try
    {
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt == queryParameters.end())
        {
            string errorMessage = string("'ingestionJobKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

        auto binaryPathFileIt = requestDetails.find("HTTP_X_FILE");
        if (binaryPathFileIt == requestDetails.end())
        {
            string errorMessage = string("'HTTP_X_FILE' item is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        string binaryPathFile = binaryPathFileIt->second;

        // Content-Range: bytes 0-99999/100000
        bool contentRangePresent = false;
        long long contentRangeStart  = -1;
        long long contentRangeEnd  = -1;
        long long contentRangeSize  = -1;
        auto contentRangeIt = requestDetails.find("HTTP_CONTENT_RANGE");
        if (contentRangeIt != requestDetails.end())
        {
            string contentRange = contentRangeIt->second;
            try
            {
                parseContentRange(contentRange,
                    contentRangeStart,
                    contentRangeEnd,
                    contentRangeSize);

                contentRangePresent = true;                
            }
            catch(exception e)
            {
                string errorMessage = string("Content-Range is not well done. Expected format: 'Content-Range: bytes <start>-<end>/<size>'")
                    + ", contentRange: " + contentRange
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                throw runtime_error(errorMessage);            
            }
        }

        _logger->info(__FILEREF__ + "Content-Range details"
            + ", contentRangePresent: " + to_string(contentRangePresent)
            + ", contentRangeStart: " + to_string(contentRangeStart)
            + ", contentRangeEnd: " + to_string(contentRangeEnd)
            + ", contentRangeSize: " + to_string(contentRangeSize)
        );

        shared_ptr<Workspace> workspace = get<1>(userKeyWorkspaceAndFlags);
        string workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace);
        workspaceIngestionBinaryPathName
                .append("/")
                .append(to_string(ingestionJobKey))
                .append("_source")
                ;
             
        if (!contentRangePresent)
        {
            try
            {
                _logger->info(__FILEREF__ + "Moving file"
                    + ", binaryPathFile: " + binaryPathFile
                    + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                );

                FileIO::moveFile(binaryPathFile, workspaceIngestionBinaryPathName);
            }
            catch(exception e)
            {
                string errorMessage = string("Error to move file")
                    + ", binaryPathFile: " + binaryPathFile
                    + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                throw runtime_error(errorMessage);            
            }

            bool sourceBinaryTransferred = true;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
            );                            
            _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
                ingestionJobKey, sourceBinaryTransferred);
        }
        else
        {
            //  Content-Range is present
            
            if (FileIO::fileExisting (workspaceIngestionBinaryPathName))
            {
                if (contentRangeStart == 0)
                {
                    // content is reset
                    ofstream osDestStream(workspaceIngestionBinaryPathName.c_str(), 
                            ofstream::binary | ofstream::trunc);

                    osDestStream.close();
                }
                
                bool inCaseOfLinkHasItToBeRead  = false;
                unsigned long workspaceIngestionBinarySizeInBytes = FileIO::getFileSizeInBytes (
                    workspaceIngestionBinaryPathName, inCaseOfLinkHasItToBeRead);
                unsigned long binarySizeInBytes = FileIO::getFileSizeInBytes (
                    binaryPathFile, inCaseOfLinkHasItToBeRead);
                
                if (contentRangeStart != workspaceIngestionBinarySizeInBytes)
                {
                    string errorMessage = string("This is NOT the next expected chunk because Content-Range start is different from fileSizeInBytes")
                        + ", contentRangeStart: " + to_string(contentRangeStart)
                        + ", workspaceIngestionBinarySizeInBytes: " + to_string(workspaceIngestionBinarySizeInBytes)
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
                
                /*
                if (contentRangeEnd - contentRangeStart + 1 != binarySizeInBytes)
                {
                    string errorMessage = string("The size specified by Content-Range start and end is not consistent with the size of the binary ingested")
                        + ", contentRangeStart: " + to_string(contentRangeStart)
                        + ", contentRangeEnd: " + to_string(contentRangeEnd)
                        + ", binarySizeInBytes: " + to_string(binarySizeInBytes)
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
                 */
                
                try
                {
                    bool removeSrcFileAfterConcat = true;
                    
                    _logger->info(__FILEREF__ + "Concat file"
                        + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                        + ", binaryPathFile: " + binaryPathFile
                        + ", removeSrcFileAfterConcat: " + to_string(removeSrcFileAfterConcat)
                    );

                    FileIO::concatFile(workspaceIngestionBinaryPathName, binaryPathFile, removeSrcFileAfterConcat);
                }
                catch(exception e)
                {
                    string errorMessage = string("Error to concat file")
                        + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                        + ", binaryPathFile: " + binaryPathFile
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
            }
            else
            {
                // binary file does not exist, so this is the first chunk
                
                if (contentRangeStart != 0)
                {
                    string errorMessage = string("This is the first chunk of the file and Content-Range start has to be 0")
                        + ", contentRangeStart: " + to_string(contentRangeStart)
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
                
                try
                {
                    _logger->info(__FILEREF__ + "Moving file"
                        + ", binaryPathFile: " + binaryPathFile
                        + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                    );

                    FileIO::moveFile(binaryPathFile, workspaceIngestionBinaryPathName);
                }
                catch(exception e)
                {
                    string errorMessage = string("Error to move file")
                        + ", binaryPathFile: " + binaryPathFile
                        + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                    ;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 500, errorMessage);

                    throw runtime_error(errorMessage);            
                }
            }
            
            if (contentRangeEnd + 1 == contentRangeSize)
            {
                bool sourceBinaryTransferred = true;
                _logger->info(__FILEREF__ + "Update IngestionJob"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
                );                            
                _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
                    ingestionJobKey, sourceBinaryTransferred);
            }
        }
        
        string responseBody;
        sendSuccess(request, 201, responseBody);

        /*
        if (requestMethod == "HEAD")
        {
            unsigned long fileSize = 0;
            try
            {
                if (FileIO::fileExisting(workspaceIngestionBinaryPathName))
                {
                    bool inCaseOfLinkHasItToBeRead = false;
                    fileSize = FileIO::getFileSizeInBytes (
                        workspaceIngestionBinaryPathName, inCaseOfLinkHasItToBeRead);
                }
            }
            catch(exception e)
            {
                string errorMessage = string("Error to retrieve the file size")
                    + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                throw runtime_error(errorMessage);            
            }

            sendHeadSuccess(request, 200, fileSize);
        }
        else
        {
            chrono::system_clock::time_point uploadStartTime = chrono::system_clock::now();

            bool resume = false;
            {
                if (xCatraMMSResumeHeader != "")
                {
                    unsigned long fileSize = 0;
                    try
                    {
                        if (FileIO::fileExisting(workspaceIngestionBinaryPathName))
                        {
                            bool inCaseOfLinkHasItToBeRead = false;
                            fileSize = FileIO::getFileSizeInBytes (
                                workspaceIngestionBinaryPathName, inCaseOfLinkHasItToBeRead);
                        }
                    }
                    catch(exception e)
                    {
                        string errorMessage = string("Error to retrieve the file size")
                            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                        ;
                        _logger->error(__FILEREF__ + errorMessage);
    //
    //                    sendError(500, errorMessage);
    //
    //                    throw runtime_error(errorMessage);            
                    }

                    if (stol(xCatraMMSResumeHeader) == fileSize)
                    {
                        _logger->info(__FILEREF__ + "Resume is enabled"
                            + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
                            + ", fileSize: " + to_string(fileSize)
                        );
                        resume = true;
                    }
                    else
                    {
                        _logger->info(__FILEREF__ + "Resume is NOT enabled (X-CatraMMS-Resume header found but different length)"
                            + ", xCatraMMSResumeHeader: " + xCatraMMSResumeHeader
                            + ", fileSize: " + to_string(fileSize)
                        );
                    }
                }
                else
                {
                    _logger->info(__FILEREF__ + "Resume flag is NOT present (No X-CatraMMS-Resume header found)"
                    );
                }
            }
            
            ofstream binaryFileStream(workspaceIngestionBinaryPathName, 
                    resume ? (ofstream::binary | ofstream::app) : (ofstream::binary | ofstream::trunc));
            buffer = new char [_binaryBufferLength];

            unsigned long currentRead;
            unsigned long totalRead = 0;
            {
                // we have the content-length and we will use it to read the binary

                chrono::system_clock::time_point lastTimeProgressUpdate = chrono::system_clock::now();
                double lastPercentageUpdated = -1;
                
                unsigned long bytesToBeRead;
                while (totalRead < contentLength)
                {
                    if (contentLength - totalRead >= _binaryBufferLength)
                        bytesToBeRead = _binaryBufferLength;
                    else
                        bytesToBeRead = contentLength - totalRead;

                    currentRead = FCGX_GetStr(buffer, bytesToBeRead, request.in);
                    // cin.read(buffer, bytesToBeRead);
                    // currentRead = cin.gcount();
                    if (currentRead != bytesToBeRead)
                    {
                        // this should never happen because it will be against the content-length
                        string errorMessage = string("Error reading the binary")
                            + ", contentLength: " + to_string(contentLength)
                            + ", totalRead: " + to_string(totalRead)
                            + ", bytesToBeRead: " + to_string(bytesToBeRead)
                            + ", currentRead: " + to_string(currentRead)
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        sendError(request, 400, errorMessage);

                        throw runtime_error(errorMessage);            
                    }

                    totalRead   += currentRead;

                    binaryFileStream.write(buffer, currentRead); 
                    
                    {
                        chrono::system_clock::time_point now = chrono::system_clock::now();

                        if (now - lastTimeProgressUpdate >= chrono::seconds(_progressUpdatePeriodInSeconds))
                        {
                            double progress = ((double) totalRead / (double) contentLength) * 100;
                            // int uploadingPercentage = floorf(progress * 100) / 100;
                            // this is to have one decimal in the percentage
                            double uploadingPercentage = ((double) ((int) (progress * 10))) / 10;

                            _logger->info(__FILEREF__ + "Upload still running"
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", progress: " + to_string(progress)
                                + ", uploadingPercentage: " + to_string(uploadingPercentage)
                                + ", totalRead: " + to_string(totalRead)
                                + ", contentLength: " + to_string(contentLength)
                            );

                            lastTimeProgressUpdate = now;

                            if (lastPercentageUpdated != uploadingPercentage)
                            {
                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", uploadingPercentage: " + to_string(uploadingPercentage)
                                );                            
                                _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                                    ingestionJobKey, uploadingPercentage);

                                lastPercentageUpdated = uploadingPercentage;
                            }
                        }
                    }
                }
            }

            binaryFileStream.close();

            delete buffer;

            unsigned long elapsedUploadInSeconds = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - uploadStartTime).count();
            _logger->info(__FILEREF__ + "Binary read"
                + ", contentLength: " + to_string(contentLength)
                + ", totalRead: " + to_string(totalRead)
                + ", elapsedUploadInSeconds: " + to_string(elapsedUploadInSeconds)
            );

//            {
//                // Chew up any remaining stdin - this shouldn't be necessary
//                // but is because mod_fastcgi doesn't handle it correctly.
//
//                // ignore() doesn't set the eof bit in some versions of glibc++
//                // so use gcount() instead of eof()...
//                do 
//                    cin.ignore(bufferLength); 
//                while (cin.gcount() == bufferLength);
//            }    

            bool sourceBinaryTransferred = true;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
            );                            
            _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
                ingestionJobKey, sourceBinaryTransferred);

            string responseBody = string("{ ")
                + "\"contentLength\": " + to_string(contentLength) + ", "
                + "\"writtenBytes\": " + to_string(totalRead) + ", "
                + "\"elapsedUploadInSeconds\": " + to_string(elapsedUploadInSeconds) + " "
                + "}";
            sendSuccess(request, 201, responseBody);
        }
        */
    }
    catch (runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }    
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }    
}

void API::addEncodingProfilesSet(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addEncodingProfilesSet";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        auto sContentTypeIt = queryParameters.find("contentType");
        if (sContentTypeIt == queryParameters.end())
        {
            string errorMessage = string("'contentType' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        MMSEngineDBFacade::ContentType contentType = 
                MMSEngineDBFacade::toContentType(sContentTypeIt->second);
        
        Json::Value encodingProfilesSetRoot;
        try
        {
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &encodingProfilesSetRoot, &errors);
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
        }
        catch(runtime_error e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        string responseBody;    
        shared_ptr<MySQLConnection> conn;

        try
        {            
            conn = _mmsEngineDBFacade->beginIngestionJobs();

            Validator validator(_logger, _mmsEngineDBFacade);
            validator.validateEncodingProfilesSetRootMetadata(contentType, encodingProfilesSetRoot);
        
            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfilesSetRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string label = encodingProfilesSetRoot.get(field, "XXX").asString();
                        
            int64_t encodingProfilesSetKey = _mmsEngineDBFacade->addEncodingProfilesSet(conn,
                    workspace->_workspaceKey, contentType, label);
            
            field = "profiles";
            if (_mmsEngineDBFacade->isMetadataPresent(encodingProfilesSetRoot, field))
            {
                Json::Value profilesRoot = encodingProfilesSetRoot[field];

                for (int profileIndex = 0; profileIndex < profilesRoot.size(); profileIndex++)
                {
                    Json::Value profileRoot = profilesRoot[profileIndex];

                    string field = "Label";
                    if (!_mmsEngineDBFacade->isMetadataPresent(profileRoot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    string profileLabel = profileRoot.get(field, "XXX").asString();
            
                    MMSEngineDBFacade::EncodingTechnology encodingTechnology;
                    
                    if (contentType == MMSEngineDBFacade::ContentType::Image)
                        encodingTechnology = MMSEngineDBFacade::EncodingTechnology::Image;
                    else
                        encodingTechnology = MMSEngineDBFacade::EncodingTechnology::MP4;
                       
                    string jsonProfile;
                    {
                        Json::StreamWriterBuilder wbuilder;

                        jsonProfile = Json::writeString(wbuilder, profileRoot);
                    }
                       
                    int64_t encodingProfileKey = _mmsEngineDBFacade->addEncodingProfile(
                        conn, workspace->_workspaceKey, profileLabel,
                        contentType, encodingTechnology, jsonProfile,
                        encodingProfilesSetKey);
                    
                    if (responseBody != "")
                        responseBody += string(", ");
                    responseBody += (
                            string("{ ") 
                            + "\"encodingProfileKey\": " + to_string(encodingProfileKey)
                            + ", \"label\": \"" + profileLabel + "\" "
                            + "}"
                            );
                }
            }
            
            bool commit = true;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);
            
            string beginOfResponseBody = string("{ ")
                + "\"encodingProfilesSet\": { "
                    + "\"encodingProfilesSetKey\": " + to_string(encodingProfilesSetKey)
                    + ", \"label\": \"" + label + "\" "
                    + "}, "
                    + "\"profiles\": [ ";
            responseBody.insert(0, beginOfResponseBody);
            responseBody += " ] }";
        }
        catch(runtime_error e)
        {
            bool commit = false;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            bool commit = false;
            _mmsEngineDBFacade->endIngestionJobs(conn, commit);

            _logger->error(__FILEREF__ + "request body parsing failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::addEncodingProfile(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addEncodingProfile";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        auto sContentTypeIt = queryParameters.find("contentType");
        if (sContentTypeIt == queryParameters.end())
        {
            string errorMessage = string("'contentType' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        MMSEngineDBFacade::ContentType contentType = 
                MMSEngineDBFacade::toContentType(sContentTypeIt->second);
        
        Json::Value encodingProfileRoot;
        try
        {
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &encodingProfileRoot, &errors);
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
        }
        catch(runtime_error e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        string responseBody;    

        try
        {
            Validator validator(_logger, _mmsEngineDBFacade);
            validator.validateEncodingProfileRootMetadata(contentType, encodingProfileRoot);

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string profileLabel = encodingProfileRoot.get(field, "XXX").asString();

            MMSEngineDBFacade::EncodingTechnology encodingTechnology;

            if (contentType == MMSEngineDBFacade::ContentType::Image)
                encodingTechnology = MMSEngineDBFacade::EncodingTechnology::Image;
            else
                encodingTechnology = MMSEngineDBFacade::EncodingTechnology::MP4;

            string jsonEncodingProfile;
            {
                Json::StreamWriterBuilder wbuilder;

                jsonEncodingProfile = Json::writeString(wbuilder, encodingProfileRoot);
            }
            
            int64_t encodingProfileKey = _mmsEngineDBFacade->addEncodingProfile(
                workspace->_workspaceKey, profileLabel,
                contentType, encodingTechnology, jsonEncodingProfile);

            responseBody = (
                    string("{ ") 
                    + "\"encodingProfileKey\": " + to_string(encodingProfileKey)
                    + ", \"label\": \"" + profileLabel + "\" "
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEncodingProfile failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEncodingProfile failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::parseContentRange(string contentRange,
        long long& contentRangeStart,
        long long& contentRangeEnd,
        long long& contentRangeSize)
{
    // Content-Range: bytes 0-99999/100000

    contentRangeStart   = -1;
    contentRangeEnd     = -1;
    contentRangeSize    = -1;

    try
    {
        string prefix ("bytes ");
        if (!(contentRange.size() >= prefix.size() && 0 == contentRange.compare(0, prefix.size(), prefix)))
        {
            string errorMessage = string("Content-Range does not start with 'bytes '")
                    + ", contentRange: " + contentRange
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        int startIndex = prefix.size();
        int endIndex = contentRange.find("-", startIndex);
        if (endIndex == string::npos)
        {
            string errorMessage = string("Content-Range does not have '-'")
                    + ", contentRange: " + contentRange
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        contentRangeStart = stoll(contentRange.substr(startIndex, endIndex - startIndex));

        endIndex++;
        int sizeIndex = contentRange.find("/", endIndex);
        if (sizeIndex == string::npos)
        {
            string errorMessage = string("Content-Range does not have '/'")
                    + ", contentRange: " + contentRange
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

        contentRangeEnd = stoll(contentRange.substr(endIndex, sizeIndex - endIndex));

        sizeIndex++;
        contentRangeSize = stoll(contentRange.substr(sizeIndex));
    }
    catch(exception e)
    {
        string errorMessage = string("Content-Range is not well done. Expected format: 'Content-Range: bytes <start>-<end>/<size>'")
            + ", contentRange: " + contentRange
        ;
        _logger->error(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);            
    }
}

