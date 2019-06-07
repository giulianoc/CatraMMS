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
#include "catralibraries/LdapWrapper.h"
#include "PersistenceLock.h"
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

    _ldapEnabled  = api["activeDirectory"].get("enabled", false).asBool();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->activeDirectory->enabled: " + to_string(_ldapEnabled)
    );
    _ldapURL  = api["activeDirectory"].get("ldapURL", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->activeDirectory->ldapURL: " + _ldapURL
    );
    _ldapManagerUserName  = api["activeDirectory"].get("managerUserName", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->activeDirectory->managerUserName: " + _ldapManagerUserName
    );
    _ldapManagerPassword  = api["activeDirectory"].get("managerPassword", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->activeDirectory->managerPassword: " + _ldapManagerPassword
    );
    _ldapBaseDn  = api["activeDirectory"].get("baseDn", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->activeDirectory->baseDn: " + _ldapBaseDn
    );
	_ldapDefaultWorkspaceKey  = api["activeDirectory"].get("defaultWorkspaceKey", 0).asInt64();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", api->activeDirectory->defaultWorkspaceKey: " + to_string(_ldapDefaultWorkspaceKey)
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
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool,bool,bool,bool,bool,bool>&
			userKeyWorkspaceAndFlags,
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
    bool editConfiguration;
    bool killEncoding;

    if (basicAuthenticationPresent)
    {
        tie(userKey, workspace, admin, ingestWorkflow, createProfiles,
				deliveryAuthorization, shareWorkspace, editMedia, editConfiguration, killEncoding) 
                = userKeyWorkspaceAndFlags;

        _logger->info(__FILEREF__ + "Received manageRequestAndResponse"
            + ", requestURI: " + requestURI
            + ", requestMethod: " + requestMethod
            + ", contentLength: " + to_string(contentLength)
            + ", userKey: " + to_string(userKey)
            + ", workspace->_name: " + workspace->_name
            + ", requestBody: " + requestBody
            + ", admin: " + to_string(admin)
            + ", ingestWorkflow: " + to_string(ingestWorkflow)
            + ", createProfiles: " + to_string(createProfiles)
            + ", deliveryAuthorization: " + to_string(deliveryAuthorization)
            + ", shareWorkspace: " + to_string(shareWorkspace)
            + ", editMedia: " + to_string(editMedia)
            + ", editConfiguration: " + to_string(editConfiguration)
            + ", killEncoding: " + to_string(killEncoding)
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

    if (!basicAuthenticationPresent)
    {
        _logger->info(__FILEREF__ + "Received manageRequestAndResponse"
            + ", requestURI: " + requestURI
            + ", requestMethod: " + requestMethod
            + ", contentLength: " + to_string(contentLength)
			// next is to avoid to log the password
            + (method == "login" ? ", requestBody: ..." : (", requestBody: " + requestBody))
        );
    }

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
    else if (method == "deleteWorkspace")
    {
        deleteWorkspace(request, userKey, workspace);
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
    else if (method == "killOrCancelEncodingJob")
    {
        killOrCancelEncodingJob(request, workspace, queryParameters, requestBody);
    }
    else if (method == "mediaItemsList")
    {
        mediaItemsList(request, workspace, queryParameters, requestBody, admin);
    }
    else if (method == "tagsList")
    {
        tagsList(request, workspace, queryParameters, requestBody);
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
        int64_t physicalPathKey = -1;
        auto physicalPathKeyIt = queryParameters.find("physicalPathKey");
        if (physicalPathKeyIt != queryParameters.end())
        {
			physicalPathKey = stoll(physicalPathKeyIt->second);
        }

        int64_t mediaItemKey = -1;
        auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
        if (mediaItemKeyIt != queryParameters.end())
        {
			mediaItemKey = stoll(mediaItemKeyIt->second);
        }

        int64_t encodingProfileKey = -1;
        auto encodingProfileKeyIt = queryParameters.find("encodingProfileKey");
        if (encodingProfileKeyIt != queryParameters.end())
        {
			encodingProfileKey = stoll(encodingProfileKeyIt->second);
        }

		if (physicalPathKey == -1 && (mediaItemKey == -1 || encodingProfileKey == -1))
		{
            string errorMessage = string("The 'physicalPathKey' or the mediaItemKey/encodingProfileKey parameters have to be present");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
		}

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
            tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails;

			if (physicalPathKey != -1)
                storageDetails = _mmsEngineDBFacade->getStorageDetails(physicalPathKey);
			else
                storageDetails = _mmsEngineDBFacade->getStorageDetails(mediaItemKey, encodingProfileKey);

            int mmsPartitionNumber;
            shared_ptr<Workspace> contentWorkspace;
            string relativePath;
            string fileName;
            string deliveryFileName;
            string title;
            int64_t sizeInBytes;
			if (physicalPathKey != -1)
				tie(ignore, mmsPartitionNumber, contentWorkspace, relativePath, fileName, 
                    deliveryFileName, title, sizeInBytes) = storageDetails;
			else
				tie(physicalPathKey, mmsPartitionNumber, contentWorkspace, relativePath, fileName, 
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

			CURL *curl = curl_easy_init();
			if(curl)
			{
				int outLength;
				char *decoded = curl_easy_unescape(curl,
						title.c_str(), title.length(), &outLength);
				if(decoded)
				{
					string sDecoded = decoded;
					curl_free(decoded);

					// still there is the '+' char
					string plus = "\\+";
					string plusDecoded = " ";
					title = regex_replace(sDecoded, regex(plus), plusDecoded);
				}
			}
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

void API::tagsList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "tagsList";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
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
        
        {
            Json::Value tagsRoot = _mmsEngineDBFacade->getTagsList(
                    workspace->_workspaceKey, start, rows,
                    contentTypePresent, contentType);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, tagsRoot);
            
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

