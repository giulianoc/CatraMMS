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
#include <regex>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "catralibraries/Convert.h"
#include "Validator.h"
#include "EMailSender.h"
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
    try
    {
        _encodingPriorityWorkspaceDefaultValue = MMSEngineDBFacade::toEncodingPriority(encodingPriority);    // it generate an exception in case of wrong string
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "Configuration item is wrong. 'low' encoding priority is set"
            + ", api->encodingPriorityWorkspaceDefaultValue: " + encodingPriority
        );

        _encodingPriorityWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPriority::Low;
    }

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
    _maxStorageInMBWorkspaceDefaultValue = _configuration["api"].get("maxStorageInMBWorkspaceDefaultValue", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->maxStorageInMBWorkspaceDefaultValue: " + to_string(_maxStorageInMBWorkspaceDefaultValue)
    );

    _apiProtocol =  _configuration["api"].get("protocol", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->protocol: " + _apiProtocol
    );
    _apiHostname =  _configuration["api"].get("hostname", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->hostname: " + _apiHostname
    );
    _apiPort = _configuration["api"].get("port", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->port: " + to_string(_apiPort)
    );

    _guiProtocol =  _configuration["mms"].get("guiProtocol", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->guiProtocol: " + _guiProtocol
    );
    _guiHostname =  _configuration["mms"].get("guiHostname", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->guiHostname: " + _guiHostname
    );
    _guiPort = _configuration["mms"].get("guiPort", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->guiPort: " + to_string(_guiPort)
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
        
    _ffmpegEncoderProtocol = _configuration["ffmpeg"].get("encoderProtocol", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderProtocol: " + _ffmpegEncoderProtocol
    );
    _ffmpegEncoderPort = _configuration["ffmpeg"].get("encoderPort", "").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderPort: " + to_string(_ffmpegEncoderPort)
    );
    _ffmpegEncoderUser = _configuration["ffmpeg"].get("encoderUser", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderUser: " + _ffmpegEncoderUser
    );
    _ffmpegEncoderPassword = _configuration["ffmpeg"].get("encoderPassword", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderPassword: " + "..."
    );
    _ffmpegEncoderKillEncodingURI = _configuration["ffmpeg"].get("encoderKillEncodingURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderKillEncodingURI: " + _ffmpegEncoderKillEncodingURI
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

/*
void API::getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool>& userKeyWorkspaceAndFlags,
        unsigned long contentLength
)
{
    _logger->error(__FILEREF__ + "API application is able to manage ONLY NON-Binary requests");
    
    string errorMessage = string("Internal server error");
    _logger->error(__FILEREF__ + errorMessage);

    sendError(500, errorMessage);

    throw runtime_error(errorMessage);
}
*/

void API::manageRequestAndResponse(
        FCGX_Request& request,
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        bool basicAuthenticationPresent,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool,bool,bool,bool>& userKeyWorkspaceAndFlags,
        unsigned long contentLength,
        string requestBody,
        unordered_map<string, string>& requestDetails
)
{
    
    int64_t userKey;
    shared_ptr<Workspace> workspace;
    bool admin;
    bool ingestWorkflow;
    bool createProfiles;
    bool deliveryAuthorization;
    bool shareWorkspace;
    bool editMedia;

    if (basicAuthenticationPresent)
    {
        tie(userKey, workspace, admin, ingestWorkflow, createProfiles, deliveryAuthorization, shareWorkspace, editMedia) 
                = userKeyWorkspaceAndFlags;

        _logger->info(__FILEREF__ + "Received manageRequestAndResponse"
            + ", requestURI: " + requestURI
            + ", requestMethod: " + requestMethod
            + ", requestBody: " + requestBody
            + ", contentLength: " + to_string(contentLength)
            + ", userKey        " + to_string(userKey)
            + ", workspace->_name: " + workspace->_name
            + ", admin: " + to_string(admin)
            + ", ingestWorkflow: " + to_string(ingestWorkflow)
            + ", createProfiles: " + to_string(createProfiles)
            + ", deliveryAuthorization: " + to_string(deliveryAuthorization)
            + ", shareWorkspace: " + to_string(shareWorkspace)
            + ", editMedia: " + to_string(editMedia)
        );        
    }
    else
    {
        _logger->info(__FILEREF__ + "Received manageRequestAndResponse"
            + ", requestURI: " + requestURI
            + ", requestMethod: " + requestMethod
            + ", requestBody: " + requestBody
            + ", contentLength: " + to_string(contentLength)
        );        
    }
        

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
    else if (method == "login")
    {
        login(request, requestBody);
    }
    else if (method == "registerUser")
    {
        registerUser(request, requestBody);
    }
    else if (method == "updateUser")
    {
        updateUser(request, userKey, requestBody);
    }
    else if (method == "updateWorkspace")
    {
        updateWorkspace(request, workspace, userKey, requestBody);
    }
    else if (method == "createWorkspace")
    {
        createWorkspace(request, userKey, queryParameters, requestBody);
    }
    else if (method == "shareWorkspace")
    {
        if (!shareWorkspace)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", shareWorkspace: " + to_string(shareWorkspace)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        shareWorkspace_(request, workspace, queryParameters, requestBody);
    }
    else if (method == "confirmRegistration")
    {
        confirmRegistration(request, queryParameters);
    }
    else if (method == "createDeliveryAuthorization")
    {
        if (!deliveryAuthorization)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", deliveryAuthorization: " + to_string(deliveryAuthorization)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        string clientIPAddress;
        auto remoteAddrIt = requestDetails.find("REMOTE_ADDR");
        if (remoteAddrIt != requestDetails.end())
            clientIPAddress = remoteAddrIt->second;
            
        createDeliveryAuthorization(request, userKey, workspace,
                clientIPAddress, queryParameters);
    }
    else if (method == "ingestion")
    {
        if (!ingestWorkflow)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", ingestWorkflow: " + to_string(ingestWorkflow)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        ingestion(request, workspace, queryParameters, requestBody);
    }
    else if (method == "ingestionRootsStatus")
    {
        ingestionRootsStatus(request, workspace, queryParameters, requestBody);
    }
    else if (method == "ingestionRootMetaDataContent")
    {
        ingestionRootMetaDataContent(request, workspace, queryParameters, requestBody);
    }
    else if (method == "ingestionJobsStatus")
    {
        ingestionJobsStatus(request, workspace, queryParameters, requestBody);
    }
    else if (method == "encodingJobsStatus")
    {
        encodingJobsStatus(request, workspace, queryParameters, requestBody);
    }
    else if (method == "encodingJobPriority")
    {
        encodingJobPriority(request, workspace, queryParameters, requestBody);
    }
    else if (method == "killEncodingJob")
    {
        killEncodingJob(request, workspace, queryParameters, requestBody);
    }
    else if (method == "mediaItemsList")
    {
        mediaItemsList(request, workspace, queryParameters, requestBody, admin);
    }
    else if (method == "uploadedBinary")
    {
        uploadedBinary(request, requestMethod,
            queryParameters, userKeyWorkspaceAndFlags, // contentLength,
                requestDetails);
    }
    else if (method == "addEncodingProfilesSet")
    {
        if (!createProfiles)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createProfiles: " + to_string(createProfiles)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
                
        addEncodingProfilesSet(request, workspace,
            queryParameters, requestBody);
    }
    else if (method == "encodingProfilesSetsList")
    {
        encodingProfilesSetsList(request, workspace, queryParameters);
    }
    else if (method == "addEncodingProfile")
    {
        if (!createProfiles)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createProfiles: " + to_string(createProfiles)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
                
        addEncodingProfile(request, workspace,
            queryParameters, requestBody);
    }
    else if (method == "removeEncodingProfile")
    {
        if (!createProfiles)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createProfiles: " + to_string(createProfiles)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
                
        removeEncodingProfile(request, workspace,
            queryParameters);
    }
    else if (method == "removeEncodingProfilesSet")
    {
        if (!createProfiles)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createProfiles: " + to_string(createProfiles)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
                
        removeEncodingProfilesSet(request, workspace,
            queryParameters);
    }
    else if (method == "encodingProfilesList")
    {
        encodingProfilesList(request, workspace, queryParameters);
    }
    else if (method == "testEmail")
    {
        if (!admin)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", admin: " + to_string(admin)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
                
        string to = "giulianocatrambone@gmail.com";
        string subject = "Email test";

        vector<string> emailBody;
        emailBody.push_back(string("<p>Hi, this is just a test") + "</p>");
        emailBody.push_back("<p>Bye</p>");
        emailBody.push_back("<p>MMS technical support</p>");

        EMailSender emailSender(_logger, _configuration);
        emailSender.sendEmail(to, subject, emailBody);
    }
    else if (method == "addYouTubeConf")
    {
        addYouTubeConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyYouTubeConf")
    {
        modifyYouTubeConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeYouTubeConf")
    {
        removeYouTubeConf(request, workspace, queryParameters);
    }
    else if (method == "youTubeConfList")
    {
        youTubeConfList(request, workspace);
    }
    else if (method == "addFacebookConf")
    {
        addFacebookConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyFacebookConf")
    {
        modifyFacebookConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeFacebookConf")
    {
        removeFacebookConf(request, workspace, queryParameters);
    }
    else if (method == "facebookConfList")
    {
        facebookConfList(request, workspace);
    }
    else if (method == "addLiveURLConf")
    {
        addLiveURLConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyLiveURLConf")
    {
        modifyLiveURLConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeLiveURLConf")
    {
        removeLiveURLConf(request, workspace, queryParameters);
    }
    else if (method == "liveURLConfList")
    {
        liveURLConfList(request, workspace);
    }
    else if (method == "addFTPConf")
    {
        addFTPConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyFTPConf")
    {
        modifyFTPConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeFTPConf")
    {
        removeFTPConf(request, workspace, queryParameters);
    }
    else if (method == "ftpConfList")
    {
        ftpConfList(request, workspace);
    }
    else if (method == "addEMailConf")
    {
        addEMailConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyEMailConf")
    {
        modifyEMailConf(request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeEMailConf")
    {
        removeEMailConf(request, workspace, queryParameters);
    }
    else if (method == "emailConfList")
    {
        emailConfList(request, workspace);
    }
    else
    {
        string errorMessage = string("No API is matched")
            + ", requestURI: " + requestURI
            + ", method: " + method
            + ", requestMethod: " + requestMethod
                ;
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
        int maxStorageInMB;

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

        maxStorageInMB = _maxStorageInMBWorkspaceDefaultValue;
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
                    if (isalnum(c)) 
                        return c; 
                    else 
                        return (unsigned char) '_'; } 
            );

            _logger->info(__FILEREF__ + "Registering User"
                + ", workspaceName: " + workspaceName
                + ", email: " + email
            );
            
            tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode = 
                _mmsEngineDBFacade->registerUserAndAddWorkspace(
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
                    maxStorageInMB,                 // long maxStorageInMB,
                    "",                             // string languageCode,
                    chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
                );

            _logger->info(__FILEREF__ + "Registered User and added Workspace"
                + ", workspaceName: " + workspaceName
                + ", email: " + email
                + ", userKey: " + to_string(get<1>(workspaceKeyUserKeyAndConfirmationCode))
                + ", confirmationCode: " + get<2>(workspaceKeyUserKeyAndConfirmationCode)
            );
            
            string responseBody = string("{ ")
                + "\"workspaceKey\": " + to_string(get<0>(workspaceKeyUserKeyAndConfirmationCode)) + " "
                + ", \"userKey\": " + to_string(get<1>(workspaceKeyUserKeyAndConfirmationCode)) + " "
                + "}";
            sendSuccess(request, 201, responseBody);
            
			string confirmationURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				confirmationURL += (":" + to_string(_guiPort));
			confirmationURL += ("/catramms/login.xhtml?confirmationRequested=true");

            string to = email;
            string subject = "Confirmation code";
            
            vector<string> emailBody;
            emailBody.push_back(string("<p>Hi ") + name + ",</p>");
            emailBody.push_back(string("<p>the registration has been done successfully, user and default Workspace have been created</p>"));
            emailBody.push_back(string("<p>here follows the user key <b>") + to_string(get<1>(workspaceKeyUserKeyAndConfirmationCode)) 
                + "</b> and the confirmation code <b>" + get<2>(workspaceKeyUserKeyAndConfirmationCode) + "</b> to be used to confirm the registration</p>");
            // string confirmURL = _apiProtocol + "://" + _apiHostname + ":" + to_string(_apiPort) + "/catramms/v1/user/" 
            //         + to_string(get<1>(workspaceKeyUserKeyAndConfirmationCode)) + "/" + get<2>(workspaceKeyUserKeyAndConfirmationCode);
            // emailBody.push_back(string("<p>Click <a href=\"") + confirmURL + "\">here</a> to confirm the registration</p>");
            emailBody.push_back(
					string("<p>Please click <a href=\"")
					+ confirmationURL
					+ "\">here</a> to confirm the registration</p>");
            emailBody.push_back("<p>Have a nice day, best regards</p>");
            emailBody.push_back("<p>MMS technical support</p>");

            EMailSender emailSender(_logger, _configuration);
            emailSender.sendEmail(to, subject, emailBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
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

void API::createWorkspace(
        FCGX_Request& request,
        int64_t userKey,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "createWorkspace";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        string workspaceName;
        MMSEngineDBFacade::EncodingPriority encodingPriority;
        MMSEngineDBFacade::EncodingPeriod encodingPeriod;
        int maxIngestionsNumber;
        int maxStorageInMB;

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
                "WorkspaceName"
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
        }

        encodingPriority = _encodingPriorityWorkspaceDefaultValue;

        encodingPeriod = _encodingPeriodWorkspaceDefaultValue;
        maxIngestionsNumber = _maxIngestionsNumberWorkspaceDefaultValue;

        maxStorageInMB = _maxStorageInMBWorkspaceDefaultValue;

        try
        {
            string workspaceDirectoryName;

            workspaceDirectoryName.resize(workspaceName.size());

            transform(
                workspaceName.begin(), 
                workspaceName.end(), 
                workspaceDirectoryName.begin(), 
                [](unsigned char c){
                    if (isalnum(c)) 
                        return c; 
                    else 
                        return (unsigned char) '_'; } 
            );

            _logger->info(__FILEREF__ + "Creating Workspace"
                + ", workspaceName: " + workspaceName
            );
            
            pair<int64_t,string> workspaceKeyAndConfirmationCode =
                    _mmsEngineDBFacade->createWorkspace(
                        userKey,
                        workspaceName,
                        workspaceDirectoryName,
                        MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,  // MMSEngineDBFacade::WorkspaceType workspaceType
                        "",     // string deliveryURL,
                        encodingPriority,               //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
                        encodingPeriod,                 //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
                        maxIngestionsNumber,            // long maxIngestionsNumber,
                        maxStorageInMB,                 // long maxStorageInMB,
                        "",                             // string languageCode,
                        chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
                );

            _logger->info(__FILEREF__ + "Created a new Workspace for the User"
                + ", workspaceName: " + workspaceName
                + ", userKey: " + to_string(userKey)
                + ", confirmationCode: " + get<1>(workspaceKeyAndConfirmationCode)
            );
            
            pair<string, string> emailAddressAndName = _mmsEngineDBFacade->getUserDetails (userKey);

            string responseBody = string("{ ")
                + "\"workspaceKey\": " + to_string(get<0>(workspaceKeyAndConfirmationCode)) + " "
                + "}";
            sendSuccess(request, 201, responseBody);
            
            string to = emailAddressAndName.first;
            string subject = "Confirmation code";
            
            vector<string> emailBody;
            emailBody.push_back(string("<p>Hi ") + emailAddressAndName.second + ",</p>");
            emailBody.push_back(string("<p>the Workspace has been created successfully</p>"));
            emailBody.push_back(string("<p>here follows the confirmation code ") + get<1>(workspaceKeyAndConfirmationCode) + " to be used to confirm the registration</p>");
            string confirmURL = _apiProtocol + "://" + _apiHostname + ":" + to_string(_apiPort) + "/catramms/v1/user/" 
                    + to_string(userKey) + "/" + get<1>(workspaceKeyAndConfirmationCode);
            emailBody.push_back(string("<p>Click <a href=\"") + confirmURL + "\">here</a> to confirm the registration</p>");
            emailBody.push_back("<p>Have a nice day, best regards</p>");
            emailBody.push_back("<p>MMS technical support</p>");

            EMailSender emailSender(_logger, _configuration);
            emailSender.sendEmail(to, subject, emailBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
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

void API::shareWorkspace_(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "shareWorkspace";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        bool userAlreadyPresent;
        auto userAlreadyPresentIt = queryParameters.find("userAlreadyPresent");
        if (userAlreadyPresentIt == queryParameters.end())
        {
            string errorMessage = string("The 'userAlreadyPresent' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        userAlreadyPresent = (userAlreadyPresentIt->second == "true" ? true : false);

        bool ingestWorkflow;
        auto ingestWorkflowIt = queryParameters.find("ingestWorkflow");
        if (ingestWorkflowIt == queryParameters.end())
        {
            string errorMessage = string("The 'ingestWorkflow' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        ingestWorkflow = (ingestWorkflowIt->second == "true" ? true : false);

        bool createProfiles;
        auto createProfilesIt = queryParameters.find("createProfiles");
        if (createProfilesIt == queryParameters.end())
        {
            string errorMessage = string("The 'createProfiles' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        createProfiles = (createProfilesIt->second == "true" ? true : false);

        bool deliveryAuthorization;
        auto deliveryAuthorizationIt = queryParameters.find("deliveryAuthorization");
        if (deliveryAuthorizationIt == queryParameters.end())
        {
            string errorMessage = string("The 'deliveryAuthorization' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        deliveryAuthorization = (deliveryAuthorizationIt->second == "true" ? true : false);

        bool shareWorkspace;
        auto shareWorkspaceIt = queryParameters.find("shareWorkspace");
        if (shareWorkspaceIt == queryParameters.end())
        {
            string errorMessage = string("The 'shareWorkspace' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        shareWorkspace = (shareWorkspaceIt->second == "true" ? true : false);

        bool editMedia;
        auto editMediaIt = queryParameters.find("editMedia");
        if (editMediaIt == queryParameters.end())
        {
            string errorMessage = string("The 'editMedia' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        editMedia = (editMediaIt->second == "true" ? true : false);

        string name;
        string email;
        string password;
        string country;

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

        if (userAlreadyPresent)
        {
            vector<string> mandatoryFields = {
                "EMail"
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

            email = metadataRoot.get("EMail", "XXX").asString();
        }
        else
        {
            vector<string> mandatoryFields = {
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

            email = metadataRoot.get("EMail", "XXX").asString();
            password = metadataRoot.get("Password", "XXX").asString();
            name = metadataRoot.get("Name", "XXX").asString();
            country = metadataRoot.get("Country", "XXX").asString();
        }

        try
        {
            _logger->info(__FILEREF__ + "Registering User"
                + ", email: " + email
            );
            
            tuple<int64_t,string> userKeyAndConfirmationCode = 
                _mmsEngineDBFacade->registerUserAndShareWorkspace(
                    userAlreadyPresent,
                    name, 
                    email, 
                    password,
                    country, 
                    ingestWorkflow, createProfiles, deliveryAuthorization, shareWorkspace, editMedia,
                    workspace->_workspaceKey,
                    chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
                );

            _logger->info(__FILEREF__ + "Registered User and shared Workspace"
                + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                + ", email: " + email
                + ", userKey: " + to_string(get<0>(userKeyAndConfirmationCode))
                + ", confirmationCode: " + get<1>(userKeyAndConfirmationCode)
            );
            
            string responseBody = string("{ ")
                + "\"userKey\": " + to_string(get<0>(userKeyAndConfirmationCode)) + " "
                + "}";
            sendSuccess(request, 201, responseBody);
            
			string confirmationURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				confirmationURL += (":" + to_string(_guiPort));
			confirmationURL += ("/catramms/login.xhtml?confirmationRequested=true");

            string to = email;
            string subject = "Confirmation code";
            
            vector<string> emailBody;
            emailBody.push_back(string("<p>Hi ") + name + ",</p>");
            emailBody.push_back(string("<p>the workspace has been shared successfully</p>"));
            emailBody.push_back(string("<p>Here follows the user key <b>") + to_string(get<0>(userKeyAndConfirmationCode)) 
                + "</b> and the confirmation code <b>" + get<1>(userKeyAndConfirmationCode) + "</b> to be used to confirm the sharing of the Workspace</p>");
            emailBody.push_back(
					string("<p>Please click <a href=\"")
					+ confirmationURL
					+ "\">here</a> to confirm the registration</p>");
            emailBody.push_back("<p>Have a nice day, best regards</p>");
            emailBody.push_back("<p>MMS technical support</p>");

            EMailSender emailSender(_logger, _configuration);
            emailSender.sendEmail(to, subject, emailBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
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

void API::confirmRegistration(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters)
{
    string api = "confirmRegistration";

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
            tuple<string,string,string> apiKeyNameAndEmailAddress
                = _mmsEngineDBFacade->confirmRegistration(confirmationCodeIt->second);

            string apiKey;
            string name;
            string emailAddress;
            
            tie(apiKey, name, emailAddress) = apiKeyNameAndEmailAddress;
            
            string responseBody = string("{ ")
                + "\"status\": \"Success\", "
                + "\"apiKey\": \"" + apiKey + "\" "
                + "}";
            sendSuccess(request, 201, responseBody);
            
            string to = emailAddress;
            string subject = "Welcome";
            
            vector<string> emailBody;
            emailBody.push_back(string("<p>Hi ") + name + ",</p>");
            emailBody.push_back(string("<p>Your registration is now completed and you can enjoy working with MMS</p>"));
            emailBody.push_back("<p>Best regards</p>");
            emailBody.push_back("<p>MMS technical support</p>");

            EMailSender emailSender(_logger, _configuration);
            emailSender.sendEmail(to, subject, emailBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
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

void API::login(
        FCGX_Request& request,
        string requestBody)
{
    string api = "login";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        string email;
        string password;

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
                "EMail",
                "Password"
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

            email = metadataRoot.get("EMail", "XXX").asString();
            password = metadataRoot.get("Password", "XXX").asString();
        }

        try
        {
            _logger->info(__FILEREF__ + "Login User"
                + ", email: " + email
            );
                        
            Json::Value loginDetailsRoot = _mmsEngineDBFacade->login(
                    email, 
                    password
                );

            string field = "userKey";
            int64_t userKey = loginDetailsRoot.get(field, 0).asInt64();
            
            _logger->info(__FILEREF__ + "Login User"
                + ", userKey: " + to_string(userKey)
                + ", email: " + email
            );
            
            Json::Value workspaceDetailsRoot =
                    _mmsEngineDBFacade->getWorkspaceDetails(userKey);
            
            field = "workspaces";
            loginDetailsRoot[field] = workspaceDetailsRoot;
            
            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, loginDetailsRoot);
            
            sendSuccess(request, 200, responseBody);            
        }
        catch(LoginFailed e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = e.what();
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 401, errorMessage);   // unauthorized

            throw runtime_error(errorMessage);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
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

void API::updateUser(
        FCGX_Request& request,
        int64_t userKey,
        string requestBody)
{
    string api = "updateUser";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        string name;
        string email;
        string password;
        string country;

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

            name = metadataRoot.get("Name", "XXX").asString();
            email = metadataRoot.get("EMail", "XXX").asString();
            password = metadataRoot.get("Password", "XXX").asString();
            country = metadataRoot.get("Country", "XXX").asString();
        }

        try
        {
            _logger->info(__FILEREF__ + "Updating User"
                + ", userKey: " + to_string(userKey)
                + ", name: " + name
                + ", email: " + email
            );
            
            Json::Value loginDetailsRoot = _mmsEngineDBFacade->updateUser(
                    userKey,
                    name, 
                    email, 
                    password,
                    country);

            _logger->info(__FILEREF__ + "User updated"
                + ", userKey: " + to_string(userKey)
                + ", name: " + name
                + ", email: " + email
            );
            
            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, loginDetailsRoot);
            
            sendSuccess(request, 200, responseBody);            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
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

void API::updateWorkspace(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
        string requestBody)
{
    string api = "updateWorkspace";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        bool newEnabled;
        string newMaxEncodingPriority;
        string newEncodingPeriod;
        int64_t newMaxIngestionsNumber;
        int64_t newMaxStorageInMB;
        string newLanguageCode;
        bool newIngestWorkflow;
        bool newCreateProfiles;
        bool newDeliveryAuthorization;
        bool newShareWorkspace;
        bool newEditMedia;

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
                "Enabled",
                "MaxEncodingPriority",
                "EncodingPeriod",
                "MaxIngestionsNumber",
                "MaxStorageInMB",
                "LanguageCode",
                "IngestWorkflow",
                "CreateProfiles",
                "DeliveryAuthorization",
                "ShareWorkspace",
                "EditMedia"
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

            newEnabled = metadataRoot.get("Enabled", "XXX").asBool();
            newMaxEncodingPriority = metadataRoot.get("MaxEncodingPriority", "XXX").asString();
            newEncodingPeriod = metadataRoot.get("EncodingPeriod", "XXX").asString();
            newMaxIngestionsNumber = metadataRoot.get("MaxIngestionsNumber", "XXX").asInt64();
            newMaxStorageInMB = metadataRoot.get("MaxStorageInMB", "XXX").asInt64();
            newLanguageCode = metadataRoot.get("LanguageCode", "XXX").asString();
            newIngestWorkflow = metadataRoot.get("IngestWorkflow", "XXX").asBool();
            newCreateProfiles = metadataRoot.get("CreateProfiles", "XXX").asBool();
            newDeliveryAuthorization = metadataRoot.get("DeliveryAuthorization", "XXX").asBool();
            newShareWorkspace = metadataRoot.get("ShareWorkspace", "XXX").asBool();
            newEditMedia = metadataRoot.get("EditMedia", "XXX").asBool();
        }

        try
        {
            _logger->info(__FILEREF__ + "Updating WorkspaceDetails"
                + ", userKey: " + to_string(userKey)
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
            );
            
            Json::Value workspaceDetailRoot = _mmsEngineDBFacade->updateWorkspaceDetails (
                    userKey,
                    workspace->_workspaceKey,
                    newEnabled, newMaxEncodingPriority,
                    newEncodingPeriod, newMaxIngestionsNumber,
                    newMaxStorageInMB, newLanguageCode,
                    newIngestWorkflow, newCreateProfiles,
                    newDeliveryAuthorization, newShareWorkspace,
                    newEditMedia);

            _logger->info(__FILEREF__ + "WorkspaceDetails updated"
                + ", userKey: " + to_string(userKey)
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
            );
            
            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, workspaceDetailRoot);
            
            sendSuccess(request, 200, responseBody);            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
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
        if (ttlInSecondsIt != queryParameters.end() && ttlInSecondsIt->second != "")
        {
            ttlInSeconds = stol(ttlInSecondsIt->second);
        }

        int maxRetries = _defaultMaxRetries;
        auto maxRetriesIt = queryParameters.find("maxRetries");
        if (maxRetriesIt != queryParameters.end() && maxRetriesIt->second != "")
        {
            maxRetries = stol(maxRetriesIt->second);
        }
        
        bool redirect = _defaultRedirect;
        auto redirectIt = queryParameters.find("redirect");
        if (redirectIt != queryParameters.end())
        {
            if (redirectIt->second == "true")
                redirect = true;
            else
                redirect = false;
        }
        
        bool save = false;
        auto saveIt = queryParameters.find("save");
        if (saveIt != queryParameters.end())
        {
            if (saveIt->second == "true")
                save = true;
            else
                save = false;
        }

        try
        {
            tuple<int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails =
                _mmsEngineDBFacade->getStorageDetails(physicalPathKey);

            int mmsPartitionNumber;
            shared_ptr<Workspace> contentWorkspace;
            string relativePath;
            string fileName;
            string deliveryFileName;
            string title;
            int64_t sizeInBytes;
            tie(mmsPartitionNumber, contentWorkspace, relativePath, fileName, 
                    deliveryFileName, title, sizeInBytes) = storageDetails;

            if (save)
            {
                if (deliveryFileName == "")
                    deliveryFileName = title;

                if (deliveryFileName != "")
                {
                    // use the extension of fileName
                    size_t extensionIndex = fileName.find_last_of(".");
                    if (extensionIndex != string::npos)
                        deliveryFileName.append(fileName.substr(extensionIndex));
                }
            }
            
            if (contentWorkspace->_workspaceKey != requestWorkspace->_workspaceKey)
            {
                string errorMessage = string ("Workspace of the content and Workspace of the requester is different")
                        + ", contentWorkspace->_workspaceKey: " + to_string(contentWorkspace->_workspaceKey)
                        + ", requestWorkspace->_workspaceKey: " + to_string(requestWorkspace->_workspaceKey)
                        ;
                _logger->error(__FILEREF__ + errorMessage);
                
                throw runtime_error(errorMessage);
            }
            
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
            ;
            if (save && deliveryFileName != "")
                deliveryURL.append("&deliveryFileName=").append(deliveryFileName);
            
            if (redirect)
            {
                sendRedirect(request, deliveryURL);
            }
            else
            {
                string responseBody = string("{ ")
                    + "\"deliveryURL\": \"" + deliveryURL + "\""
                    + ", \"deliveryFileName\": \"" + deliveryFileName + "\""
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

            string errorMessage = string("Internal server error: ") + e.what();
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

void API::ingestionRootsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestionRootsStatus";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t ingestionRootKey = -1;
        auto ingestionRootKeyIt = queryParameters.find("ingestionRootKey");
        if (ingestionRootKeyIt != queryParameters.end() && ingestionRootKeyIt->second != "")
        {
            ingestionRootKey = stoll(ingestionRootKeyIt->second);
        }

        int start = 0;
        auto startIt = queryParameters.find("start");
        if (startIt != queryParameters.end() && startIt->second != "")
        {
            start = stoll(startIt->second);
        }

        int rows = 10;
        auto rowsIt = queryParameters.find("rows");
        if (rowsIt != queryParameters.end() && rowsIt->second != "")
        {
            rows = stoll(rowsIt->second);
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

        string status = "all";
        auto statusIt = queryParameters.find("status");
        if (statusIt != queryParameters.end() && statusIt->second != "")
        {
            status = statusIt->second;
        }
        
        bool asc = true;
        auto ascIt = queryParameters.find("asc");
        if (ascIt != queryParameters.end() && ascIt->second != "")
        {
            if (ascIt->second == "true")
                asc = true;
            else
                asc = false;
        }

        {
            Json::Value ingestionStatusRoot = _mmsEngineDBFacade->getIngestionRootsStatus(
                    workspace, ingestionRootKey,
                    start, rows,
                    startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
                    status, asc
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

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::ingestionRootMetaDataContent(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestionRootMetaDataContent";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t ingestionRootKey = -1;
        auto ingestionRootKeyIt = queryParameters.find("ingestionRootKey");
        if (ingestionRootKeyIt == queryParameters.end() || ingestionRootKeyIt->second == "")
		{
			string errorMessage = string("The 'ingestionRootKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		ingestionRootKey = stoll(ingestionRootKeyIt->second);

        {
            string ingestionRootMetaDataContent = _mmsEngineDBFacade->getIngestionRootMetaDataContent(
                    workspace, ingestionRootKey);

            sendSuccess(request, 200, ingestionRootMetaDataContent);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::ingestionJobsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestionJobsStatus";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t ingestionJobKey = -1;
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt != queryParameters.end() && ingestionJobKeyIt->second != "")
        {
            ingestionJobKey = stoll(ingestionJobKeyIt->second);
        }

        int start = 0;
        auto startIt = queryParameters.find("start");
        if (startIt != queryParameters.end() && startIt->second != "")
        {
            start = stoll(startIt->second);
        }

        int rows = 10;
        auto rowsIt = queryParameters.find("rows");
        if (rowsIt != queryParameters.end() && rowsIt->second != "")
        {
            rows = stoll(rowsIt->second);
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

        bool asc = true;
        auto ascIt = queryParameters.find("asc");
        if (ascIt != queryParameters.end() && ascIt->second != "")
        {
            if (ascIt->second == "true")
                asc = true;
            else
                asc = false;
        }

        string status = "all";
        auto statusIt = queryParameters.find("status");
        if (statusIt != queryParameters.end() && statusIt->second != "")
        {
            status = statusIt->second;
        }

        {
            Json::Value ingestionStatusRoot = _mmsEngineDBFacade->getIngestionJobsStatus(
                    workspace, ingestionJobKey,
                    start, rows,
                    startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
                    asc, status
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

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::encodingJobsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "encodingJobsStatus";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t encodingJobKey = -1;
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt != queryParameters.end() && encodingJobKeyIt->second != "")
        {
            encodingJobKey = stoll(encodingJobKeyIt->second);
        }

        int start = 0;
        auto startIt = queryParameters.find("start");
        if (startIt != queryParameters.end() && startIt->second != "")
        {
            start = stoll(startIt->second);
        }

        int rows = 10;
        auto rowsIt = queryParameters.find("rows");
        if (rowsIt != queryParameters.end() && rowsIt->second != "")
        {
            rows = stoll(rowsIt->second);
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

        bool asc = true;
        auto ascIt = queryParameters.find("asc");
        if (ascIt != queryParameters.end() && ascIt->second != "")
        {
            if (ascIt->second == "true")
                asc = true;
            else
                asc = false;
        }

        string status = "all";
        auto statusIt = queryParameters.find("status");
        if (statusIt != queryParameters.end() && statusIt->second != "")
        {
            status = statusIt->second;
        }

        {
            Json::Value encodingStatusRoot = _mmsEngineDBFacade->getEncodingJobsStatus(
                    workspace, encodingJobKey,
                    start, rows,
                    startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
                    asc, status
                    );

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, encodingStatusRoot);
            
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

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::encodingJobPriority(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "encodingJobPriority";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t encodingJobKey = -1;
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt != queryParameters.end() && encodingJobKeyIt->second != "")
        {
            encodingJobKey = stoll(encodingJobKeyIt->second);
        }

        MMSEngineDBFacade::EncodingPriority newEncodingJobPriority;
        bool newEncodingJobPriorityPresent = false;
        auto newEncodingJobPriorityCodeIt = queryParameters.find("newEncodingJobPriorityCode");
        if (newEncodingJobPriorityCodeIt != queryParameters.end() && newEncodingJobPriorityCodeIt->second != "")
        {
            newEncodingJobPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(stoll(newEncodingJobPriorityCodeIt->second));
            newEncodingJobPriorityPresent = true;
        }

        bool tryEncodingAgain = false;
        auto tryEncodingAgainIt = queryParameters.find("tryEncodingAgain");
        if (tryEncodingAgainIt != queryParameters.end())
        {
            if (tryEncodingAgainIt->second == "false")
                tryEncodingAgain = false;
            else
                tryEncodingAgain = true;
        }

        {
            if (newEncodingJobPriorityPresent)
            {
                _mmsEngineDBFacade->updateEncodingJobPriority(
                    workspace, encodingJobKey, newEncodingJobPriority);
            }
            
            if (tryEncodingAgain)
            {
                _mmsEngineDBFacade->updateEncodingJobTryAgain(
                    workspace, encodingJobKey);
            }
            
            if (!newEncodingJobPriorityPresent && !tryEncodingAgain)
            {
                _logger->warn(__FILEREF__ + "Useless API call, no encoding update was done"
                    + ", newEncodingJobPriorityPresent: " + to_string(newEncodingJobPriorityPresent)
                    + ", tryEncodingAgain: " + to_string(tryEncodingAgain)
                );
            }

            string responseBody;
            
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

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::killEncodingJob(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "encodingJobKill";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t encodingJobKey = -1;
        auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
        if (encodingJobKeyIt != queryParameters.end() && encodingJobKeyIt->second != "")
        {
            encodingJobKey = stoll(encodingJobKeyIt->second);
        }

        {
			string transcoder = _mmsEngineDBFacade->getEncodingJobDetails(encodingJobKey);

			killEncodingJob(transcoder, encodingJobKey);

            string responseBody;
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

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::mediaItemsList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody,
		bool admin)
{
    string api = "mediaItemsList";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t mediaItemKey = -1;
        auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
        if (mediaItemKeyIt != queryParameters.end() && mediaItemKeyIt->second != "")
        {
            mediaItemKey = stoll(mediaItemKeyIt->second);
            if (mediaItemKey == 0)
                mediaItemKey = -1;
        }

        int64_t physicalPathKey = -1;
        auto physicalPathKeyIt = queryParameters.find("physicalPathKey");
        if (physicalPathKeyIt != queryParameters.end() && physicalPathKeyIt->second != "")
        {
            physicalPathKey = stoll(physicalPathKeyIt->second);
            if (physicalPathKey == 0)
                physicalPathKey = -1;
        }

        int start = 0;
        auto startIt = queryParameters.find("start");
        if (startIt != queryParameters.end() && startIt->second != "")
        {
            start = stoll(startIt->second);
        }

        int rows = 10;
        auto rowsIt = queryParameters.find("rows");
        if (rowsIt != queryParameters.end() && rowsIt->second != "")
        {
            rows = stoll(rowsIt->second);
        }
        
        bool contentTypePresent = false;
        MMSEngineDBFacade::ContentType contentType;
        auto contentTypeIt = queryParameters.find("contentType");
        if (contentTypeIt != queryParameters.end() && contentTypeIt->second != "")
        {
            contentType = MMSEngineDBFacade::toContentType(contentTypeIt->second);
            
            contentTypePresent = true;
        }
        
		/*
		 * liveRecordingChunk:
		 * -1: no condition in select
		 *  0: look for NO liveRecordingChunk
		 *  1: look for liveRecordingChunk
		 */
        int liveRecordingChunk = -1;
        auto liveRecordingChunkIt = queryParameters.find("liveRecordingChunk");
        if (liveRecordingChunkIt != queryParameters.end() && liveRecordingChunkIt->second != "")
        {
			if (liveRecordingChunkIt->second == "true")
				liveRecordingChunk = 1;
			else if (liveRecordingChunkIt->second == "false")
				liveRecordingChunk = 0;
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

        string title;
        auto titleIt = queryParameters.find("title");
        if (titleIt != queryParameters.end() && titleIt->second != "")
        {
            title = titleIt->second;
        }

		vector<string> tags;
        auto tagsIt = queryParameters.find("tags");
        if (tagsIt != queryParameters.end() && tagsIt->second != "")
		{
			char delim = ',';

			stringstream ssTags (tagsIt->second);
			string item;

			while (getline (ssTags, item, delim))
			{
				tags.push_back (item);
			}
        }

        string jsonCondition;
        auto jsonConditionIt = queryParameters.find("jsonCondition");
        if (jsonConditionIt != queryParameters.end() && jsonConditionIt->second != "")
        {
            jsonCondition = jsonConditionIt->second;

			CURL *curl = curl_easy_init();
			if(curl)
			{
				int outLength;
				char *decoded = curl_easy_unescape(curl,
						jsonCondition.c_str(), jsonCondition.length(), &outLength);
				if(decoded)
				{
					string sDecoded = decoded;
					curl_free(decoded);

					// still there is the '+' char
					string plus = "\\+";
					string plusDecoded = " ";
					jsonCondition = regex_replace(sDecoded, regex(plus), plusDecoded);
				}
			}
        }

        string ingestionDateOrder;
        auto ingestionDateOrderIt = queryParameters.find("ingestionDateOrder");
        if (ingestionDateOrderIt != queryParameters.end() && ingestionDateOrderIt->second != "")
        {
            if (ingestionDateOrderIt->second == "asc" || ingestionDateOrderIt->second == "desc")
                ingestionDateOrder = ingestionDateOrderIt->second;
            else
                _logger->warn(__FILEREF__ + "mediaItemsList: 'ingestionDateOrder' parameter is unknown"
                    + ", ingestionDateOrder: " + ingestionDateOrderIt->second);
        }

        string jsonOrderBy;
        auto jsonOrderByIt = queryParameters.find("jsonOrderBy");
        if (jsonOrderByIt != queryParameters.end() && jsonOrderByIt->second != "")
        {
            jsonOrderBy = jsonOrderByIt->second;

			CURL *curl = curl_easy_init();
			if(curl)
			{
				int outLength;
				char *decoded = curl_easy_unescape(curl,
						jsonOrderBy.c_str(), jsonOrderBy.length(), &outLength);
				if(decoded)
				{
					string sDecoded = decoded;
					curl_free(decoded);

					// still there is the '+' char
					string plus = "\\+";
					string plusDecoded = " ";
					jsonOrderBy = regex_replace(sDecoded, regex(plus), plusDecoded);
				}
			}
        }

        {
            Json::Value ingestionStatusRoot = _mmsEngineDBFacade->getMediaItemsList(
                    workspace->_workspaceKey, mediaItemKey, physicalPathKey,
                    start, rows,
                    contentTypePresent, contentType,
                    startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
                    title, liveRecordingChunk, jsonCondition, tags, ingestionDateOrder, jsonOrderBy, admin);

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

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::encodingProfilesSetsList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "encodingProfilesSetsList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        int64_t encodingProfilesSetKey = -1;
        auto encodingProfilesSetKeyIt = queryParameters.find("encodingProfilesSetKey");
        if (encodingProfilesSetKeyIt != queryParameters.end() && encodingProfilesSetKeyIt->second != "")
        {
            encodingProfilesSetKey = stoll(encodingProfilesSetKeyIt->second);
        }

        bool contentTypePresent = false;
        MMSEngineDBFacade::ContentType contentType;
        auto contentTypeIt = queryParameters.find("contentType");
        if (contentTypeIt != queryParameters.end() && contentTypeIt->second != "")
        {
            contentType = MMSEngineDBFacade::toContentType(contentTypeIt->second);
            
            contentTypePresent = true;
        }
        
        {
            
            Json::Value encodingProfilesSetListRoot = _mmsEngineDBFacade->getEncodingProfilesSetList(
                    workspace->_workspaceKey, encodingProfilesSetKey,
                    contentTypePresent, contentType);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, encodingProfilesSetListRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::encodingProfilesList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "encodingProfilesList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        int64_t encodingProfileKey = -1;
        auto encodingProfileKeyIt = queryParameters.find("encodingProfileKey");
        if (encodingProfileKeyIt != queryParameters.end() && encodingProfileKeyIt->second != "")
        {
            encodingProfileKey = stoll(encodingProfileKeyIt->second);
        }

        bool contentTypePresent = false;
        MMSEngineDBFacade::ContentType contentType;
        auto contentTypeIt = queryParameters.find("contentType");
        if (contentTypeIt != queryParameters.end() && contentTypeIt->second != "")
        {
            contentType = MMSEngineDBFacade::toContentType(contentTypeIt->second);
            
            contentTypePresent = true;
        }
        
        {
            Json::Value encodingProfileListRoot = _mmsEngineDBFacade->getEncodingProfileList(
                    workspace->_workspaceKey, encodingProfileKey,
                    contentTypePresent, contentType);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, encodingProfileListRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::addYouTubeConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addYouTubeConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string refreshToken;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "RefreshToken";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            refreshToken = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey = _mmsEngineDBFacade->addYouTubeConf(
                workspace->_workspaceKey, label, refreshToken);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addYouTubeConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addYouTubeConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::modifyYouTubeConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyYouTubeConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string refreshToken;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "RefreshToken";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            refreshToken = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifyYouTubeConf(
                confKey, workspace->_workspaceKey, label, refreshToken);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyYouTubeConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyYouTubeConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::removeYouTubeConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeYouTubeConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeYouTubeConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeYouTubeConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeYouTubeConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::youTubeConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "youTubeConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
            
            Json::Value youTubeConfListRoot = _mmsEngineDBFacade->getYouTubeConfList(
                    workspace->_workspaceKey);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, youTubeConfListRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::addFacebookConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addFacebookConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string pageToken;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "PageToken";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            pageToken = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey = _mmsEngineDBFacade->addFacebookConf(
                workspace->_workspaceKey, label, pageToken);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFacebookConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFacebookConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::modifyFacebookConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyFacebookConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string pageToken;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "PageToken";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            pageToken = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifyFacebookConf(
                confKey, workspace->_workspaceKey, label, pageToken);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFacebookConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFacebookConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::removeFacebookConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeFacebookConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeFacebookConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFacebookConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFacebookConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::facebookConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "facebookConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
            
            Json::Value facebookConfListRoot = _mmsEngineDBFacade->getFacebookConfList(
                    workspace->_workspaceKey);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, facebookConfListRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::addLiveURLConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addLiveURLConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string liveURL;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "LiveURL";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            liveURL = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey = _mmsEngineDBFacade->addLiveURLConf(
                workspace->_workspaceKey, label, liveURL);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addLiveURLConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addLiveURLConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::modifyLiveURLConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyLiveURLConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string liveURL;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "LiveURL";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            liveURL = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifyLiveURLConf(
                confKey, workspace->_workspaceKey, label, liveURL);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyLiveURLConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyLiveURLConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::removeLiveURLConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeLiveURLConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeLiveURLConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeLiveURLConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeLiveURLConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::liveURLConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "liveURLConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
            
            Json::Value liveURLConfListRoot = _mmsEngineDBFacade->getLiveURLConfList(
                    workspace->_workspaceKey);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, liveURLConfListRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::addFTPConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addFTPConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string server;
        int port;
        string userName;
        string password;
        string remoteDirectory;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "Server";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            server = requestBodyRoot.get(field, "XXX").asString();            

            field = "Port";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            port = requestBodyRoot.get(field, 0).asInt();            

            field = "UserName";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            userName = requestBodyRoot.get(field, "XXX").asString();            

            field = "Password";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            password = requestBodyRoot.get(field, "XXX").asString();            

            field = "RemoteDirectory";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            remoteDirectory = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey = _mmsEngineDBFacade->addFTPConf(
                workspace->_workspaceKey, label, server, port,
				userName, password, remoteDirectory);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFTPConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFTPConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::modifyFTPConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyFTPConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string server;
        int port;
        string userName;
        string password;
        string remoteDirectory;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "Server";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            server = requestBodyRoot.get(field, "XXX").asString();            

            field = "Port";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            port = requestBodyRoot.get(field, 0).asInt();            

            field = "UserName";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            userName = requestBodyRoot.get(field, "XXX").asString();            

            field = "Password";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            password = requestBodyRoot.get(field, "XXX").asString();            

            field = "RemoteDirectory";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            remoteDirectory = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifyFTPConf(
                confKey, workspace->_workspaceKey, label, 
				server, port, userName, password, remoteDirectory);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFTPConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFTPConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::removeFTPConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeFTPConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeFTPConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFTPConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFTPConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::ftpConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "ftpConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
            
            Json::Value ftpConfListRoot = _mmsEngineDBFacade->getFTPConfList(
                    workspace->_workspaceKey);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, ftpConfListRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::addEMailConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addEMailConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string address;
        string subject;
        string message;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "Address";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            address = requestBodyRoot.get(field, "XXX").asString();            

            field = "Subject";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            subject = requestBodyRoot.get(field, "XXX").asString();            

            field = "Message";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            message = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey = _mmsEngineDBFacade->addEMailConf(
                workspace->_workspaceKey, label, address, subject, message);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEMailConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEMailConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 201, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::modifyEMailConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyEMailConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string address;
        string subject;
        string message;
        
        try
        {
            Json::Value requestBodyRoot;
            
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

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "Address";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            address = requestBodyRoot.get(field, "XXX").asString();            

            field = "Subject";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            subject = requestBodyRoot.get(field, "XXX").asString();            

            field = "Message";
            if (!_mmsEngineDBFacade->isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            message = requestBodyRoot.get(field, "XXX").asString();            
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
        
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifyEMailConf(
                confKey, workspace->_workspaceKey, label, 
				address, subject, message);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEMailConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEMailConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::removeEMailConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeEMailConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeEMailConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEMailConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEMailConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(request, 200, sResponse);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::emailConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "emailConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
            
            Json::Value emailConfListRoot = _mmsEngineDBFacade->getEMailConfList(
                    workspace->_workspaceKey);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, emailConfListRoot);
            
            sendSuccess(request, 200, responseBody);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

                    throw runtime_error(errors);
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

            Validator validator(_logger, _mmsEngineDBFacade, _configuration);
            // it starts from the root and validate recursively the entire body
            validator.validateIngestedRootMetadata(workspace->_workspaceKey, 
                    requestBodyRoot);
        
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
                workspace->_workspaceKey, rootType, rootLabel, requestBody.c_str());
    
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

        string errorMessage = string("Internal server error: ") + e.what();
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
            encodingPriority = parametersRoot.get(field, "XXX").asString();
            /*
            string sRequestedEncodingPriority = parametersRoot.get(field, "XXX").asString();
            MMSEngineDBFacade::EncodingPriority requestedEncodingPriority = 
                    MMSEngineDBFacade::toEncodingPriority(sRequestedEncodingPriority);
            
            encodingPriority = MMSEngineDBFacade::toString(requestedEncodingPriority);
            */
        }
        /*
        else
        {
            encodingPriority = MMSEngineDBFacade::toString(
                    static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority));
        }
        */
        
            
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
            
            if (encodingPriority != "")
            {
                field = "EncodingPriority";
                newParametersRoot[field] = encodingPriority;
            }
            
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
    else if (type == "Live-Recorder")
    {
		// Live-Recorder generates MediaItems as soon as the files/segments are generated by the Live-Recorder.
		// For this reason, the events (onSuccess, onError, onComplete) has to be attached
		// to the workflow built to add these contents
		// Here, we will remove the events (onSuccess, onError, onComplete) from LiveRecorder, if present,
		// and we will add temporary inside the Parameters section. These events will be managed later
		// in EncoderVideoAudioProxy.cpp when the workflow for the generated contents will be created

		string onSuccessField = "OnSuccess";
		string onErrorField = "OnError";
		string onCompleteField = "OnComplete";
    	if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, onSuccessField)
			|| _mmsEngineDBFacade->isMetadataPresent(taskRoot, onErrorField)
			|| _mmsEngineDBFacade->isMetadataPresent(taskRoot, onCompleteField)
		)
    	{
			string internalMMSField = "InternalMMS";

        	Json::Value internalMMSRoot;

    		if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, onSuccessField))
			{
        		Json::Value onSuccessRoot = taskRoot[onSuccessField];

				internalMMSRoot[onSuccessField] = onSuccessRoot;

				Json::Value removed;
				taskRoot.removeMember(onSuccessField, &removed);
			}
    		if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, onErrorField))
			{
        		Json::Value onErrorRoot = taskRoot[onErrorField];

				internalMMSRoot[onErrorField] = onErrorRoot;

				Json::Value removed;
				taskRoot.removeMember(onErrorField, &removed);
			}
    		if (_mmsEngineDBFacade->isMetadataPresent(taskRoot, onCompleteField))
			{
        		Json::Value onCompleteRoot = taskRoot[onCompleteField];

				internalMMSRoot[onCompleteField] = onCompleteRoot;

				Json::Value removed;
				taskRoot.removeMember(onCompleteField, &removed);
			}

			parametersRoot[internalMMSField] = internalMMSRoot;
		}
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

void API::uploadedBinary(
        FCGX_Request& request,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool,bool,bool,bool> userKeyWorkspaceAndFlags,
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

        string errorMessage = string("Internal server error: ") + e.what();
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

            Validator validator(_logger, _mmsEngineDBFacade, _configuration);
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
            
            field = "Profiles";
            Json::Value profilesRoot = encodingProfilesSetRoot[field];

            for (int profileIndex = 0; profileIndex < profilesRoot.size(); profileIndex++)
            {
                string profileLabel = profilesRoot[profileIndex].asString();
                
                int64_t encodingProfileKey = _mmsEngineDBFacade->addEncodingProfileIntoSet(
                        conn, workspace->_workspaceKey, profileLabel,
                        contentType, encodingProfilesSetKey);

                if (responseBody != "")
                    responseBody += string(", ");
                responseBody += (
                        string("{ ") 
                        + "\"encodingProfileKey\": " + to_string(encodingProfileKey)
                        + ", \"label\": \"" + profileLabel + "\" "
                        + "}"
                        );
            }
            
            /*            
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
            */
            
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

        string errorMessage = string("Internal server error: ") + e.what();
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
            Validator validator(_logger, _mmsEngineDBFacade, _configuration);
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

        string errorMessage = string("Internal server error: ") + e.what();
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

void API::removeEncodingProfile(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeEncodingProfile";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto encodingProfileKeyIt = queryParameters.find("encodingProfileKey");
        if (encodingProfileKeyIt == queryParameters.end())
        {
            string errorMessage = string("'encodingProfileKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t encodingProfileKey = stoll(encodingProfileKeyIt->second);
        
        try
        {
            _mmsEngineDBFacade->removeEncodingProfile(
                workspace->_workspaceKey, encodingProfileKey);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodingProfile failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodingProfile failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        string responseBody;
        
        sendSuccess(request, 200, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::removeEncodingProfilesSet(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeEncodingProfilesSet";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto encodingProfilesSetKeyIt = queryParameters.find("encodingProfilesSetKey");
        if (encodingProfilesSetKeyIt == queryParameters.end())
        {
            string errorMessage = string("'encodingProfilesSetKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t encodingProfilesSetKey = stoll(encodingProfilesSetKeyIt->second);
        
        try
        {
            _mmsEngineDBFacade->removeEncodingProfilesSet(
                workspace->_workspaceKey, encodingProfilesSetKey);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodingProfilesSet failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodingProfilesSet failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        string responseBody;
        
        sendSuccess(request, 200, responseBody);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
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

void API::killEncodingJob(string transcoderHost, int64_t encodingJobKey)
{
	string ffmpegEncoderURL;
	ostringstream response;
	try
	{
		ffmpegEncoderURL = _ffmpegEncoderProtocol
			+ "://"
			+ transcoderHost + ":"
			+ to_string(_ffmpegEncoderPort)
			+ _ffmpegEncoderKillEncodingURI
			+ "/" + to_string(encodingJobKey)
		;
            
		list<string> header;

		{
			string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":" + _ffmpegEncoderPassword);
			string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

			header.push_back(basicAuthorization);
		}
            
		curlpp::Cleanup cleaner;
		curlpp::Easy request;

		// Setting the URL to retrive.
		request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));
		request.setOpt(new curlpp::options::CustomRequest("DELETE"));

		if (_ffmpegEncoderProtocol == "https")
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

		request.setOpt(new curlpp::options::WriteStream(&response));

		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "killEncodingJob"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
		);
		request.perform();
		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
		_logger->info(__FILEREF__ + "killEncodingJob"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
			+ ", response.str: " + response.str()
		);

		string sResponse = response.str();

		// LF and CR create problems to the json parser...                                                    
		while (sResponse.back() == 10 || sResponse.back() == 13)                                              
			sResponse.pop_back();                                                                             

		{
			string message = __FILEREF__ + "Kill encoding response"                                       
				+ ", encodingJobKey: " + to_string(encodingJobKey)          
				+ ", sResponse: " + sResponse                                                                 
			;                                                                                             
			_logger->info(message);
		}

		long responseCode = curlpp::infos::ResponseCode::get(request);                                        
		if (responseCode != 200)
		{
			string errorMessage = __FILEREF__ + "Kill encoding URL failed"                                       
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", sResponse: " + sResponse                                                                 
				+ ", responseCode: " + to_string(responseCode)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "killEncoding URL failed (LogicError)"
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{ 
		string errorMessage = string("killEncoding URL failed (RuntimeError)")
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		;
          
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "killEncoding URL failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "killEncoding URL failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
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

