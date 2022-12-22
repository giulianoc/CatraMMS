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

#include "JSONUtils.h"
#include "AWSSigner.h"
#include <fstream>
#include <sstream>
#include <regex>
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "catralibraries/Convert.h"
#include "catralibraries/LdapWrapper.h"
#include "Validator.h"
#include "EMailSender.h"
#include "catralibraries/Encrypt.h"
// #include <openssl/md5.h>
#include <openssl/evp.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "API.h"

int main(int argc, char** argv) 
{

	try
	{
		bool noFileSystemAccess = false;

		if (argc == 2)
		{
			string sAPIType = argv[1];
			if (sAPIType == "NoFileSystem")
				noFileSystemAccess = true;
		}

		// Init libxml
		{
			xmlInitParser();
			LIBXML_TEST_VERSION
		}

		const char* configurationPathName = getenv("MMS_CONFIGPATHNAME");
		if (configurationPathName == nullptr)
		{
			cerr << "MMS API: the MMS_CONFIGPATHNAME environment variable is not defined" << endl;
        
			return 1;
		}
    
		Json::Value configuration = APICommon::loadConfigurationFile(configurationPathName);
    
		string logPathName =  configuration["log"]["api"].get("pathName", "XXX").asString();
		string logType =  configuration["log"]["api"].get("type", "").asString();
		bool stdout =  JSONUtils::asBool(configuration["log"]["api"], "stdout", false);
    
		std::vector<spdlog::sink_ptr> sinks;
		{
			if(logType == "daily")
			{
				int logRotationHour = JSONUtils::asInt(configuration["log"]["api"]["daily"],
					"rotationHour", 1);
				int logRotationMinute = JSONUtils::asInt(configuration["log"]["api"]["daily"],
					"rotationMinute", 1);

				auto dailySink = make_shared<spdlog::sinks::daily_file_sink_mt> (logPathName.c_str(),
					logRotationHour, logRotationMinute);
				sinks.push_back(dailySink);
			}
			else if(logType == "rotating")
			{
				int64_t maxSizeInKBytes = JSONUtils::asInt64(configuration["log"]["encoder"]["rotating"],
					"maxSizeInKBytes", 1000);
				int maxFiles = JSONUtils::asInt(configuration["log"]["api"]["rotating"],
					"maxFiles", 10);

				auto rotatingSink = make_shared<spdlog::sinks::rotating_file_sink_mt> (logPathName.c_str(),
					maxSizeInKBytes * 1000, maxFiles);
				sinks.push_back(rotatingSink);
			}

			if (stdout)
			{
				auto stdoutSink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
				sinks.push_back(stdoutSink);
			}
		}

		auto logger = std::make_shared<spdlog::logger>("API", begin(sinks), end(sinks));
		spdlog::register_logger(logger);

		// shared_ptr<spdlog::logger> logger = spdlog::stdout_logger_mt("API");
		// shared_ptr<spdlog::logger> logger = spdlog::daily_logger_mt("API", logPathName.c_str(), 11, 20);
    
		// trigger flush if the log severity is error or higher
		logger->flush_on(spdlog::level::trace);
    
		string logLevel =  configuration["log"]["api"].get("level", "XXX").asString();
		if (logLevel == "debug")
			spdlog::set_level(spdlog::level::debug); // trace, debug, info, warn, err, critical, off
		else if (logLevel == "info")
			spdlog::set_level(spdlog::level::info); // trace, debug, info, warn, err, critical, off
		else if (logLevel == "err")
			spdlog::set_level(spdlog::level::err); // trace, debug, info, warn, err, critical, off
		string pattern =  configuration["log"]["api"].get("pattern", "XXX").asString();
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

		size_t masterDbPoolSize = JSONUtils::asInt(configuration["database"]["master"], "apiPoolSize", 5);
		logger->info(__FILEREF__ + "Configuration item"
			+ ", database->master->apiPoolSize: " + to_string(masterDbPoolSize)
		);
		size_t slaveDbPoolSize = JSONUtils::asInt(configuration["database"]["slave"], "apiPoolSize", 5);
		logger->info(__FILEREF__ + "Configuration item"
			+ ", database->slave->apiPoolSize: " + to_string(slaveDbPoolSize)
		);
		logger->info(__FILEREF__ + "Creating MMSEngineDBFacade"
            );
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade = make_shared<MMSEngineDBFacade>(
            configuration, masterDbPoolSize, slaveDbPoolSize, logger);

		logger->info(__FILEREF__ + "Creating MMSStorage"
			+ ", noFileSystemAccess: " + to_string(noFileSystemAccess)
		);
		shared_ptr<MMSStorage> mmsStorage = make_shared<MMSStorage>(
			noFileSystemAccess, mmsEngineDBFacade, configuration, logger);

		shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization =
			make_shared<MMSDeliveryAuthorization>(configuration,
			mmsStorage, mmsEngineDBFacade, logger);

		FCGX_Init();

		int threadsNumber = JSONUtils::asInt(configuration["api"], "threadsNumber", 1);
		logger->info(__FILEREF__ + "Configuration item"
			+ ", api->threadsNumber: " + to_string(threadsNumber)
		);

		mutex fcgiAcceptMutex;
		API::FileUploadProgressData fileUploadProgressData;

		vector<shared_ptr<API>> apis;
		vector<thread> apiThreads;

		for (int threadIndex = 0; threadIndex < threadsNumber; threadIndex++)
		{
			shared_ptr<API> api = make_shared<API>(
				noFileSystemAccess,
				configuration, 
                mmsEngineDBFacade,
				mmsStorage,
				mmsDeliveryAuthorization,
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

		// libxml
		{
			// Shutdown libxml
			xmlCleanupParser();

			// this is to debug memory for regression tests
			xmlMemoryDump();
		}
	}
    catch(sql::SQLException se)
    {
        cerr << __FILEREF__ + "main failed. SQL exception"
            + ", se.what(): " + se.what()
        ;

        // throw se;
		return 1;
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

API::API(bool noFileSystemAccess, Json::Value configuration, 
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		shared_ptr<MMSStorage> mmsStorage,
		shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization,
		mutex* fcgiAcceptMutex,
		FileUploadProgressData* fileUploadProgressData,
		shared_ptr<spdlog::logger> logger)
    :APICommon(configuration, 
		fcgiAcceptMutex,
		mmsEngineDBFacade,
		logger) 
{
	_noFileSystemAccess = noFileSystemAccess;
	_mmsStorage = mmsStorage;
	_mmsDeliveryAuthorization = mmsDeliveryAuthorization;

    string encodingPriority =  _configuration["api"]["workspaceDefaults"].get("encodingPriority", "low").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->workspaceDefaults->encodingPriority: " + encodingPriority
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

	_maxPageSize = JSONUtils::asInt(configuration["database"], "maxPageSize", 5);
	logger->info(__FILEREF__ + "Configuration item"
		+ ", database->maxPageSize: " + to_string(_maxPageSize)
	);

    string encodingPeriod =  _configuration["api"]["workspaceDefaults"].get("encodingPeriod", "daily").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->workspaceDefaults->encodingPeriod: " + encodingPeriod
    );
    if (encodingPeriod == "daily")
        _encodingPeriodWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;
    else
        _encodingPeriodWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;

    _maxIngestionsNumberWorkspaceDefaultValue = JSONUtils::asInt(_configuration["api"]["workspaceDefaults"], "maxIngestionsNumber", 100);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->workspaceDefaults->maxIngestionsNumber: " + to_string(_maxIngestionsNumberWorkspaceDefaultValue)
    );
    _maxStorageInMBWorkspaceDefaultValue = JSONUtils::asInt(_configuration["api"]["workspaceDefaults"], "maxStorageInMB", 100);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->workspaceDefaults->maxStorageInMBWorkspaceDefaultValue: " + to_string(_maxStorageInMBWorkspaceDefaultValue)
    );
    _expirationInDaysWorkspaceDefaultValue = JSONUtils::asInt(_configuration["api"]["workspaceDefaults"], "expirationInDays", 30);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->workspaceDefaults->expirationInDaysWorkspaceDefaultValue: " + to_string(_expirationInDaysWorkspaceDefaultValue)
    );

	{
		Json::Value sharedEncodersPoolRoot = _configuration["api"]["sharedEncodersPool"];

		_sharedEncodersPoolLabel = sharedEncodersPoolRoot.get("label", "").asString();
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", api->sharedEncodersPool->label: " + _sharedEncodersPoolLabel
		);

		_sharedEncodersLabel = sharedEncodersPoolRoot["encodersLabel"];
	}

    _apiProtocol =  _configuration["api"].get("protocol", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->protocol: " + _apiProtocol
    );
    _apiHostname =  _configuration["api"].get("hostname", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->hostname: " + _apiHostname
    );
    _apiPort = JSONUtils::asInt(_configuration["api"], "port", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->port: " + to_string(_apiPort)
    );
    _apiVersion =  _configuration["api"].get("version", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->version: " + _apiVersion
    );

    Json::Value api = _configuration["api"];
    // _binaryBufferLength             = api["binary"].get("binaryBufferLength", "XXX").asInt();
    // _logger->info(__FILEREF__ + "Configuration item"
    //    + ", api->binary->binaryBufferLength: " + to_string(_binaryBufferLength)
    // );
    _progressUpdatePeriodInSeconds  = JSONUtils::asInt(api["binary"], "progressUpdatePeriodInSeconds", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->progressUpdatePeriodInSeconds: " + to_string(_progressUpdatePeriodInSeconds)
    );
    _webServerPort  = JSONUtils::asInt(api["binary"], "webServerPort", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->webServerPort: " + to_string(_webServerPort)
    );
    _maxProgressCallFailures  = JSONUtils::asInt(api["binary"], "maxProgressCallFailures", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->maxProgressCallFailures: " + to_string(_maxProgressCallFailures)
    );
    _progressURI  = api["binary"].get("progressURI", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->progressURI: " + _progressURI
    );
    
    _defaultTTLInSeconds  = JSONUtils::asInt(api["delivery"], "defaultTTLInSeconds", 60);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->defaultTTLInSeconds: " + to_string(_defaultTTLInSeconds)
    );

    _defaultMaxRetries  = JSONUtils::asInt(api["delivery"], "defaultMaxRetries", 60);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->defaultMaxRetries: " + to_string(_defaultMaxRetries)
    );

    _defaultRedirect  = JSONUtils::asBool(api["delivery"], "defaultRedirect", true);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->defaultRedirect: " + to_string(_defaultRedirect)
    );
    
    _deliveryProtocol  = api["delivery"].get("deliveryProtocol", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->deliveryProtocol: " + _deliveryProtocol
    );
    _deliveryHost_authorizationThroughParameter  = api["delivery"].get("deliveryHost_authorizationThroughParameter", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->delivery->deliveryHost_authorizationThroughParameter: " + _deliveryHost_authorizationThroughParameter
    );
    _deliveryHost_authorizationThroughPath  = api["delivery"].get("deliveryHost_authorizationThroughPath", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
		+ ", api->delivery->deliveryHost_authorizationThroughPath: " + _deliveryHost_authorizationThroughPath
	);

    _ldapEnabled  = JSONUtils::asBool(api["activeDirectory"], "enabled", false);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->activeDirectory->enabled: " + to_string(_ldapEnabled)
    );
    _ldapURL  = api["activeDirectory"].get("ldapURL", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->activeDirectory->ldapURL: " + _ldapURL
    );
    _ldapCertificatePathName  = api["activeDirectory"].get("certificatePathName", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->activeDirectory->certificatePathName: " + _ldapCertificatePathName
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
	_ldapDefaultWorkspaceKeys	= api["activeDirectory"].get("defaultWorkspaceKeys", 0).asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", api->activeDirectory->defaultWorkspaceKeys: " + _ldapDefaultWorkspaceKeys
	);

	_registerUserEnabled  = JSONUtils::asBool(api, "registerUserEnabled", false);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->registerUserEnabled: " + to_string(_registerUserEnabled)
    );
	_savingGEOUserInfo = JSONUtils::asBool(_configuration["mms"]["geoService"], "savingGEOUserInfo", false);
	if (_savingGEOUserInfo)
	{
		_geoServiceURL = _configuration["mms"]["geoService"].get("geoServiceURL", "").asString();
		_geoServiceTimeoutInSeconds = JSONUtils::asInt(_configuration["mms"]["geoService"], "geoServiceTimeoutInSeconds", 10);
	}

	/*
    _ffmpegEncoderProtocol = _configuration["ffmpeg"].get("encoderProtocol", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderProtocol: " + _ffmpegEncoderProtocol
    );
    _ffmpegEncoderPort = JSONUtils::asInt(_configuration["ffmpeg"], "encoderPort", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderPort: " + to_string(_ffmpegEncoderPort)
    );
	*/
    _ffmpegEncoderUser = _configuration["ffmpeg"].get("encoderUser", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderUser: " + _ffmpegEncoderUser
    );
    _ffmpegEncoderPassword = _configuration["ffmpeg"].get("encoderPassword", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderPassword: " + "..."
    );
    _ffmpegEncoderTimeoutInSeconds = JSONUtils::asInt(_configuration["ffmpeg"], "encoderTimeoutInSeconds", 120);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderTimeoutInSeconds: " + to_string(_ffmpegEncoderTimeoutInSeconds)
    );
    _ffmpegEncoderKillEncodingURI = _configuration["ffmpeg"].get("encoderKillEncodingURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderKillEncodingURI: " + _ffmpegEncoderKillEncodingURI
    );
    _ffmpegEncoderChangeLiveProxyPlaylistURI = _configuration["ffmpeg"].get("encoderChangeLiveProxyPlaylistURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->encoderChangeLiveProxyPlaylistURI: " + _ffmpegEncoderChangeLiveProxyPlaylistURI
    );

	_maxSecondsToWaitAPIIngestionLock  = JSONUtils::asInt(_configuration["mms"]["locks"], "maxSecondsToWaitAPIIngestionLock", 0);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", mms->locks->maxSecondsToWaitAPIIngestionLock: " + to_string(_maxSecondsToWaitAPIIngestionLock)
	);

	_keyPairId =  _configuration["aws"].get("keyPairId", "").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", aws->keyPairId: " + _keyPairId
	);
	_privateKeyPEMPathName =  _configuration["aws"]
		.get("privateKeyPEMPathName", "").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", aws->privateKeyPEMPathName: " + _privateKeyPEMPathName
	);
	_vodCloudFrontHostNamesRoot =  _configuration["aws"]["vodCloudFrontHostNames"];
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", aws->vodCloudFrontHostNames: " + "..."
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
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request,
	string requestURI,
	string requestMethod,
	unordered_map<string, string> queryParameters,
	bool basicAuthenticationPresent,
	tuple<int64_t,shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>&
		userKeyWorkspaceAndFlags,
	string apiKey,
	unsigned long contentLength,
	string requestBody,
	unordered_map<string, string>& requestDetails
)
{

    int64_t userKey;
    shared_ptr<Workspace> workspace;
    bool admin;
    bool createRemoveWorkspace;
    bool ingestWorkflow;
    bool createProfiles;
    bool deliveryAuthorization;
    bool shareWorkspace;
    bool editMedia;
    bool editConfiguration;
    bool killEncoding;
    bool cancelIngestionJob_;
    bool editEncodersPool;
    bool applicationRecorder;

    if (basicAuthenticationPresent)
    {
        tie(userKey, workspace, admin, createRemoveWorkspace, ingestWorkflow, createProfiles,
				deliveryAuthorization, shareWorkspace, editMedia, editConfiguration, killEncoding,
				cancelIngestionJob_, editEncodersPool, applicationRecorder) 
                = userKeyWorkspaceAndFlags;

        _logger->info(__FILEREF__ + "Received manageRequestAndResponse"
            + ", requestURI: " + requestURI
            + ", requestMethod: " + requestMethod
            + ", contentLength: " + to_string(contentLength)
            + ", userKey: " + to_string(userKey)
            + ", workspace->_name: " + workspace->_name
            + ", requestBody: " + requestBody
            + ", admin: " + to_string(admin)
            + ", createRemoveWorkspace: " + to_string(createRemoveWorkspace)
            + ", ingestWorkflow: " + to_string(ingestWorkflow)
            + ", createProfiles: " + to_string(createProfiles)
            + ", deliveryAuthorization: " + to_string(deliveryAuthorization)
            + ", shareWorkspace: " + to_string(shareWorkspace)
            + ", editMedia: " + to_string(editMedia)
            + ", editConfiguration: " + to_string(editConfiguration)
            + ", killEncoding: " + to_string(killEncoding)
            + ", cancelIngestionJob: " + to_string(cancelIngestionJob_)
            + ", editEncodersPool: " + to_string(editEncodersPool)
            + ", applicationRecorder: " + to_string(applicationRecorder)
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

    string version;
    auto versionIt = queryParameters.find("version");
    if (versionIt != queryParameters.end())
		version = versionIt->second;

    if (!basicAuthenticationPresent)
    {
        _logger->info(__FILEREF__ + "Received manageRequestAndResponse"
            + ", requestURI: " + requestURI
            + ", requestMethod: " + requestMethod
            + ", contentLength: " + to_string(contentLength)
            + ", method: " + method
			// next is to avoid to log the password
            + (method == "login" ? ", requestBody: ..." : (", requestBody: " + requestBody))
        );
    }

    if (method == "status")
    {
        try
        {
			Json::Value statusRoot;

			statusRoot["status"] = "API server up and running";
			statusRoot["version-api"] = version;

			string sJson = JSONUtils::toString(statusRoot);

            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, sJson);
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
    else if (method == "binaryAuthorization")
    {
        // since we are here, for sure user is authorized
        
		auto binaryVirtualHostNameIt = queryParameters.find("binaryVirtualHostName");
		auto binaryListenHostIt = queryParameters.find("binaryListenHost");

        // retrieve the HTTP_X_ORIGINAL_METHOD to retrieve the progress id (set in the nginx server configuration)
        auto progressIdIt = requestDetails.find("HTTP_X_ORIGINAL_METHOD");
        auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
        if (binaryVirtualHostNameIt != queryParameters.end()
				&& binaryListenHostIt != queryParameters.end()
				&& progressIdIt != requestDetails.end()
				&& originalURIIt != requestDetails.end())
        {
            int ingestionJobKeyIndex = originalURIIt->second.find_last_of("/");
            if (ingestionJobKeyIndex != string::npos)
            {
                try
                {
                    struct FileUploadProgressData::RequestData requestData;

                    requestData._progressId = progressIdIt->second;
                    requestData._binaryListenHost = binaryListenHostIt->second;
                    requestData._binaryVirtualHostName = binaryVirtualHostNameIt->second;
					// requestData._binaryListenIp = binaryVirtualHostNameIt->second;
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
                        + ", _binaryVirtualHostName: " + requestData._binaryVirtualHostName
                        + ", _binaryListenHost: " + requestData._binaryListenHost
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
        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
    }
    else if (method == "deliveryAuthorizationThroughParameter")
    {
        // retrieve the HTTP_X_ORIGINAL_METHOD to retrieve the token to be checked (set in the nginx server configuration)
        try
        {
			auto tokenIt = requestDetails.find("HTTP_X_ORIGINAL_METHOD");
			auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
			if (tokenIt == requestDetails.end()
				|| originalURIIt == requestDetails.end())
			{
				string errorMessage = string("deliveryAuthorization, not authorized")
					+ ", token: " + (tokenIt != requestDetails.end() ? tokenIt->second : "null")
					+ ", URI: " + (originalURIIt != requestDetails.end() ? originalURIIt->second : "null")
                          ;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			string contentURI = originalURIIt->second;
			size_t endOfURIIndex = contentURI.find_last_of("?");
			if (endOfURIIndex == string::npos)
			{
				string errorMessage = string("Wrong URI format")
					+ ", contentURI: " + contentURI
				;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			contentURI = contentURI.substr(0, endOfURIIndex);

			string tokenParameter = tokenIt->second;

			_logger->info(__FILEREF__ + "Calling checkDeliveryAuthorizationThroughParameter"
				+ ", contentURI: " + contentURI
				+ ", tokenParameter: " + tokenParameter
			);

			checkDeliveryAuthorizationThroughParameter(contentURI, tokenParameter);

			string responseBody;
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(runtime_error e)
        {
            string errorMessage = string("Not authorized");
            _logger->warn(__FILEREF__ + errorMessage);

			string responseBody;
			sendError(request, 403, errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = string("Not authorized: exception managing token");
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);
        }
    }
    else if (method == "deliveryAuthorizationThroughPath")
    {
        // retrieve the HTTP_X_ORIGINAL_METHOD to retrieve the token to be checked (set in the nginx server configuration)
        try
        {
			auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
			if (originalURIIt == requestDetails.end())
			{
				string errorMessage = string("deliveryAuthorization, not authorized")
					+ ", URI: " + (originalURIIt != requestDetails.end() ? originalURIIt->second : "null")
                          ;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string contentURI = originalURIIt->second;

			_logger->info(__FILEREF__ + "deliveryAuthorizationThroughPath. Calling checkAuthorizationThroughPath"
				+ ", contentURI: " + contentURI
			);

			checkDeliveryAuthorizationThroughPath(contentURI);

			string responseBody;
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, requestURI, requestMethod, 200, responseBody);
        }
        catch(runtime_error e)
        {
            string errorMessage = string("Not authorized");
            _logger->warn(__FILEREF__ + errorMessage);

			string responseBody;
			sendError(request, 403, errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = string("Not authorized: exception managing token");
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);
        }
    }
    else if (method == "manageHTTPStreamingManifest_authorizationThroughParameter")
    {
        try
        {
			if (_noFileSystemAccess)
			{
				string errorMessage = string("no rights to execute this method")
					+ ", _noFileSystemAccess: " + to_string(_noFileSystemAccess)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

            auto tokenIt = queryParameters.find("token");
            if (tokenIt == queryParameters.end())
			{
				string errorMessage = string("Not authorized: token parameter not present")
					;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			// we could have:
			//		- master manifest, token parameter: <token>---
			//		- secondary manifest (that has to be treated as a .ts delivery), token parameter:
			//			<encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
			bool secondaryManifest;
			int64_t tokenComingFromURL;

			bool isNumber = !(tokenIt->second).empty()
				&& find_if(
					(tokenIt->second).begin(),
					(tokenIt->second).end(),
					[](unsigned char c) {
						return !isdigit(c);
					})
					== (tokenIt->second).end();
			if (isNumber)
			{
				secondaryManifest = false;
				tokenComingFromURL = stoll(tokenIt->second);
			}
			else
			{
				secondaryManifest = true;
				// tokenComingFromURL will be initialized in the next statement
			}
			_logger->info(__FILEREF__ + "manageHTTPStreamingManifest"
				+ ", analizing the token " + tokenIt->second
				+ ", isNumber: " + to_string(isNumber)
				+ ", secondaryManifest: " + to_string(secondaryManifest)
			);

			string contentURI;
			{
				size_t endOfURIIndex = requestURI.find_last_of("?");
				if (endOfURIIndex == string::npos)
				{
					string errorMessage = string("Wrong URI format")
						+ ", requestURI: " + requestURI
					;
					_logger->info(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				contentURI = requestURI.substr(0, endOfURIIndex);
			}

			if (secondaryManifest)
			{
				auto cookieIt = queryParameters.find("cookie");
				if (cookieIt == queryParameters.end())
				{
					string errorMessage = string("The 'cookie' parameter is not found");
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				string cookie = cookieIt->second;

				string tokenParameter = tokenIt->second + "---" + cookie;
				_logger->info(__FILEREF__ + "Calling checkDeliveryAuthorizationThroughParameter"
					+ ", contentURI: " + contentURI
					+ ", tokenParameter: " + tokenParameter
				);
				tokenComingFromURL = checkDeliveryAuthorizationThroughParameter(contentURI, tokenParameter);
			}
			else
			{
				// cookie parameter is added inside catramms.nginx
				string mmsInfoCookie;
				auto cookieIt = queryParameters.find("cookie");
				if (cookieIt != queryParameters.end())
					mmsInfoCookie = cookieIt->second;

				_logger->info(__FILEREF__ + "manageHTTPStreamingManifest"
					+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
					+ ", mmsInfoCookie: " + mmsInfoCookie
				);

				if (mmsInfoCookie == "")
				{
					if (!_mmsEngineDBFacade->checkDeliveryAuthorization(tokenComingFromURL, contentURI))
					{
						string errorMessage = string("Not authorized: token invalid")
							+ ", contentURI: " + contentURI
							+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
						;
						_logger->info(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}

					_logger->info(__FILEREF__ + "token authorized"
						+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
					);
				}
				else
				{
					string sTokenComingFromCookie = Encrypt::opensslDecrypt(mmsInfoCookie);
					int64_t tokenComingFromCookie = stoll(sTokenComingFromCookie);

					if (tokenComingFromCookie != tokenComingFromURL)
					{
						string errorMessage = string("cookie invalid, let's check the token")
							+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
							+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
						;
						_logger->info(__FILEREF__ + errorMessage);

						if (!_mmsEngineDBFacade->checkDeliveryAuthorization(tokenComingFromURL, contentURI))
						{
							string errorMessage = string("Not authorized: token invalid")
								+ ", contentURI: " + contentURI
								+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
							;
							_logger->info(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}

						_logger->info(__FILEREF__ + "token authorized"
							+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
						);
					}
					else
					{
						_logger->info(__FILEREF__ + "cookie authorized"
							+ ", mmsInfoCookie: " + mmsInfoCookie
						);
					}
				}
			}

			// manifest authorized

			{
				string contentType;

				string m3u8Extension(".m3u8");
				if (contentURI.size() >= m3u8Extension.size() && 0 == contentURI.compare(
					contentURI.size()-m3u8Extension.size(), m3u8Extension.size(), m3u8Extension))
					contentType = "Content-type: application/x-mpegURL";
				else	// dash
					contentType = "Content-type: application/dash+xml";
				string cookieName = "mmsInfo";

				string responseBody;
				{
					string manifestPathFileName = _mmsStorage->getMMSRootRepository()
						+ contentURI.substr(1);

					_logger->info(__FILEREF__ + "Reading manifest file"
						+ ", manifestPathFileName: " + manifestPathFileName
					);

					if (!FileIO::isFileExisting (manifestPathFileName.c_str()))
					{
						string errorMessage = string("manifest file not existing")
							+ ", manifestPathFileName: " + manifestPathFileName
							;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}

					if (contentURI.size() >= m3u8Extension.size() && 0 == contentURI.compare(
						contentURI.size()-m3u8Extension.size(), m3u8Extension.size(), m3u8Extension))
					{
						std::ifstream manifestFile;

						manifestFile.open(manifestPathFileName, ios::in);
						if (!manifestFile.is_open())
						{
							string errorMessage = string("Not authorized: manifest file not opened")
								+ ", manifestPathFileName: " + manifestPathFileName
								;
							_logger->info(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}

						string manifestLine;
						string tsExtension = ".ts";
						string m3u8Extension = ".m3u8";
						string m3u8ExtXMedia = "#EXT-X-MEDIA";
						string endLine = "\n";
						while(getline(manifestFile, manifestLine))
						{
							if (manifestLine[0] != '#' &&

								// end with
								manifestLine.size() >= tsExtension.size()
								&& 0 == manifestLine.compare(manifestLine.size()-tsExtension.size(),
									tsExtension.size(), tsExtension)
							)
							{
								/*
								_logger->info(__FILEREF__ + "Creation token parameter for ts"
									+ ", manifestLine: " + manifestLine
									+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
								);
								*/
								string auth = Encrypt::opensslEncrypt(manifestLine + "+++" + to_string(tokenComingFromURL));
								responseBody += (manifestLine + "?token=" + auth + endLine);
							}
							else if (manifestLine[0] != '#' &&

								// end with
								manifestLine.size() >= m3u8Extension.size()
								&& 0 == manifestLine.compare(manifestLine.size()-m3u8Extension.size(),
									m3u8Extension.size(), m3u8Extension)
							)
							{
								// scenario where we have several .m3u8 manifest files
								/*
								_logger->info(__FILEREF__ + "Creation token parameter for m3u8"
									+ ", manifestLine: " + manifestLine
									+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
								);
								*/
								string auth = Encrypt::opensslEncrypt(manifestLine + "+++" + to_string(tokenComingFromURL));
								responseBody += (manifestLine + "?token=" + auth + endLine);
							}
							// start with
							else if (manifestLine.size() >= m3u8ExtXMedia.size() &&
								0 == manifestLine.compare(0, m3u8ExtXMedia.size(), m3u8ExtXMedia))
							{
								// #EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="eng",NAME="eng",AUTOSELECT=YES, DEFAULT=YES,URI="eng/1247999_384641.m3u8"
								string temp = "URI=\"";
								size_t uriStartIndex = manifestLine.find(temp);
								if (uriStartIndex != string::npos)
								{
									uriStartIndex += temp.size();
									size_t uriEndIndex = uriStartIndex;
									while(manifestLine[uriEndIndex] != '\"'
											&& uriEndIndex < manifestLine.size())
										uriEndIndex++;
									if (manifestLine[uriEndIndex] == '\"')
									{
										string uri = manifestLine.substr(uriStartIndex, uriEndIndex - uriStartIndex);
										/*
										_logger->info(__FILEREF__ + "Creation token parameter for m3u8"
											+ ", uri: " + uri
											+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
										);
										*/
										string auth = Encrypt::opensslEncrypt(uri + "+++" + to_string(tokenComingFromURL));
										string tokenParameter = string("?token=") + auth;

										manifestLine.insert(uriEndIndex, tokenParameter);
									}
								}

								responseBody += (manifestLine + endLine);
							}
							else
							{
								responseBody += (manifestLine + endLine);
							}
						}
						manifestFile.close();
					}
					else	// dash
					{
#if defined(LIBXML_TREE_ENABLED) && defined(LIBXML_OUTPUT_ENABLED) && \
defined(LIBXML_XPATH_ENABLED) && defined(LIBXML_SAX1_ENABLED)
	_logger->info(__FILEREF__ + "libxml define OK");
#else
	_logger->info(__FILEREF__ + "libxml define KO");
#endif

/*
<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
        xmlns="urn:mpeg:dash:schema:mpd:2011"
        xmlns:xlink="http://www.w3.org/1999/xlink"
        xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd"
        profiles="urn:mpeg:dash:profile:isoff-live:2011"
        type="dynamic"
        minimumUpdatePeriod="PT10S"
        suggestedPresentationDelay="PT10S"
        availabilityStartTime="2020-02-03T15:11:56Z"
        publishTime="2020-02-04T08:54:57Z"
        timeShiftBufferDepth="PT1M0.0S"
        minBufferTime="PT20.0S">
        <ProgramInformation>
        </ProgramInformation>
        <Period id="0" start="PT0.0S">
                <AdaptationSet id="0" contentType="video" segmentAlignment="true" bitstreamSwitching="true">
                        <Representation id="0" mimeType="video/mp4" codecs="avc1.640029" bandwidth="1494920" width="1024" height="576" frameRate="25/1">
                                <SegmentTemplate timescale="12800" initialization="init-stream$RepresentationID$.m4s" media="chunk-stream$RepresentationID$-$Number%05d$.m4s" startNumber="6373">
                                        <SegmentTimeline>
                                                <S t="815616000" d="128000" r="5" />
                                        </SegmentTimeline>
                                </SegmentTemplate>
                        </Representation>
                </AdaptationSet>
                <AdaptationSet id="1" contentType="audio" segmentAlignment="true" bitstreamSwitching="true">
                        <Representation id="1" mimeType="audio/mp4" codecs="mp4a.40.5" bandwidth="95545" audioSamplingRate="48000">
                                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2" />
                                <SegmentTemplate timescale="48000" initialization="init-stream$RepresentationID$.m4s" media="chunk-stream$RepresentationID$-$Number%05d$.m4s" startNumber="6373">
                                        <SegmentTimeline>
                                                <S t="3058557246" d="479232" />
                                                <S d="481280" />
                                                <S d="479232" r="1" />
                                                <S d="481280" />
                                                <S d="479232" />
                                        </SegmentTimeline>
                                </SegmentTemplate>
                        </Representation>
                </AdaptationSet>
        </Period>
</MPD>
*/
						xmlDocPtr doc = xmlParseFile(manifestPathFileName.c_str());
						if (doc == NULL)
						{
							string errorMessage = string("xmlParseFile failed")
								+ ", manifestPathFileName: " + manifestPathFileName
								;
							_logger->info(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}

						// xmlNode* rootElement = xmlDocGetRootElement(doc);

						/* Create xpath evaluation context */
						xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
						if(xpathCtx == NULL)
						{
							xmlFreeDoc(doc);

							string errorMessage = string("xmlXPathNewContext failed")
								+ ", manifestPathFileName: " + manifestPathFileName
								;
							_logger->info(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}

						if(xmlXPathRegisterNs(xpathCtx,
							BAD_CAST "xmlns",
							BAD_CAST "urn:mpeg:dash:schema:mpd:2011") != 0)
						{
							xmlXPathFreeContext(xpathCtx);
							xmlFreeDoc(doc);

							string errorMessage = string("xmlXPathRegisterNs xmlns:xsi")
								+ ", manifestPathFileName: " + manifestPathFileName
								;
							_logger->info(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}
						/*
						if(xmlXPathRegisterNs(xpathCtx,
							BAD_CAST "xmlns:xlink",
							BAD_CAST "http://www.w3.org/1999/xlink") != 0)
						{
							xmlXPathFreeContext(xpathCtx);
							xmlFreeDoc(doc);

							string errorMessage = string("xmlXPathRegisterNs xmlns:xlink")
								+ ", manifestPathFileName: " + manifestPathFileName
								;
							_logger->info(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}
						if(xmlXPathRegisterNs(xpathCtx,
							BAD_CAST "xsi:schemaLocation",
							BAD_CAST "http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd") != 0)
						{
							xmlXPathFreeContext(xpathCtx);
							xmlFreeDoc(doc);

							string errorMessage = string("xmlXPathRegisterNs xsi:schemaLocation")
								+ ", manifestPathFileName: " + manifestPathFileName
								;
							_logger->info(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}
						*/

						// Evaluate xpath expression
						const char *xpathExpr = "//xmlns:Period/xmlns:AdaptationSet/xmlns:Representation/xmlns:SegmentTemplate";
						xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(BAD_CAST xpathExpr, xpathCtx);
						if(xpathObj == NULL)
						{
							xmlXPathFreeContext(xpathCtx);
							xmlFreeDoc(doc);

							string errorMessage = string("xmlXPathEvalExpression failed")
								+ ", manifestPathFileName: " + manifestPathFileName
								;
							_logger->info(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}

						xmlNodeSetPtr nodes = xpathObj->nodesetval;
						_logger->info(__FILEREF__ + "processing mpd manifest file"
							+ ", manifestPathFileName: " + manifestPathFileName
							+ ", nodesNumber: " + to_string(nodes->nodeNr)
						);
						for (int nodeIndex = 0; nodeIndex < nodes->nodeNr; nodeIndex++)
						{
							if (nodes->nodeTab[nodeIndex] == NULL)
							{
								xmlXPathFreeContext(xpathCtx);
								xmlFreeDoc(doc);

								string errorMessage = string("nodes->nodeTab[nodeIndex] is null")
									+ ", manifestPathFileName: " + manifestPathFileName
									+ ", nodeIndex: " + to_string(nodeIndex)
								;
								_logger->info(__FILEREF__ + errorMessage);

								throw runtime_error(errorMessage);
							}

							const char* mediaAttributeName = "media";
							const char* initializationAttributeName = "initialization";
							xmlChar* mediaValue = xmlGetProp(nodes->nodeTab[nodeIndex],
									BAD_CAST mediaAttributeName);
							xmlChar* initializationValue = xmlGetProp(nodes->nodeTab[nodeIndex],
									BAD_CAST initializationAttributeName);
							if (mediaValue == (xmlChar*) NULL
									|| initializationValue == (xmlChar*) NULL)
							{
								xmlXPathFreeContext(xpathCtx);
								xmlFreeDoc(doc);

								string errorMessage = string("xmlGetProp failed")
									+ ", manifestPathFileName: " + manifestPathFileName
								;
								_logger->info(__FILEREF__ + errorMessage);

								throw runtime_error(errorMessage);
							}

							string auth = Encrypt::opensslEncrypt(string((char*) mediaValue)
								+ "+++" + to_string(tokenComingFromURL));
							string newMediaAttributeValue = string((char*) mediaValue)
								+ "?token=" + auth;
							// xmlAttrPtr
							xmlSetProp (nodes->nodeTab[nodeIndex],
								BAD_CAST mediaAttributeName,
								BAD_CAST newMediaAttributeValue.c_str());

							string newInitializationAttributeValue =
								string((char*) initializationValue)
								+ "?token=" + auth;
							// xmlAttrPtr
							xmlSetProp (nodes->nodeTab[nodeIndex],
								BAD_CAST initializationAttributeName,
								BAD_CAST newInitializationAttributeValue.c_str());

							// const char *value = "ssss";
							// xmlNodeSetContent(nodes->nodeTab[nodeIndex], BAD_CAST value);

							/*
							* All the elements returned by an XPath query are pointers to
							* elements from the tree *except* namespace nodes where the XPath
							* semantic is different from the implementation in libxml2 tree.
							* As a result when a returned node set is freed when
							* xmlXPathFreeObject() is called, that routine must check the
							* element type. But node from the returned set may have been removed
							* by xmlNodeSetContent() resulting in access to freed data.
							* This can be exercised by running
							*       valgrind xpath2 test3.xml '//discarded' discarded
							* There is 2 ways around it:
							*   - make a copy of the pointers to the nodes from the result set
							*     then call xmlXPathFreeObject() and then modify the nodes
							* or
							*   - remove the reference to the modified nodes from the node set
							*     as they are processed, if they are not namespace nodes.
							*/
							// if (nodes->nodeTab[nodeIndex]->type != XML_NAMESPACE_DECL)
							// 	nodes->nodeTab[nodeIndex] = NULL;
						}

						/* Cleanup of XPath data */
						xmlXPathFreeObject(xpathObj);
						xmlXPathFreeContext(xpathCtx);

						/* dump the resulting document */
						{
							xmlChar *xmlbuff;
							int buffersize;
							xmlDocDumpFormatMemoryEnc(doc, &xmlbuff, &buffersize, "UTF-8", 1);
							_logger->info(__FILEREF__ + "dumping mpd manifest file"
								+ ", manifestPathFileName: " + manifestPathFileName
								+ ", buffersize: " + to_string(buffersize)
							);

							responseBody = (char*) xmlbuff;

							xmlFree(xmlbuff);
							// xmlDocDump(stdout, doc);
						}

						/* free the document */
						xmlFreeDoc(doc);

						/*
						std::ifstream manifestFile(manifestPathFileName);
						std::stringstream buffer;
						buffer << manifestFile.rdbuf();

						responseBody = buffer.str();
						*/
					}
				}

				string cookieValue = Encrypt::opensslEncrypt(to_string(tokenComingFromURL));
				string cookiePath;
				{
					size_t cookiePathIndex = contentURI.find_last_of("/");
					if (cookiePathIndex == string::npos)
					{
						string errorMessage = string("Wrong URI format")
							+ ", contentURI: " + contentURI
							;
						_logger->info(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
					cookiePath = contentURI.substr(0, cookiePathIndex);
				}

				bool enableCorsGETHeader = true;
				string originHeader;
				{
					auto originIt = requestDetails.find("HTTP_ORIGIN");
					if (originIt != requestDetails.end())
						originHeader = originIt->second;
				}
				if (secondaryManifest)
					sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
						request, requestURI, requestMethod, 200, responseBody,
						contentType, "", "", "",
						enableCorsGETHeader, originHeader);
				else
					sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
						request, requestURI, requestMethod, 200, responseBody,
						contentType, cookieName, cookieValue, cookiePath,
						enableCorsGETHeader, originHeader);
			}
		}
        catch(runtime_error e)
        {
            string errorMessage = string("Not authorized");
            _logger->warn(__FILEREF__ + errorMessage);

			string responseBody;
			sendError(request, 403, errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = string("Not authorized: exception managing token");
            _logger->warn(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);
        }
    }
    else if (method == "login")
    {
        login(sThreadId, requestIdentifier, responseBodyCompressed, request, requestBody);
    }
    else if (method == "registerUser")
    {
        registerUser(sThreadId, requestIdentifier, responseBodyCompressed, request, requestBody);
    }
    else if (method == "updateUser")
    {
        updateUser(sThreadId, requestIdentifier, responseBodyCompressed, request, userKey,
			requestBody, admin);
    }
    else if (method == "createTokenToResetPassword")
    {
        createTokenToResetPassword(sThreadId, requestIdentifier, responseBodyCompressed,
			request, queryParameters);
    }
    else if (method == "resetPassword")
    {
        resetPassword(sThreadId, requestIdentifier, responseBodyCompressed,
			request, requestBody);
    }
    else if (method == "updateWorkspace")
    {
        updateWorkspace(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, userKey, requestBody);
    }
    else if (method == "setWorkspaceAsDefault")
    {
        setWorkspaceAsDefault(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, userKey, queryParameters, requestBody);
    }
    else if (method == "createWorkspace")
    {
        if (!admin && !createRemoveWorkspace)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createRemoveWorkspace: " + to_string(createRemoveWorkspace)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        createWorkspace(sThreadId, requestIdentifier, responseBodyCompressed,
			request, userKey, queryParameters, requestBody, admin);
    }
    else if (method == "deleteWorkspace")
    {
        if (!admin && !createRemoveWorkspace)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createRemoveWorkspace: " + to_string(createRemoveWorkspace)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        deleteWorkspace(sThreadId, requestIdentifier, responseBodyCompressed,
			request, userKey, workspace);
    }
    else if (method == "workspaceUsage")
    {
        workspaceUsage(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace);
    }
    else if (method == "shareWorkspace")
    {
        if (!admin && !shareWorkspace)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", shareWorkspace: " + to_string(shareWorkspace)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        shareWorkspace_(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "workspaceList")
    {
		workspaceList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, userKey, workspace, queryParameters, admin);
    }
    else if (method == "confirmRegistration")
    {
        confirmRegistration(sThreadId, requestIdentifier, responseBodyCompressed,
			request, queryParameters);
    }
    else if (method == "addEncoder")
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

		addEncoder(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, requestBody);
    }
    else if (method == "removeEncoder")
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

		removeEncoder(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "modifyEncoder")
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

		modifyEncoder(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "encoderList")
    {
		encoderList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, admin, queryParameters);
    }
    else if (method == "encodersPoolList")
    {
		encodersPoolList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, admin, queryParameters);
    }
    else if (method == "addEncodersPool")
    {
        if (!admin && !editEncodersPool)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editEncodersPool: " + to_string(editEncodersPool)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

		addEncodersPool(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, requestBody);
    }
    else if (method == "modifyEncodersPool")
    {
        if (!admin && !editEncodersPool)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editEncodersPool: " + to_string(editEncodersPool)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

		modifyEncodersPool(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeEncodersPool")
    {
        if (!admin && !editEncodersPool)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editEncodersPool: " + to_string(editEncodersPool)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

		removeEncodersPool(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "addAssociationWorkspaceEncoder")
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

		addAssociationWorkspaceEncoder(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "removeAssociationWorkspaceEncoder")
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

		removeAssociationWorkspaceEncoder(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "createDeliveryAuthorization")
    {
        if (!admin && !deliveryAuthorization)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", deliveryAuthorization: " + to_string(deliveryAuthorization)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

		string clientIPAddress = getClientIPAddress(requestDetails);

		createDeliveryAuthorization(sThreadId, requestIdentifier, responseBodyCompressed,
			request, userKey, workspace, clientIPAddress, queryParameters);
    }
    else if (method == "createBulkOfDeliveryAuthorization")
    {
        if (!admin && !deliveryAuthorization)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", deliveryAuthorization: " + to_string(deliveryAuthorization)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

		string clientIPAddress = getClientIPAddress(requestDetails);

		createBulkOfDeliveryAuthorization(sThreadId, requestIdentifier, responseBodyCompressed,
			request, userKey, workspace, clientIPAddress, queryParameters, requestBody);
    }
    else if (method == "createDeliveryCDN77Authorization")
    {
        if (!admin && !deliveryAuthorization)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", deliveryAuthorization: " + to_string(deliveryAuthorization)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

		string clientIPAddress = getClientIPAddress(requestDetails);

        createDeliveryCDN77Authorization(sThreadId, requestIdentifier, responseBodyCompressed,
			request, userKey, workspace,
			clientIPAddress, queryParameters);
    }
    else if (method == "ingestion")
    {
        if (!admin && !ingestWorkflow)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", ingestWorkflow: " + to_string(ingestWorkflow)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        ingestion(sThreadId, requestIdentifier, responseBodyCompressed,
			request, userKey, apiKey, workspace, queryParameters, requestBody);
    }
    else if (method == "ingestionRootsStatus")
    {
        ingestionRootsStatus(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "ingestionRootMetaDataContent")
    {
        ingestionRootMetaDataContent(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "ingestionJobsStatus")
    {
        ingestionJobsStatus(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "cancelIngestionJob")
    {
        if (!admin && !cancelIngestionJob_)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", cancelIngestionJob: " + to_string(cancelIngestionJob_)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        cancelIngestionJob(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "updateIngestionJob")
    {
        if (!admin && !editMedia)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editMedia: " + to_string(editMedia)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        updateIngestionJob(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, userKey, queryParameters, requestBody, admin);
    }
    else if (method == "encodingJobsStatus")
    {
        encodingJobsStatus(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "encodingJobPriority")
    {
        encodingJobPriority(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "killOrCancelEncodingJob")
    {
        if (!admin && !killEncoding)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", killEncoding: " + to_string(killEncoding)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        killOrCancelEncodingJob(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "changeLiveProxyPlaylist")
    {
        changeLiveProxyPlaylist(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "mediaItemsList")
    {
        mediaItemsList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody, admin);
    }
    else if (method == "updateMediaItem")
    {
        if (!admin && !editMedia)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editMedia: " + to_string(editMedia)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        updateMediaItem(sThreadId, requestIdentifier, responseBodyCompressed, request,
			workspace, userKey, queryParameters, requestBody, admin);
    }
    else if (method == "updatePhysicalPath")
    {
        if (!admin && !editMedia)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editMedia: " + to_string(editMedia)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        updatePhysicalPath(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, userKey, queryParameters, requestBody, admin);
    }
    else if (method == "tagsList")
    {
        tagsList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "uploadedBinary")
    {
        uploadedBinary(sThreadId, requestIdentifier, responseBodyCompressed,
			request, requestMethod, queryParameters, workspace, // contentLength,
                requestDetails);
    }
    else if (method == "addEncodingProfilesSet")
    {
        if (!admin && !createProfiles)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createProfiles: " + to_string(createProfiles)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        addEncodingProfilesSet(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "encodingProfilesSetsList")
    {
        encodingProfilesSetsList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "addEncodingProfile")
    {
        if (!admin && !createProfiles)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createProfiles: " + to_string(createProfiles)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
                
        addEncodingProfile(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeEncodingProfile")
    {
        if (!admin && !createProfiles)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createProfiles: " + to_string(createProfiles)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }
                
        removeEncodingProfile(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "removeEncodingProfilesSet")
    {
        if (!admin && !createProfiles)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", createProfiles: " + to_string(createProfiles)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        removeEncodingProfilesSet(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "encodingProfilesList")
    {
        encodingProfilesList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "workflowsAsLibraryList")
    {
        workflowsAsLibraryList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "workflowAsLibraryContent")
    {
        workflowAsLibraryContent(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "saveWorkflowAsLibrary")
    {
        saveWorkflowAsLibrary(sThreadId, requestIdentifier, responseBodyCompressed,
			request, userKey, workspace, queryParameters, requestBody, admin);
    }
    else if (method == "removeWorkflowAsLibrary")
    {
        removeWorkflowAsLibrary(sThreadId, requestIdentifier, responseBodyCompressed,
			request, userKey, workspace, queryParameters, admin);
    }
    else if (method == "mmsSupport")
    {
		mmsSupport(sThreadId, requestIdentifier, responseBodyCompressed, request,
			userKey, apiKey, workspace, queryParameters, requestBody);
    }
    else if (method == "addYouTubeConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        addYouTubeConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyYouTubeConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        modifyYouTubeConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeYouTubeConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        removeYouTubeConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "youTubeConfList")
    {
        youTubeConfList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace);
    }
    else if (method == "addFacebookConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        addFacebookConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyFacebookConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        modifyFacebookConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeFacebookConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        removeFacebookConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "facebookConfList")
    {
        facebookConfList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace);
    }
    else if (method == "addStream")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        addStream(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyStream")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        modifyStream(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeStream")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        removeStream(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "streamList")
    {
        streamList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "addSourceTVStream")
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

        addSourceTVStream(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifySourceTVStream")
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

        modifySourceTVStream(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeSourceTVStream")
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

        removeSourceTVStream(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "sourceTVStreamList")
    {
        sourceTVStreamList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "addAWSChannelConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        addAWSChannelConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyAWSChannelConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        modifyAWSChannelConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeAWSChannelConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        removeAWSChannelConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "awsChannelConfList")
    {
        awsChannelConfList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace);
    }
    else if (method == "addFTPConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        addFTPConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyFTPConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        modifyFTPConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeFTPConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        removeFTPConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "ftpConfList")
    {
        ftpConfList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace);
    }
    else if (method == "addEMailConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        addEMailConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "modifyEMailConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        modifyEMailConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "removeEMailConf")
    {
        if (!admin && !editConfiguration)
        {
            string errorMessage = string("APIKey does not have the permission"
                    ", editConfiguration: " + to_string(editConfiguration)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 403, errorMessage);

            throw runtime_error(errorMessage);
        }

        removeEMailConf(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "emailConfList")
    {
        emailConfList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace);
    }
    else if (method == "addRequestStatistic")
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

        addRequestStatistic(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters, requestBody);
    }
    else if (method == "requestStatisticList")
    {
		requestStatisticList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "requestStatisticPerContentList")
    {
		requestStatisticPerContentList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "requestStatisticPerUserList")
    {
		requestStatisticPerUserList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "requestStatisticPerMonthList")
    {
		requestStatisticPerMonthList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "requestStatisticPerDayList")
    {
		requestStatisticPerDayList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
    }
    else if (method == "requestStatisticPerHourList")
    {
		requestStatisticPerHourList(sThreadId, requestIdentifier, responseBodyCompressed,
			request, workspace, queryParameters);
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
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
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
			if (mediaItemKey == 0)
				mediaItemKey = -1;
        }

        string uniqueName;
        auto uniqueNameIt = queryParameters.find("uniqueName");
        if (uniqueNameIt != queryParameters.end())
        {
			uniqueName = uniqueNameIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(uniqueName, regex(plus), plusDecoded);

			uniqueName = curlpp::unescape(firstDecoding);
        }

        int64_t encodingProfileKey = -1;
        auto encodingProfileKeyIt = queryParameters.find("encodingProfileKey");
        if (encodingProfileKeyIt != queryParameters.end())
		{
			encodingProfileKey = stoll(encodingProfileKeyIt->second);
			if (encodingProfileKey == 0)
				encodingProfileKey = -1;
		}

        string encodingProfileLabel;
        auto encodingProfileLabelIt = queryParameters.find("encodingProfileLabel");
        if (encodingProfileLabelIt != queryParameters.end())
        {
			encodingProfileLabel = encodingProfileLabelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(encodingProfileLabel, regex(plus), plusDecoded);

			encodingProfileLabel = curlpp::unescape(firstDecoding);
        }

		// this is for live authorization
        int64_t ingestionJobKey = -1;
        auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
        if (ingestionJobKeyIt != queryParameters.end())
        {
			ingestionJobKey = stoll(ingestionJobKeyIt->second);
        }

		// this is for live authorization
        int64_t deliveryCode = -1;
        auto deliveryCodeIt = queryParameters.find("deliveryCode");
        if (deliveryCodeIt != queryParameters.end())
        {
			deliveryCode = stoll(deliveryCodeIt->second);
        }

		if (physicalPathKey == -1
			&& ((mediaItemKey == -1 && uniqueName == "") || (encodingProfileKey == -1 && encodingProfileLabel == ""))
			&& ingestionJobKey == -1)
		{
            string errorMessage = string("The 'physicalPathKey' or the (mediaItemKey-uniqueName)/(encodingProfileKey-encodingProfileLabel) or ingestionJobKey parameters have to be present");
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

        string deliveryType;
        auto deliveryTypeIt = queryParameters.find("deliveryType");
        if (deliveryTypeIt != queryParameters.end())
			deliveryType = deliveryTypeIt->second;

        bool filteredByStatistic = false;
        auto filteredByStatisticIt = queryParameters.find("filteredByStatistic");
        if (filteredByStatisticIt != queryParameters.end())
        {
            if (filteredByStatisticIt->second == "true")
                filteredByStatistic = true;
            else
                filteredByStatistic = false;
        }

		string userId;
		auto userIdIt = queryParameters.find("userId");
		if (userIdIt != queryParameters.end())
		{
			userId = userIdIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(userId, regex(plus), plusDecoded);

			userId = curlpp::unescape(firstDecoding);
		}


		try
		{
			bool warningIfMissingMediaItemKey = false;
			pair<string, string> deliveryAuthorizationDetails =
				_mmsDeliveryAuthorization->createDeliveryAuthorization(
				userKey,
				requestWorkspace,
				clientIPAddress,

				mediaItemKey,
				uniqueName,
				encodingProfileKey,
				encodingProfileLabel,

				physicalPathKey,

				ingestionJobKey,
				deliveryCode,

				ttlInSeconds,
				maxRetries,
				save,
				deliveryType,

				warningIfMissingMediaItemKey,
				filteredByStatistic,
				userId
			);

			string deliveryURL;
			string deliveryFileName;

			tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

			if (redirect)
			{
				sendRedirect(request, deliveryURL);
			}
			else
			{
				Json::Value responseRoot;

				string field = "deliveryURL";
				responseRoot[field] = deliveryURL;

				field = "deliveryFileName";
				responseRoot[field] = deliveryFileName;

				field = "ttlInSeconds";
				responseRoot[field] = ttlInSeconds;

				field = "maxRetries";
				responseRoot[field] = maxRetries;

				string responseBody = JSONUtils::toString(responseRoot);

				/*
				string responseBody = string("{ ")
					+ "\"deliveryURL\": \"" + deliveryURL + "\""
					+ ", \"deliveryFileName\": \"" + deliveryFileName + "\""
					+ ", \"ttlInSeconds\": " + to_string(ttlInSeconds)
					+ ", \"maxRetries\": " + to_string(maxRetries)
					+ " }";
				*/
				sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
					request, "", api, 201, responseBody);
			}
        }
        catch(MediaItemKeyNotFound e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

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

void API::createBulkOfDeliveryAuthorization(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request,
	int64_t userKey,
	shared_ptr<Workspace> requestWorkspace,
	string clientIPAddress,
	unordered_map<string, string> queryParameters,
	string requestBody)
{
    string api = "createBulkOfDeliveryAuthorization";

    _logger->info(__FILEREF__ + "Received " + api
    );

	try
	{
		/*
		 * input:
            {
                "uniqueNameList" : [
                    {
                        "uniqueName": "...",
						"encodingProfileKey": 123
                    },
                    ...
                ],
                "liveIngestionJobKeyList" : [
                    {
                        "ingestionJobKey": 1234
                    },
                    ...
                ]
             }
			output is like the input with the addition of the deliveryURL field
		*/
		Json::Value deliveryAutorizationDetailsRoot;
        try
        {
			deliveryAutorizationDetailsRoot = JSONUtils::toJson(-1, -1, requestBody);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + e.what());

            sendError(request, 400, e.what());

            throw runtime_error(e.what());
        }

		try
		{
			int ttlInSeconds = _defaultTTLInSeconds;
			auto ttlInSecondsIt = queryParameters.find("ttlInSeconds");
			if (ttlInSecondsIt != queryParameters.end() && ttlInSecondsIt->second != "")
				ttlInSeconds = stol(ttlInSecondsIt->second);

			int maxRetries = _defaultMaxRetries;
			auto maxRetriesIt = queryParameters.find("maxRetries");
			if (maxRetriesIt != queryParameters.end() && maxRetriesIt->second != "")
				maxRetries = stol(maxRetriesIt->second);

			bool save = false;

			string field = "mediaItemKeyList";
			if (JSONUtils::isMetadataPresent(deliveryAutorizationDetailsRoot, field))
			{
				Json::Value mediaItemKeyListRoot = deliveryAutorizationDetailsRoot[field];
				for (int mediaItemKeyIndex = 0;
					mediaItemKeyIndex < mediaItemKeyListRoot.size();
					mediaItemKeyIndex++)
				{
					Json::Value mediaItemKeyRoot = mediaItemKeyListRoot[mediaItemKeyIndex];

					field = "mediaItemKey";
					int64_t mediaItemKey = JSONUtils::asInt64(mediaItemKeyRoot, field, -1);
					field = "encodingProfileKey";
					int64_t encodingProfileKey = JSONUtils::asInt64(mediaItemKeyRoot,
						field, -1);
					field = "encodingProfileLabel";
					string encodingProfileLabel = mediaItemKeyRoot.get(field, "").asString();

					field = "deliveryType";
					string deliveryType = mediaItemKeyRoot.get(field, "").asString();

					field = "filteredByStatistic";
					bool filteredByStatistic  = JSONUtils::asBool(mediaItemKeyRoot, field, false);

					field = "userId";
					string userId = mediaItemKeyRoot.get(field, "").asString();

					pair<string, string> deliveryAuthorizationDetails;
					try
					{
						bool warningIfMissingMediaItemKey = true;
						deliveryAuthorizationDetails =
							_mmsDeliveryAuthorization->createDeliveryAuthorization(
							userKey,
							requestWorkspace,
							clientIPAddress,

							mediaItemKey,
							"",	// uniqueName,
							encodingProfileKey,
							encodingProfileLabel,

							-1,	// physicalPathKey,

							-1,	// ingestionJobKey,
							-1,	// deliveryCode,

							ttlInSeconds,
							maxRetries,
							save,
							deliveryType,
							warningIfMissingMediaItemKey,
							filteredByStatistic,
							userId
						);
					}
					catch (MediaItemKeyNotFound e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (runtime_error e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (exception e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}

					string deliveryURL;
					string deliveryFileName;

					tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

					field = "deliveryURL";
					mediaItemKeyRoot[field] = deliveryURL;

					mediaItemKeyListRoot[mediaItemKeyIndex] = mediaItemKeyRoot;
				}

				field = "mediaItemKeyList";
				deliveryAutorizationDetailsRoot[field] = mediaItemKeyListRoot;
			}

			field = "uniqueNameList";
			if (JSONUtils::isMetadataPresent(deliveryAutorizationDetailsRoot, field))
			{
				Json::Value uniqueNameListRoot = deliveryAutorizationDetailsRoot[field];
				for (int uniqueNameIndex = 0; uniqueNameIndex < uniqueNameListRoot.size();
					uniqueNameIndex++)
				{
					Json::Value uniqueNameRoot = uniqueNameListRoot[uniqueNameIndex];

					field = "uniqueName";
					string uniqueName = uniqueNameRoot.get(field, "").asString();
					field = "encodingProfileKey";
					int64_t encodingProfileKey = JSONUtils::asInt64(uniqueNameRoot, field, -1);
					field = "encodingProfileLabel";
					string encodingProfileLabel = uniqueNameRoot.get(field, "").asString();

					field = "deliveryType";
					string deliveryType = uniqueNameRoot.get(field, "").asString();

					field = "filteredByStatistic";
					bool filteredByStatistic  = JSONUtils::asBool(uniqueNameRoot, field, false);

					field = "userId";
					string userId = uniqueNameRoot.get(field, "").asString();

					pair<string, string> deliveryAuthorizationDetails;
					try
					{
						bool warningIfMissingMediaItemKey = true;
						deliveryAuthorizationDetails =
							_mmsDeliveryAuthorization->createDeliveryAuthorization(
							userKey,
							requestWorkspace,
							clientIPAddress,

							-1,	// mediaItemKey,
							uniqueName,
							encodingProfileKey,
							encodingProfileLabel,

							-1,	// physicalPathKey,

							-1,	// ingestionJobKey,
							-1,	// deliveryCode,

							ttlInSeconds,
							maxRetries,
							save,
							deliveryType,
							warningIfMissingMediaItemKey,
							filteredByStatistic,
							userId
						);
					}
					catch (MediaItemKeyNotFound e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", uniqueName: " + uniqueName
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (runtime_error e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", uniqueName: " + uniqueName
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (exception e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", uniqueName: " + uniqueName
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", e.what(): " + e.what()
						);

						continue;
					}

					string deliveryURL;
					string deliveryFileName;

					tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

					field = "deliveryURL";
					uniqueNameRoot[field] = deliveryURL;

					uniqueNameListRoot[uniqueNameIndex] = uniqueNameRoot;
				}

				field = "uniqueNameList";
				deliveryAutorizationDetailsRoot[field] = uniqueNameListRoot;
			}

			field = "liveIngestionJobKeyList";
			if (JSONUtils::isMetadataPresent(deliveryAutorizationDetailsRoot, field))
			{
				Json::Value liveIngestionJobKeyListRoot = deliveryAutorizationDetailsRoot[field];
				for (int liveIngestionJobKeyIndex = 0;
					liveIngestionJobKeyIndex < liveIngestionJobKeyListRoot.size();
					liveIngestionJobKeyIndex++)
				{
					Json::Value liveIngestionJobKeyRoot = liveIngestionJobKeyListRoot[liveIngestionJobKeyIndex];

					field = "ingestionJobKey";
					int64_t ingestionJobKey = JSONUtils::asInt64(liveIngestionJobKeyRoot, field, -1);
					field = "deliveryCode";
					int64_t deliveryCode = JSONUtils::asInt64(liveIngestionJobKeyRoot, field, -1);

					field = "deliveryType";
					string deliveryType = liveIngestionJobKeyRoot.get(field, "").asString();

					field = "filteredByStatistic";
					bool filteredByStatistic  = JSONUtils::asBool(liveIngestionJobKeyRoot, field, false);

					field = "userId";
					string userId = liveIngestionJobKeyRoot.get(field, "").asString();

					pair<string, string> deliveryAuthorizationDetails;
					try
					{
						bool warningIfMissingMediaItemKey = false;
						deliveryAuthorizationDetails =
							_mmsDeliveryAuthorization->createDeliveryAuthorization(
							userKey,
							requestWorkspace,
							clientIPAddress,

							-1,	// mediaItemKey,
							"",	// uniqueName,
							-1,	// encodingProfileKey,
							"",	// encodingProfileLabel,

							-1,	// physicalPathKey,

							ingestionJobKey,
							deliveryCode,

							ttlInSeconds,
							maxRetries,
							save,
							deliveryType,
							warningIfMissingMediaItemKey,
							filteredByStatistic,
							userId
						);
					}
					catch (MediaItemKeyNotFound e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", deliveryCode: " + to_string(deliveryCode)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (runtime_error e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", deliveryCode: " + to_string(deliveryCode)
							+ ", e.what(): " + e.what()
						);

						continue;
					}
					catch (exception e)
					{
						_logger->error(__FILEREF__ + "createDeliveryAuthorization failed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", deliveryCode: " + to_string(deliveryCode)
							+ ", e.what(): " + e.what()
						);

						continue;
					}

					string deliveryURL;
					string deliveryFileName;

					tie(deliveryURL, deliveryFileName) = deliveryAuthorizationDetails;

					field = "deliveryURL";
					liveIngestionJobKeyRoot[field] = deliveryURL;

					liveIngestionJobKeyListRoot[liveIngestionJobKeyIndex] = liveIngestionJobKeyRoot;
				}

				field = "liveIngestionJobKeyList";
				deliveryAutorizationDetailsRoot[field] = liveIngestionJobKeyListRoot;
			}

			{
				string responseBody = JSONUtils::toString(deliveryAutorizationDetailsRoot);

				sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
					request, "", api, 201, responseBody);
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

int64_t API::checkDeliveryAuthorizationThroughParameter(
		string contentURI, string tokenParameter)
{
	int64_t tokenComingFromURL;
	try
	{
		_logger->info(__FILEREF__ + "checkDeliveryAuthorizationThroughParameter, received"
			+ ", contentURI: " + contentURI
			+ ", tokenParameter: " + tokenParameter
		);

		string firstPartOfToken;
		string secondPartOfToken;
		{
			// token formats:
			// scenario in case of .ts (hls) delivery: <encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
			// scenario in case of .m4s (dash) delivery: ---<cookie: encription of 'token'>
			//		both encryption were built in 'manageHTTPStreamingManifest'
			// scenario in case of .m3u8 delivery:
			//		case 1. master .m3u8: <token>---
			//		case 2. secondary .m3u8: <encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
			// scenario in case of any other delivery: <token>---
			//		Any other delivery includes for example also the delivery/download of a .ts file, not part
			//		of an hls delivery. In this case we will have just a token as any normal download

			string separator = "---";
			size_t endOfTokenIndex = tokenParameter.rfind(separator);
			if (endOfTokenIndex == string::npos)
			{
				string errorMessage = string("Wrong token format, no --- is present")
					+ ", contentURI: " + contentURI
					+ ", tokenParameter: " + tokenParameter
				;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			firstPartOfToken = tokenParameter.substr(0, endOfTokenIndex);
			secondPartOfToken = tokenParameter.substr(endOfTokenIndex + separator.length());
		}

		// end with
		string tsExtension(".ts");		// hls
		string m4sExtension(".m4s");	// dash
		string m3u8Extension(".m3u8");	// m3u8
		if (
			(secondPartOfToken != "")	// secondPartOfToken is the cookie
			&&
			(
			(contentURI.size() >= tsExtension.size() && 0 == contentURI.compare(
			contentURI.size()-tsExtension.size(), tsExtension.size(), tsExtension))
			||
			(contentURI.size() >= m4sExtension.size() && 0 == contentURI.compare(
			contentURI.size()-m4sExtension.size(), m4sExtension.size(), m4sExtension))
			||
			(contentURI.size() >= m3u8Extension.size() && 0 == contentURI.compare(
			contentURI.size()-m3u8Extension.size(), m3u8Extension.size(), m3u8Extension)
			&& secondPartOfToken != "")
			)
		)
		{
			// .ts/m4s content to be authorized

			string encryptedToken = firstPartOfToken;
			string cookie = secondPartOfToken;

			if (cookie == "")
			{
				string errorMessage = string("cookie is wrong")
					+ ", contentURI: " + contentURI
					+ ", cookie: " + cookie
					+ ", firstPartOfToken: " + firstPartOfToken
					+ ", secondPartOfToken: " + secondPartOfToken
				;
				_logger->info(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			// manifestLineAndToken comes from ts URL
			string manifestLineAndToken = Encrypt::opensslDecrypt(encryptedToken);
			string manifestLine;
			// int64_t tokenComingFromURL;
			{
				string separator = "+++";
				size_t beginOfTokenIndex = manifestLineAndToken.rfind(separator);
				if (beginOfTokenIndex == string::npos)
				{
					string errorMessage = string("Wrong parameter format")
						+ ", contentURI: " + contentURI
						+ ", manifestLineAndToken: " + manifestLineAndToken
						;
					_logger->info(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				manifestLine = manifestLineAndToken.substr(0, beginOfTokenIndex);
				string sTokenComingFromURL = manifestLineAndToken.substr(beginOfTokenIndex + separator.length());
				tokenComingFromURL = stoll(sTokenComingFromURL);
			}

			string sTokenComingFromCookie = Encrypt::opensslDecrypt(cookie);
			int64_t tokenComingFromCookie = stoll(sTokenComingFromCookie);

			_logger->info(__FILEREF__ + "check token info"
				+ ", encryptedToken: " + encryptedToken
				+ ", manifestLineAndToken: " + manifestLineAndToken
				+ ", manifestLine: " + manifestLine
				+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
				+ ", cookie: " + cookie
				+ ", sTokenComingFromCookie: " + sTokenComingFromCookie
				+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
				+ ", contentURI: " + contentURI
			);

			if (tokenComingFromCookie != tokenComingFromURL

					// i.e., contentURI: /MMSLive/1/94/94446.ts, manifestLine: 94446.ts
					// 2020-02-04: commented because it does not work in case of dash
					// contentURI: /MMSLive/1/109/init-stream0.m4s
					// manifestLine: chunk-stream$RepresentationID$-$Number%05d$.m4s
					// || contentURI.find(manifestLine) == string::npos
			)
			{
				string errorMessage = string("Wrong parameter format")
					+ ", contentURI: " + contentURI
					+ ", manifestLine: " + manifestLine
					+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
					+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
					;
				_logger->info(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			_logger->info(__FILEREF__ + "token authorized"
				+ ", contentURI: " + contentURI
				+ ", manifestLine: " + manifestLine
				+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
				+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
			);

			// authorized
			// string responseBody;
			// sendSuccess(request, 200, responseBody);
		}
		/*
		else if (
			(contentURI.size() >= m4sExtension.size() && 0 == contentURI.compare(
			contentURI.size()-m4sExtension.size(), m4sExtension.size(), m4sExtension))
		)
		{
			// .m4s content to be authorized

			string encryptedToken = firstPartOfToken;
			string cookie = secondPartOfToken;

			// manifestLineAndToken comes from ts URL
			//string manifestLineAndToken = Encrypt::decrypt(encryptedToken);
			//string manifestLine;
			//int64_t tokenComingFromURL;
			//{
			//	string separator = "+++";
			//	size_t beginOfTokenIndex = manifestLineAndToken.rfind(separator);
			//	if (beginOfTokenIndex == string::npos)
			//	{
			//		string errorMessage = string("Wrong parameter format")
			//			+ ", contentURI: " + contentURI
			//			+ ", manifestLineAndToken: " + manifestLineAndToken
			//			;
			//		_logger->info(__FILEREF__ + errorMessage);
//
//					throw runtime_error(errorMessage);
//				}
//				manifestLine = manifestLineAndToken.substr(0, beginOfTokenIndex);
//				string sTokenComingFromURL = manifestLineAndToken.substr(beginOfTokenIndex + separator.length());
//				tokenComingFromURL = stoll(sTokenComingFromURL);
//			}

			string sTokenComingFromCookie = Encrypt::decrypt(cookie);
			int64_t tokenComingFromCookie = stoll(sTokenComingFromCookie);

//			if (tokenComingFromCookie != tokenComingFromURL
//
//					// i.e., contentURI: /MMSLive/1/94/94446.ts, manifestLine: 94446.ts
//					|| contentURI.find(manifestLine) == string::npos)
//			{
//				string errorMessage = string("Wrong parameter format")
//					+ ", contentURI: " + contentURI
//					+ ", manifestLine: " + manifestLine
//					+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
//					+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
//					;
//				_logger->info(__FILEREF__ + errorMessage);
//
//				throw runtime_error(errorMessage);
//			}

			_logger->info(__FILEREF__ + "token authorized"
				+ ", contentURI: " + contentURI
				// + ", manifestLine: " + manifestLine
				// + ", tokenComingFromURL: " + to_string(tokenComingFromURL)
				+ ", tokenComingFromCookie: " + to_string(tokenComingFromCookie)
			);

			string responseBody;
			sendSuccess(request, 200, responseBody);
		}
		*/
		else
		{
			tokenComingFromURL = stoll(firstPartOfToken);
			if (_mmsEngineDBFacade->checkDeliveryAuthorization(tokenComingFromURL, contentURI))
			{
				_logger->info(__FILEREF__ + "token authorized"
					+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
				);

				// authorized

				// string responseBody;
				// sendSuccess(request, 200, responseBody);
			}
			else
			{
				string errorMessage = string("Not authorized: token invalid")
					+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
                          ;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch(runtime_error e)
	{
		string errorMessage = string("Not authorized");
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch(exception e)
	{
		string errorMessage = string("Not authorized: exception managing token");
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}

	return tokenComingFromURL;
}

int64_t API::checkDeliveryAuthorizationThroughPath(
	string contentURI)
{
	int64_t tokenComingFromURL = -1;
	try
	{
		_logger->info(__FILEREF__ + "checkDeliveryAuthorizationThroughPath, received"
			+ ", contentURI: " + contentURI
		);

		string tokenLabel = "/token_";

		size_t startTokenIndex = contentURI.find("/token_");
		if (startTokenIndex == string::npos)
		{
			string errorMessage = string("Wrong token format")
				+ ", contentURI: " + contentURI
			;
			_logger->warn(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		startTokenIndex += tokenLabel.size();

		size_t endTokenIndex = contentURI.find(",", startTokenIndex);
		if (endTokenIndex == string::npos)
		{
			string errorMessage = string("Wrong token format")
				+ ", contentURI: " + contentURI
			;
			_logger->warn(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		size_t endExpirationIndex = contentURI.find("/", endTokenIndex);
		if (endExpirationIndex == string::npos)
		{
			string errorMessage = string("Wrong token format")
				+ ", contentURI: " + contentURI
			;
			_logger->warn(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string tokenSigned = contentURI.substr(startTokenIndex, endTokenIndex - startTokenIndex);
		string sExpirationTime = contentURI.substr(endTokenIndex + 1, endExpirationIndex - (endTokenIndex + 1));
		time_t expirationTime = stoll(sExpirationTime);



		string contentURIToBeVerified;

		size_t endContentURIIndex = contentURI.find("?", endExpirationIndex);
		if (endContentURIIndex == string::npos)
			contentURIToBeVerified = contentURI.substr(endExpirationIndex);
		else
			contentURIToBeVerified = contentURI.substr(endExpirationIndex, endContentURIIndex - endExpirationIndex);

		string m3u8Suffix(".m3u8");
		string tsSuffix(".ts");
		if (contentURIToBeVerified.size() >= m3u8Suffix.size()
			&& 0 == contentURIToBeVerified.compare(contentURIToBeVerified.size()-m3u8Suffix.size(), m3u8Suffix.size(), m3u8Suffix))
		{
			{
				size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
				if (endPathIndex != string::npos)
					contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
			}

			string md5Base64 = _mmsDeliveryAuthorization->getSignedPath(contentURIToBeVerified, expirationTime);

			_logger->info(__FILEREF__ + "Authorization through path (m3u8)"
				+ ", contentURI: " + contentURI
				+ ", contentURIToBeVerified: " + contentURIToBeVerified
				+ ", expirationTime: " + to_string(expirationTime)
				+ ", tokenSigned: " + tokenSigned
				+ ", md5Base64: " + md5Base64
			);

			if (md5Base64 != tokenSigned)
			{
				// we still try removing again the last directory to manage the scenario
				// of multi bitrate encoding or multi audio tracks (one director for each audio)
				{
					size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
					if (endPathIndex != string::npos)
						contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
				}

				string md5Base64 = _mmsDeliveryAuthorization->getSignedPath(contentURIToBeVerified, expirationTime);

				_logger->info(__FILEREF__ + "Authorization through path (m3u8 2)"
					+ ", contentURI: " + contentURI
					+ ", contentURIToBeVerified: " + contentURIToBeVerified
					+ ", expirationTime: " + to_string(expirationTime)
					+ ", tokenSigned: " + tokenSigned
					+ ", md5Base64: " + md5Base64
				);

				if (md5Base64 != tokenSigned)
				{
					string errorMessage = string("Wrong token (m3u8)")
						+ ", md5Base64: " + md5Base64
						+ ", tokenSigned: " + tokenSigned
					;
					_logger->warn(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
		else if (contentURIToBeVerified.size() >= tsSuffix.size()
			&& 0 == contentURIToBeVerified.compare(contentURIToBeVerified.size()-tsSuffix.size(), tsSuffix.size(), tsSuffix)
		)
		{
			// 2022-11-29: ci sono 3 casi per il download di un .ts:
			//	1. download NON dall'interno di un m3u8
			//	2. download dall'interno di un m3u8 single bitrate
			//	3. download dall'interno di un m3u8 multi bitrate

			{
				// check caso 1.
				string md5Base64 = _mmsDeliveryAuthorization->getSignedPath(contentURIToBeVerified,
					expirationTime);

				_logger->info(__FILEREF__ + "Authorization through path"
					+ ", contentURI: " + contentURI
					+ ", contentURIToBeVerified: " + contentURIToBeVerified
					+ ", expirationTime: " + to_string(expirationTime)
					+ ", tokenSigned: " + tokenSigned
					+ ", md5Base64: " + md5Base64
				);

				if (md5Base64 != tokenSigned)
				{
					// potremmo essere nel caso 2 o caso 3

					{
						size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
						if (endPathIndex != string::npos)
							contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
					}

					// check caso 2.
					md5Base64 = _mmsDeliveryAuthorization->getSignedPath(contentURIToBeVerified,
						expirationTime);

					_logger->info(__FILEREF__ + "Authorization through path (ts 1)"
						+ ", contentURI: " + contentURI
						+ ", contentURIToBeVerified: " + contentURIToBeVerified
						+ ", expirationTime: " + to_string(expirationTime)
						+ ", tokenSigned: " + tokenSigned
						+ ", md5Base64: " + md5Base64
					);

					if (md5Base64 != tokenSigned)
					{
						// dovremmo essere nel caso 3

						// we still try removing again the last directory to manage
						// the scenario of multi bitrate encoding
						{
							size_t endPathIndex = contentURIToBeVerified.find_last_of("/");
							if (endPathIndex != string::npos)
								contentURIToBeVerified = contentURIToBeVerified.substr(0, endPathIndex);
						}

						// check caso 3.
						string md5Base64 = _mmsDeliveryAuthorization->getSignedPath(contentURIToBeVerified,
							expirationTime);

						_logger->info(__FILEREF__ + "Authorization through path (ts 2)"
							+ ", contentURI: " + contentURI
							+ ", contentURIToBeVerified: " + contentURIToBeVerified
							+ ", expirationTime: " + to_string(expirationTime)
							+ ", tokenSigned: " + tokenSigned
							+ ", md5Base64: " + md5Base64
						);

						if (md5Base64 != tokenSigned)
						{
							// non siamo in nessuno dei 3 casi

							string errorMessage = string("Wrong token (ts)")
								+ ", md5Base64: " + md5Base64
								+ ", tokenSigned: " + tokenSigned
							;
							_logger->warn(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}
			}
		}
		else
		{
			string md5Base64 = _mmsDeliveryAuthorization->getSignedPath(contentURIToBeVerified, expirationTime);

			_logger->info(__FILEREF__ + "Authorization through path"
				+ ", contentURI: " + contentURI
				+ ", contentURIToBeVerified: " + contentURIToBeVerified
				+ ", expirationTime: " + to_string(expirationTime)
				+ ", tokenSigned: " + tokenSigned
				+ ", md5Base64: " + md5Base64
			);

			if (md5Base64 != tokenSigned)
			{
				string errorMessage = string("Wrong token")
					+ ", md5Base64: " + md5Base64
					+ ", tokenSigned: " + tokenSigned
				;
				_logger->warn(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}


		time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
		if (expirationTime < utcNow)
		{
			string errorMessage = string("Token expired")
				+ ", expirationTime: " + to_string(expirationTime)
				+ ", utcNow: " + to_string(utcNow)
			;
			_logger->warn(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch(runtime_error e)
	{
		string errorMessage = string("Not authorized");
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch(exception e)
	{
		string errorMessage = string("Not authorized: exception managing token");
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}

	return tokenComingFromURL;
}

/*
string API::getSignedPath(string contentURI, time_t expirationTime)
{
	string token = to_string(expirationTime) + contentURI;
	string md5Base64;
	{
		unsigned char digest[MD5_DIGEST_LENGTH];
		MD5((unsigned char*) token.c_str(), token.size(), digest);
		md5Base64 = Convert::base64_encode(digest, MD5_DIGEST_LENGTH);

		transform(md5Base64.begin(), md5Base64.end(), md5Base64.begin(),
			[](unsigned char c){
				if (c == '+')
					return '-';
				else if (c == '/')
					return '_';
				else
					return (char) c;
			}
		);
	}

	_logger->info(__FILEREF__ + "Authorization through path"
		+ ", contentURI: " + contentURI
		+ ", expirationTime: " + to_string(expirationTime)
		+ ", token: " + token
		+ ", md5Base64: " + md5Base64
	);


	return md5Base64;
}
*/

void API::createDeliveryCDN77Authorization(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request,
	int64_t userKey,
	shared_ptr<Workspace> requestWorkspace,
	string clientIPAddress,
	unordered_map<string, string> queryParameters)
{
	string api = "createDeliveryCDN77Authorization";

	_logger->info(__FILEREF__ + "Received " + api
	);

    try
    {
        auto confKeyIt = queryParameters.find("confKey");
        if (confKeyIt != queryParameters.end())
		{
            string errorMessage = string("The 'confKey' parameter has to be present");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
		}
		int64_t liveURLConfKey = stoll(confKeyIt->second);

		//  It's smart to set the expiration time as current time plus 5 minutes { time() + 300}.
		//  This way the link will be available only for the time needed to start the download.
		//  Afterwards a new link must be generated in order to download this content again.
        auto expireTimestampInSecondsIt = queryParameters.find("expireTimestampInSeconds");
        if (expireTimestampInSecondsIt != queryParameters.end())
		{
            string errorMessage = string("The 'expireTimestampInSeconds' parameter has to be present");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
		}
		int64_t expireTimestampInSeconds = stoll(expireTimestampInSecondsIt->second);

		string liveURLData;
		tuple<string, string, string> liveURLConfDetails
			= _mmsEngineDBFacade->getStreamDetails(
			requestWorkspace->_workspaceKey, liveURLConfKey);
		tie(ignore, ignore, liveURLData) = liveURLConfDetails;

		string cdn77DeliveryURL;
		string cdn77SecureToken;
		{
			Json::Value liveURLDataRoot = JSONUtils::toJson(-1, -1, liveURLData);

			string field = "deliveryURL";
			cdn77DeliveryURL = liveURLDataRoot.get(field, "").asString();

			field = "cdn77SecureToken";
			cdn77SecureToken = liveURLDataRoot.get(field, "").asString();
		}

		string protocolPart;
		string cdnResourceUrl;
		string filePath;
		{
			size_t cdnResourceUrlStart = cdn77DeliveryURL.find("://");
			if (cdnResourceUrlStart == string::npos)
			{
				string errorMessage = string("cdn77DeliveryURL format is wrong")
					+ ", cdn77DeliveryURL: " + cdn77DeliveryURL
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			cdnResourceUrlStart += 3;	// "://"
			// protocolPart: https://
			protocolPart = cdn77DeliveryURL.substr(0, cdnResourceUrlStart);

			size_t cdnResourceUrlEnd = cdn77DeliveryURL.find_first_of("/", cdnResourceUrlStart);
			if (cdnResourceUrlEnd == string::npos)
			{
				string errorMessage = string("cdn77DeliveryURL format is wrong")
					+ ", cdn77DeliveryURL: " + cdn77DeliveryURL
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			// cdnResourceUrl: 1234456789.rsc.cdn77.org
			cdnResourceUrl = cdn77DeliveryURL.substr(cdnResourceUrlStart, cdnResourceUrlEnd - cdnResourceUrlStart);
			// filePath: /file/playlist/d.m3u8
			filePath = cdn77DeliveryURL.substr(cdnResourceUrlEnd);
		}

		string newDeliveryURL;
		{
			// because of hls/dash, anything included after the last slash (e.g. playlist/{chunk}) shouldn't be part of the path string,
			// for which we generate the secure token. Because of that, everything included after the last slash is stripped.
			// $strippedPath = substr($filePath, 0, strrpos($filePath, '/'));
			size_t fileNameStart = filePath.find_last_of("/");
			if (fileNameStart == string::npos)
			{
				string errorMessage = string("filePath format is wrong")
					+ ", filePath: " + filePath
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			string strippedPath = filePath.substr(0, fileNameStart);

			// replace invalid URL query string characters +, =, / with valid characters -, _, ~
			// $invalidChars = ['+','/'];
			// $validChars = ['-','_'];
			// GIU: replace is done below

			// if ($strippedPath[0] != '/') {
			// 	$strippedPath = '/' . $strippedPath;
			// }
			// GIU: our strippedPath already starts with /

			// if ($pos = strpos($strippedPath, '?')) {
			// 	$filePath = substr($strippedPath, 0, $pos);
			// }
			// GIU: our strippedPath does not have ?

			// $hashStr = $strippedPath . $secureToken;
			string hashStr = strippedPath + cdn77SecureToken;

			// if ($expiryTimestamp) {
			// 	$hashStr = $expiryTimestamp . $hashStr;
			// 	$expiryTimestamp = ',' . $expiryTimestamp;
			// }
			hashStr = to_string(expireTimestampInSeconds) + hashStr;
			string sExpiryTimestamp = string(",") + to_string(expireTimestampInSeconds);

			// the URL is however, intensionaly returned with the previously stripped parts (eg. playlist/{chunk}..)
			// return 'http://' . $cdnResourceUrl . '/' .
			// 	str_replace($invalidChars, $validChars, base64_encode(md5($hashStr, TRUE))) .
			// 	$expiryTimestamp . $filePath;
			string md5Base64;
			{
				// unsigned char digest[MD5_DIGEST_LENGTH];
				// MD5((unsigned char*) hashStr.c_str(), hashStr.size(), digest);
				// md5Base64 = Convert::base64_encode(digest, MD5_DIGEST_LENGTH);

				{
					unsigned char *md5_digest;
					unsigned int md5_digest_len = EVP_MD_size(EVP_md5());

					EVP_MD_CTX *mdctx;

					// MD5_Init
					mdctx = EVP_MD_CTX_new();
					EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);

					// MD5_Update
					EVP_DigestUpdate(mdctx, (unsigned char*) hashStr.c_str(), hashStr.size());

					// MD5_Final
					md5_digest = (unsigned char *)OPENSSL_malloc(md5_digest_len);
					EVP_DigestFinal_ex(mdctx, md5_digest, &md5_digest_len);

					md5Base64 = Convert::base64_encode(md5_digest, md5_digest_len);

					OPENSSL_free(md5_digest);

					EVP_MD_CTX_free(mdctx);
				}

				// $invalidChars = ['+','/'];
				// $validChars = ['-','_'];
				transform(md5Base64.begin(), md5Base64.end(), md5Base64.begin(),
					[](unsigned char c){
						if (c == '+')
							return '-';
						else if (c == '/')
							return '_';
						else
							return (char) c;
					}
				);
			}

			newDeliveryURL = protocolPart + cdnResourceUrl + "/" + md5Base64
				+ sExpiryTimestamp + filePath;
		}

		string responseBody;
		{
			Json::Value responseBodyRoot;

			string field = "deliveryURL";
			responseBodyRoot[field] = newDeliveryURL;

			{
				responseBody = JSONUtils::toString(responseBodyRoot);
			}
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 201, responseBody);
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

void API::mmsSupport(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
		int64_t userKey, string apiKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "mmsSupport";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        string userEmailAddress;
        string subject;
        string text;

        Json::Value metadataRoot;
        try
        {
			metadataRoot = JSONUtils::toJson(-1, -1, requestBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + e.what());

            sendError(request, 400, e.what());

            throw runtime_error(e.what());
        }

		vector<string> mandatoryFields = {
			"UserEmailAddress",
			"Subject",
			"Text"
		};
		for (string field: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				string errorMessage = string("Json field is not present or it is null")
					+ ", Json field: " + field;
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		userEmailAddress = metadataRoot.get("UserEmailAddress", "").asString();
		subject = metadataRoot.get("Subject", "").asString();
		text = metadataRoot.get("Text", "").asString();

        try
        {
            string to = "mms.technical.support@catrasoft.cloud";

            vector<string> emailBody;
            emailBody.push_back(string("<p>UserKey: ") + to_string(userKey) + "</p>");
            emailBody.push_back(string("<p>WorkspaceKey: ")
				+ to_string(workspace->_workspaceKey) + "</p>");
            emailBody.push_back(string("<p>APIKey: ") + apiKey + "</p>");
            emailBody.push_back(string("<p></p>"));
            emailBody.push_back(string("<p>From: ") + userEmailAddress + "</p>");
            emailBody.push_back(string("<p></p>"));
            emailBody.push_back(string("<p>") + text + "</p>");

            EMailSender emailSender(_logger, _configuration);
			bool useMMSCCToo = true;
            emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);

            string responseBody;
            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, "", api, 201, responseBody);
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

