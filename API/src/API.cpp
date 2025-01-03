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

#include "AWSSigner.h"
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include "Validator.h"
#include "catralibraries/Convert.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/LdapWrapper.h"
#include "catralibraries/StringUtils.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <fstream>
#include <openssl/evp.h>
#include <regex>
#include <sstream>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "API.h"

API::API(
	bool noFileSystemAccess, json configurationRoot, shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, shared_ptr<MMSStorage> mmsStorage,
	shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization, mutex *fcgiAcceptMutex, FileUploadProgressData *fileUploadProgressData,
	shared_ptr<spdlog::logger> logger
)
	: FastCGIAPI(configurationRoot, fcgiAcceptMutex), _mmsEngineDBFacade(mmsEngineDBFacade), _noFileSystemAccess(noFileSystemAccess),
	  _mmsStorage(mmsStorage), _mmsDeliveryAuthorization(mmsDeliveryAuthorization)
{
	// _noFileSystemAccess = noFileSystemAccess;
	// _mmsStorage = mmsStorage;
	// _mmsDeliveryAuthorization = mmsDeliveryAuthorization;

	_logger = spdlog::default_logger();

	string encodingPriority = JSONUtils::asString(_configurationRoot["api"]["workspaceDefaults"], "encodingPriority", "low");
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->encodingPriority: {}",
		encodingPriority
	);
	try
	{
		{
			fs::path versionPathFileName = "/opt/catramms/CatraMMS/version.txt";
			if (fs::exists(versionPathFileName) && fs::is_regular_file(versionPathFileName))
			{
				ifstream f(versionPathFileName);
				stringstream buffer;
				buffer << f.rdbuf();
				_mmsVersion = buffer.str();
			}
		}

		_encodingPriorityWorkspaceDefaultValue =
			MMSEngineDBFacade::toEncodingPriority(encodingPriority); // it generate an exception in case of wrong string
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"Configuration item is wrong. 'low' encoding priority is set"
			", api->encodingPriorityWorkspaceDefaultValue: {}",
			encodingPriority
		);

		_encodingPriorityWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPriority::Low;
	}

	_maxPageSize = JSONUtils::asInt(_configurationRoot["postgres"], "maxPageSize", 5);
	logger->info(__FILEREF__ + "Configuration item" + ", postgres->maxPageSize: " + to_string(_maxPageSize));

	string encodingPeriod = JSONUtils::asString(_configurationRoot["api"]["workspaceDefaults"], "encodingPeriod", "daily");
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->encodingPeriod: {}",
		encodingPeriod
	);
	if (encodingPeriod == "daily")
		_encodingPeriodWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;
	else
		_encodingPeriodWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;

	_maxIngestionsNumberWorkspaceDefaultValue = JSONUtils::asInt(_configurationRoot["api"]["workspaceDefaults"], "maxIngestionsNumber", 100);
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->maxIngestionsNumber: {}",
		_maxIngestionsNumberWorkspaceDefaultValue
	);
	_maxStorageInMBWorkspaceDefaultValue = JSONUtils::asInt(_configurationRoot["api"]["workspaceDefaults"], "maxStorageInMB", 100);
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->maxStorageInMBWorkspaceDefaultValue: {}",
		_maxStorageInMBWorkspaceDefaultValue
	);
	_expirationInDaysWorkspaceDefaultValue = JSONUtils::asInt(_configurationRoot["api"]["workspaceDefaults"], "expirationInDays", 30);
	SPDLOG_INFO(
		"Configuration item"
		", api->workspaceDefaults->expirationInDaysWorkspaceDefaultValue: {}",
		_expirationInDaysWorkspaceDefaultValue
	);

	{
		json sharedEncodersPoolRoot = _configurationRoot["api"]["sharedEncodersPool"];

		_sharedEncodersPoolLabel = JSONUtils::asString(sharedEncodersPoolRoot, "label", "");
		SPDLOG_INFO(
			"Configuration item"
			", api->sharedEncodersPool->label: {}",
			_sharedEncodersPoolLabel
		);

		_sharedEncodersLabel = sharedEncodersPoolRoot["encodersLabel"];
	}

	_defaultSharedHLSChannelsNumber = JSONUtils::asInt(_configurationRoot["api"], "defaultSharedHLSChannelsNumber", 1);
	SPDLOG_INFO(
		"Configuration item"
		", api->defaultSharedHLSChannelsNumber: {}",
		_defaultSharedHLSChannelsNumber
	);

	/*
	_apiProtocol =  JSONUtils::asString(_configuration["api"], "protocol", "");
	SPDLOG_INFO("Configuration item"
		", api->protocol: {}", _apiProtocol
	);
	_apiHostname =  JSONUtils::asString(_configuration["api"], "hostname", "");
	SPDLOG_INFO("Configuration item"
		", api->hostname: {}", _apiHostname
	);
	_apiPort = JSONUtils::asInt(_configuration["api"], "port", 0);
	SPDLOG_INFO("Configuration item"
		", api->port: {}", _apiPort
	);
	_apiVersion =  JSONUtils::asString(_configuration["api"], "version", "");
	SPDLOG_INFO("Configuration item"
		", api->version: {}", _apiVersion
	);
	*/

	json api = _configurationRoot["api"];
	// _binaryBufferLength             = api["binary"].get("binaryBufferLength", "XXX").asInt();
	// SPDLOG_INFO(__FILEREF__ + "Configuration item"
	//    + ", api->binary->binaryBufferLength: " + to_string(_binaryBufferLength)
	// );
	_progressUpdatePeriodInSeconds = JSONUtils::asInt(api["binary"], "progressUpdatePeriodInSeconds", 0);
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->progressUpdatePeriodInSeconds: {}",
		_progressUpdatePeriodInSeconds
	);
	_webServerPort = JSONUtils::asInt(api["binary"], "webServerPort", 0);
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->webServerPort: {}",
		_webServerPort
	);
	_maxProgressCallFailures = JSONUtils::asInt(api["binary"], "maxProgressCallFailures", 0);
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->maxProgressCallFailures: {}",
		_maxProgressCallFailures
	);
	_progressURI = JSONUtils::asString(api["binary"], "progressURI", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->binary->progressURI: {}",
		_progressURI
	);

	_defaultTTLInSeconds = JSONUtils::asInt(api["delivery"], "defaultTTLInSeconds", 60);
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->defaultTTLInSeconds: {}",
		_defaultTTLInSeconds
	);

	_defaultMaxRetries = JSONUtils::asInt(api["delivery"], "defaultMaxRetries", 60);
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->defaultMaxRetries: {}",
		_defaultMaxRetries
	);

	_defaultRedirect = JSONUtils::asBool(api["delivery"], "defaultRedirect", true);
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->defaultRedirect: {}",
		_defaultRedirect
	);

	_deliveryProtocol = JSONUtils::asString(api["delivery"], "deliveryProtocol", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->deliveryProtocol: {}",
		_deliveryProtocol
	);
	_deliveryHost_authorizationThroughParameter = JSONUtils::asString(api["delivery"], "deliveryHost_authorizationThroughParameter", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->deliveryHost_authorizationThroughParameter: {}",
		_deliveryHost_authorizationThroughParameter
	);
	_deliveryHost_authorizationThroughPath = JSONUtils::asString(api["delivery"], "deliveryHost_authorizationThroughPath", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->delivery->deliveryHost_authorizationThroughPath: {}",
		_deliveryHost_authorizationThroughPath
	);

	_ldapEnabled = JSONUtils::asBool(api["activeDirectory"], "enabled", false);
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->enabled: {}",
		_ldapEnabled
	);
	_ldapURL = JSONUtils::asString(api["activeDirectory"], "ldapURL", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->ldapURL: {}",
		_ldapURL
	);
	_ldapCertificatePathName = JSONUtils::asString(api["activeDirectory"], "certificatePathName", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->certificatePathName: {}",
		_ldapCertificatePathName
	);
	_ldapManagerUserName = JSONUtils::asString(api["activeDirectory"], "managerUserName", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->managerUserName: {}",
		_ldapManagerUserName
	);
	_ldapManagerPassword = JSONUtils::asString(api["activeDirectory"], "managerPassword", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->managerPassword: {}",
		_ldapManagerPassword
	);
	_ldapBaseDn = JSONUtils::asString(api["activeDirectory"], "baseDn", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->baseDn: {}",
		_ldapBaseDn
	);
	_ldapDefaultWorkspaceKeys = JSONUtils::asString(api["activeDirectory"], "defaultWorkspaceKeys", "");
	SPDLOG_INFO(
		"Configuration item"
		", api->activeDirectory->defaultWorkspaceKeys: {}",
		_ldapDefaultWorkspaceKeys
	);

	_registerUserEnabled = JSONUtils::asBool(api, "registerUserEnabled", false);
	SPDLOG_INFO(
		"Configuration item"
		", api->registerUserEnabled: {}",
		_registerUserEnabled
	);

	/*
	_ffmpegEncoderProtocol = _configuration["ffmpeg"].get("encoderProtocol", "").asString();
	SPDLOG_INFO(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->encoderProtocol: " + _ffmpegEncoderProtocol
	);
	_ffmpegEncoderPort = JSONUtils::asInt(_configuration["ffmpeg"], "encoderPort", 0);
	SPDLOG_INFO(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->encoderPort: " + to_string(_ffmpegEncoderPort)
	);
	*/
	_ffmpegEncoderUser = JSONUtils::asString(_configurationRoot["ffmpeg"], "encoderUser", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderUser: {}",
		_ffmpegEncoderUser
	);
	_ffmpegEncoderPassword = JSONUtils::asString(_configurationRoot["ffmpeg"], "encoderPassword", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderPassword: {}",
		"..."
	);
	_ffmpegEncoderTimeoutInSeconds = JSONUtils::asInt(_configurationRoot["ffmpeg"], "encoderTimeoutInSeconds", 120);
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderTimeoutInSeconds: {}",
		_ffmpegEncoderTimeoutInSeconds
	);
	_ffmpegEncoderKillEncodingURI = JSONUtils::asString(_configurationRoot["ffmpeg"], "encoderKillEncodingURI", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderKillEncodingURI: {}",
		_ffmpegEncoderKillEncodingURI
	);
	_ffmpegEncoderChangeLiveProxyPlaylistURI = JSONUtils::asString(_configurationRoot["ffmpeg"], "encoderChangeLiveProxyPlaylistURI", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderChangeLiveProxyPlaylistURI: {}",
		_ffmpegEncoderChangeLiveProxyPlaylistURI
	);
	_ffmpegEncoderChangeLiveProxyOverlayTextURI = JSONUtils::asString(_configurationRoot["ffmpeg"], "encoderChangeLiveProxyOverlayTextURI", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderChangeLiveProxyOverlayTextURI: {}",
		_ffmpegEncoderChangeLiveProxyOverlayTextURI
	);

	_intervalInSecondsToCheckEncodingFinished = JSONUtils::asInt(_configurationRoot["encoding"], "intervalInSecondsToCheckEncodingFinished", 0);
	SPDLOG_INFO(
		"Configuration item"
		", encoding->intervalInSecondsToCheckEncodingFinished: {}",
		_intervalInSecondsToCheckEncodingFinished
	);

	_maxSecondsToWaitAPIIngestionLock = JSONUtils::asInt(_configurationRoot["mms"]["locks"], "maxSecondsToWaitAPIIngestionLock", 0);
	SPDLOG_INFO(
		"Configuration item"
		", mms->locks->maxSecondsToWaitAPIIngestionLock: {}",
		_maxSecondsToWaitAPIIngestionLock
	);

	_keyPairId = JSONUtils::asString(_configurationRoot["aws"], "keyPairId", "");
	SPDLOG_INFO(
		"Configuration item"
		", aws->keyPairId: {}",
		_keyPairId
	);
	_privateKeyPEMPathName = JSONUtils::asString(_configurationRoot["aws"], "privateKeyPEMPathName", "");
	SPDLOG_INFO(
		"Configuration item"
		", aws->privateKeyPEMPathName: {}",
		_privateKeyPEMPathName
	);
	/*
	_vodCloudFrontHostNamesRoot = _configurationRoot["aws"]["vodCloudFrontHostNames"];
	SPDLOG_INFO(
		"Configuration item"
		", aws->vodCloudFrontHostNames: {}",
		"..."
	);
	*/

	_emailProviderURL = JSONUtils::asString(_configurationRoot["EmailNotification"], "providerURL", "");
	SPDLOG_INFO(
		"Configuration item"
		", EmailNotification->providerURL: {}",
		_emailProviderURL
	);
	_emailUserName = JSONUtils::asString(_configurationRoot["EmailNotification"], "userName", "");
	SPDLOG_INFO(
		"Configuration item"
		", EmailNotification->userName: {}",
		_emailUserName
	);
	{
		string encryptedPassword = JSONUtils::asString(_configurationRoot["EmailNotification"], "password", "");
		_emailPassword = Encrypt::opensslDecrypt(encryptedPassword);
		SPDLOG_INFO(
			"Configuration item"
			", EmailNotification->password: {}",
			encryptedPassword
			// + ", _emailPassword: " + _emailPassword
		);
	}
	_emailCcsCommaSeparated = JSONUtils::asString(_configurationRoot["EmailNotification"], "cc", "");
	SPDLOG_INFO(
		"Configuration item"
		", EmailNotification->cc: {}",
		_emailCcsCommaSeparated
	);

	_guiProtocol = JSONUtils::asString(_configurationRoot["mms"], "guiProtocol", "");
	SPDLOG_INFO(
		"Configuration item"
		", mms->guiProtocol: {}",
		_guiProtocol
	);
	_guiHostname = JSONUtils::asString(_configurationRoot["mms"], "guiHostname", "");
	SPDLOG_INFO(
		"Configuration item"
		", mms->guiHostname: {}",
		_guiHostname
	);
	_guiPort = JSONUtils::asInt(_configurationRoot["mms"], "guiPort", 0);
	SPDLOG_INFO(
		"Configuration item"
		", mms->guiPort: {}",
		_guiPort
	);

	_waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt(_configurationRoot["storage"], "waitingNFSSync_maxMillisecondsToWait", 60000);
	SPDLOG_INFO(
		"Configuration item"
		", storage->_waitingNFSSync_maxMillisecondsToWait: {}",
		_waitingNFSSync_maxMillisecondsToWait
	);
	_waitingNFSSync_milliSecondsWaitingBetweenChecks =
		JSONUtils::asInt(_configurationRoot["storage"], "waitingNFSSync_milliSecondsWaitingBetweenChecks", 100);
	SPDLOG_INFO(
		"Configuration item"
		", storage->waitingNFSSync_milliSecondsWaitingBetweenChecks: {}",
		_waitingNFSSync_milliSecondsWaitingBetweenChecks
	);

	_fileUploadProgressData = fileUploadProgressData;
	_fileUploadProgressThreadShutdown = false;
}

API::~API() = default;

void API::manageRequestAndResponse(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestURI, string requestMethod,
	unordered_map<string, string> queryParameters, bool authorizationPresent, string userName, string password, unsigned long contentLength,
	string requestBody, unordered_map<string, string> &requestDetails
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

	if (authorizationPresent)
	{
		tie(userKey, workspace, admin, createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization, shareWorkspace, editMedia,
			editConfiguration, killEncoding, cancelIngestionJob_, editEncodersPool, applicationRecorder) = _userKeyWorkspaceAndFlags;

		SPDLOG_INFO(
			"Received manageRequestAndResponse"
			", requestURI: {}"
			", requestMethod: {}"
			", contentLength: {}"
			", userKey: {}"
			", workspace->_name: {}"
			", requestBody: {}"
			", admin: {}"
			", createRemoveWorkspace: {}"
			", ingestWorkflow: {}"
			", createProfiles: {}"
			", deliveryAuthorization: {}"
			", shareWorkspace: {}"
			", editMedia: {}"
			", editConfiguration: {}"
			", killEncoding: {}"
			", cancelIngestionJob: {}"
			", editEncodersPool: {}"
			", applicationRecorder: {}",
			requestURI, requestMethod, contentLength, userKey, workspace->_name, requestBody, admin, createRemoveWorkspace, ingestWorkflow,
			createProfiles, deliveryAuthorization, shareWorkspace, editMedia, editConfiguration, killEncoding, cancelIngestionJob_, editEncodersPool,
			applicationRecorder
		);
	}

	auto methodIt = queryParameters.find("method");
	if (methodIt == queryParameters.end())
	{
		string errorMessage = fmt::format("The 'method' parameter is not found");
		SPDLOG_ERROR(errorMessage);

		sendError(request, 400, errorMessage);

		throw runtime_error(errorMessage);
	}
	string method = methodIt->second;

	string version;
	auto versionIt = queryParameters.find("version");
	if (versionIt != queryParameters.end())
		version = versionIt->second;

	if (!authorizationPresent)
	{
		SPDLOG_INFO(
			"Received manageRequestAndResponse"
			", requestURI: {}"
			", requestMethod: {}"
			", contentLength: {}"
			", method: {}"
			// next is to avoid to log the password
			", requestBody: {}",
			requestURI, requestMethod, contentLength, method, (method == "login" ? "..." : requestBody)
		);
	}

	if (method == "status")
	{
		try
		{
			json statusRoot;

			statusRoot["status"] = "API server up and running";
			statusRoot["version-api"] = version;

			string sJson = JSONUtils::toString(statusRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, sJson);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"status failed"
				", requestBody: {}"
				", e.what(): {}",
				requestBody, e.what()
			);

			string errorMessage = string("Internal server error");
			SPDLOG_ERROR(errorMessage);

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
		if (binaryVirtualHostNameIt != queryParameters.end() && binaryListenHostIt != queryParameters.end() && progressIdIt != requestDetails.end() &&
			originalURIIt != requestDetails.end())
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
					requestData._contentRangeStart = -1;
					requestData._contentRangeEnd = -1;
					requestData._contentRangeSize = -1;
					auto contentRangeIt = requestDetails.find("HTTP_CONTENT_RANGE");
					if (contentRangeIt != requestDetails.end())
					{
						string contentRange = contentRangeIt->second;
						try
						{
							parseContentRange(
								contentRange, requestData._contentRangeStart, requestData._contentRangeEnd, requestData._contentRangeSize
							);

							requestData._contentRangePresent = true;
						}
						catch (exception &e)
						{
							string errorMessage = fmt::format(
								"Content-Range is not well done. Expected format: 'Content-Range: bytes <start>-<end>/<size>'"
								", contentRange: {}",
								contentRange
							);
							SPDLOG_ERROR(errorMessage);

							sendError(request, 500, errorMessage);

							throw runtime_error(errorMessage);
						}
					}

					SPDLOG_INFO(
						"Content-Range details"
						", contentRangePresent: {}"
						", contentRangeStart: {}"
						", contentRangeEnd: {}"
						", contentRangeSize: {}",
						requestData._contentRangePresent, requestData._contentRangeStart, requestData._contentRangeEnd, requestData._contentRangeSize
					);

					lock_guard<mutex> locker(_fileUploadProgressData->_mutex);

					_fileUploadProgressData->_filesUploadProgressToBeMonitored.push_back(requestData);
					SPDLOG_INFO(
						"Added upload file progress to be monitored"
						", _progressId: {}"
						", _binaryVirtualHostName: {}"
						", _binaryListenHost: {}",
						requestData._progressId, requestData._binaryVirtualHostName, requestData._binaryListenHost
					);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"ProgressId not found"
						", progressIdIt->second: {}",
						progressIdIt->second
					);
				}
			}
		}

		string responseBody;
		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
	else if (method == "deliveryAuthorizationThroughParameter")
	{
		// retrieve the HTTP_X_ORIGINAL_METHOD to retrieve the token to be checked (set in the nginx server configuration)
		try
		{
			auto tokenIt = requestDetails.find("HTTP_X_ORIGINAL_METHOD");
			auto originalURIIt = requestDetails.find("HTTP_X_ORIGINAL_URI");
			if (tokenIt == requestDetails.end() || originalURIIt == requestDetails.end())
			{
				string errorMessage = fmt::format(
					"deliveryAuthorization, not authorized"
					", token: {}"
					", URI: {}",
					(tokenIt != requestDetails.end() ? tokenIt->second : "null"),
					(originalURIIt != requestDetails.end() ? originalURIIt->second : "null")
				);
				SPDLOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}

			string contentURI = originalURIIt->second;
			size_t endOfURIIndex = contentURI.find_last_of("?");
			if (endOfURIIndex == string::npos)
			{
				string errorMessage = fmt::format(
					"Wrong URI format"
					", contentURI: {}",
					contentURI
				);
				SPDLOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
			contentURI = contentURI.substr(0, endOfURIIndex);

			string tokenParameter = tokenIt->second;

			SPDLOG_INFO(
				"Calling checkDeliveryAuthorizationThroughParameter"
				", contentURI: {}"
				", tokenParameter: {}",
				contentURI, tokenParameter
			);

			_mmsDeliveryAuthorization->checkDeliveryAuthorizationThroughParameter(contentURI, tokenParameter);

			string responseBody;
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("Not authorized");
			SPDLOG_WARN(errorMessage);

			string responseBody;
			sendError(request, 403, errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("Not authorized: exception managing token");
			SPDLOG_WARN(errorMessage);

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
				string errorMessage = fmt::format(
					"deliveryAuthorization, not authorized"
					", URI: {}",
					(originalURIIt != requestDetails.end() ? originalURIIt->second : "null")
				);
				SPDLOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
			string contentURI = originalURIIt->second;

			SPDLOG_INFO(
				"deliveryAuthorizationThroughPath. Calling checkDeliveryAuthorizationThroughPath"
				", contentURI: {}",
				contentURI
			);

			_mmsDeliveryAuthorization->checkDeliveryAuthorizationThroughPath(contentURI);

			string responseBody;
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("Not authorized");
			SPDLOG_WARN(errorMessage);

			string responseBody;
			sendError(request, 403, errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("Not authorized: exception managing token");
			SPDLOG_WARN(errorMessage);

			sendError(request, 500, errorMessage);
		}
	}
	else if (method == "manageHTTPStreamingManifest_authorizationThroughParameter")
	{
		try
		{
			if (_noFileSystemAccess)
			{
				string errorMessage = fmt::format(
					"no rights to execute this method"
					", _noFileSystemAccess: {}",
					_noFileSystemAccess
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			auto tokenIt = queryParameters.find("token");
			if (tokenIt == queryParameters.end())
			{
				string errorMessage = string("Not authorized: token parameter not present");
				SPDLOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}

			// we could have:
			//		- master manifest, token parameter: <token>--- (es: token=9163 oppure ic_vOSatb6TWp4ania5kaQ%3D%3D,1717958161)
			//			es: /MMS_0000/1/001/472/152/8063642_2/8063642_1653439.m3u8?token=9163
			//			es: /MMS_0000/1/001/470/566/8055007_2/8055007_1652158.m3u8?token=ic_vOSatb6TWp4ania5kaQ%3D%3D,1717958161
			//		- secondary manifest (that has to be treated as a .ts delivery), token parameter:
			//			<encryption of 'manifestLine+++token'>---<cookie: encription of 'token'>
			//			es:
			/// MMS_0000/1/001/472/152/8063642_2/360p/8063642_1653439.m3u8?token=Nw2npoRhfMLZC-GiRuZHpI~jGKBRA-NE-OARj~o68En4XFUriOSuXqexke21OTVd
			bool secondaryManifest;
			string tokenComingFromURL;

			bool isNumber = StringUtils::isNumber(tokenIt->second);
			if (isNumber || tokenIt->second.find(",") != string::npos)
			{
				secondaryManifest = false;
				// tokenComingFromURL = stoll(tokenIt->second);
				tokenComingFromURL = tokenIt->second;
			}
			else
			{
				secondaryManifest = true;
				// tokenComingFromURL will be initialized in the next statement
			}
			SPDLOG_INFO(
				"manageHTTPStreamingManifest"
				", analizing the token {}"
				", isNumber: {}"
				", tokenIt->second: {}"
				", secondaryManifest: {}",
				tokenIt->second, isNumber, tokenIt->second, secondaryManifest
			);

			string contentURI;
			{
				size_t endOfURIIndex = requestURI.find_last_of("?");
				if (endOfURIIndex == string::npos)
				{
					string errorMessage = fmt::format(
						"Wrong URI format"
						", requestURI: {}",
						requestURI
					);
					SPDLOG_INFO(errorMessage);

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
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				string cookie = cookieIt->second;

				string token = tokenIt->second;

				tokenComingFromURL = _mmsDeliveryAuthorization->checkDeliveryAuthorizationOfAManifest(secondaryManifest, token, cookie, contentURI);

				/*
				string tokenParameter = fmt::format("{}---{}", tokenIt->second, cookie);
				SPDLOG_INFO(
					"Calling checkDeliveryAuthorizationThroughParameter"
					", contentURI: {}"
					", tokenParameter: {}",
					contentURI, tokenParameter
				);
				tokenComingFromURL = _mmsDeliveryAuthorization->checkDeliveryAuthorizationThroughParameter(contentURI, tokenParameter);
				*/
			}
			else
			{
				// cookie parameter is added inside catramms.nginx
				string mmsInfoCookie;
				auto cookieIt = queryParameters.find("cookie");
				if (cookieIt != queryParameters.end())
					mmsInfoCookie = cookieIt->second;

				tokenComingFromURL = _mmsDeliveryAuthorization->checkDeliveryAuthorizationOfAManifest(
					secondaryManifest, tokenComingFromURL, mmsInfoCookie, contentURI
				);

				/*
				SPDLOG_INFO(
					"manageHTTPStreamingManifest"
					", tokenComingFromURL: {}"
					", mmsInfoCookie: {}",
					tokenComingFromURL, mmsInfoCookie
				);

				if (mmsInfoCookie == "")
				{
					if (StringUtils::isNumber(tokenComingFromURL)) // MMS_URLWithTokenAsParam_DB
					{
						if (!_mmsEngineDBFacade->checkDeliveryAuthorization(stoll(tokenComingFromURL), contentURI))
						{
							string errorMessage = fmt::format(
								"Not authorized: token invalid"
								", contentURI: {}"
								", tokenComingFromURL: {}",
								contentURI, tokenComingFromURL
							);
							SPDLOG_INFO(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
					else
					{
						{
							// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
							//	That  because if we have really a + char (%2B into the string), and we do the replace
							//	after unescape, this char will be changed to space and we do not want it
							string plus = "\\+";
							string plusDecoded = " ";
							string firstDecoding = regex_replace(tokenComingFromURL, regex(plus), plusDecoded);

							tokenComingFromURL = unescape(firstDecoding);
						}
						_mmsDeliveryAuthorization->checkSignedMMSPath(tokenComingFromURL, contentURI);
					}

					SPDLOG_INFO(
						"token authorized"
						", tokenComingFromURL: {}",
						tokenComingFromURL
					);
				}
				else
				{
					string sTokenComingFromCookie = Encrypt::opensslDecrypt(mmsInfoCookie);
					// int64_t tokenComingFromCookie = stoll(sTokenComingFromCookie);

					if (sTokenComingFromCookie != tokenComingFromURL)
					{
						string errorMessage = fmt::format(
							"cookie invalid, let's check the token"
							", sTokenComingFromCookie: {}"
							", tokenComingFromURL: {}",
							sTokenComingFromCookie, tokenComingFromURL
						);
						SPDLOG_INFO(errorMessage);

						if (StringUtils::isNumber(tokenComingFromURL)) // MMS_URLWithTokenAsParam_DB
						{
							if (!_mmsEngineDBFacade->checkDeliveryAuthorization(stoll(tokenComingFromURL), contentURI))
							{
								string errorMessage = fmt::format(
									"Not authorized: token invalid"
									", contentURI: {}"
									", tokenComingFromURL: {}",
									contentURI, tokenComingFromURL
								);
								SPDLOG_INFO(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
						else
						{
							_mmsDeliveryAuthorization->checkSignedMMSPath(tokenComingFromURL, contentURI);
						}

						SPDLOG_INFO(
							"token authorized"
							", tokenComingFromURL: {}",
							tokenComingFromURL
						);
					}
					else
					{
						SPDLOG_INFO(
							"cookie authorized"
							", mmsInfoCookie: {}",
							mmsInfoCookie
						);
					}
				}
				*/
			}

			// manifest authorized

			{
				string contentType;

				string m3u8Extension(".m3u8");
				if (contentURI.ends_with(m3u8Extension))
					contentType = "Content-type: application/x-mpegURL";
				else // dash
					contentType = "Content-type: application/dash+xml";
				string cookieName = "mmsInfo";

				string responseBody;
				{
					fs::path manifestPathFileName = _mmsStorage->getMMSRootRepository() / contentURI.substr(1);

					SPDLOG_INFO(
						"Reading manifest file"
						", manifestPathFileName: {}",
						manifestPathFileName.string()
					);

					if (!fs::exists(manifestPathFileName))
					{
						string errorMessage = fmt::format(
							"manifest file not existing"
							", manifestPathFileName: {}",
							manifestPathFileName.string()
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					if (contentURI.ends_with(m3u8Extension))
					{
						std::ifstream manifestFile;

						manifestFile.open(manifestPathFileName.string(), ios::in);
						if (!manifestFile.is_open())
						{
							string errorMessage = fmt::format(
								"Not authorized: manifest file not opened"
								", manifestPathFileName: {}",
								manifestPathFileName.string()
							);
							SPDLOG_INFO(errorMessage);

							throw runtime_error(errorMessage);
						}

						string manifestLine;
						string tsExtension = ".ts";
						string m3u8Extension = ".m3u8";
						string m3u8ExtXMedia = "#EXT-X-MEDIA";
						string endLine = "\n";
						while (getline(manifestFile, manifestLine))
						{
							if (manifestLine[0] != '#' && manifestLine.ends_with(tsExtension))
							{
								/*
								SPDLOG_INFO(__FILEREF__ + "Creation token parameter for ts"
									+ ", manifestLine: " + manifestLine
									+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
								);
								*/
								string auth = Encrypt::opensslEncrypt(manifestLine + "+++" + tokenComingFromURL);
								responseBody += (manifestLine + "?token=" + auth + endLine);
							}
							else if (manifestLine[0] != '#' && manifestLine.ends_with(m3u8Extension))
							{
								// scenario where we have several .m3u8 manifest files
								/*
								SPDLOG_INFO(__FILEREF__ + "Creation token parameter for m3u8"
									+ ", manifestLine: " + manifestLine
									+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
								);
								*/
								string auth = Encrypt::opensslEncrypt(manifestLine + "+++" + tokenComingFromURL);
								responseBody += (manifestLine + "?token=" + auth + endLine);
							}
							else if (manifestLine.starts_with(m3u8ExtXMedia))
							{
								// #EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",LANGUAGE="eng",NAME="eng",AUTOSELECT=YES,
								// DEFAULT=YES,URI="eng/1247999_384641.m3u8"
								string temp = "URI=\"";
								size_t uriStartIndex = manifestLine.find(temp);
								if (uriStartIndex != string::npos)
								{
									uriStartIndex += temp.size();
									size_t uriEndIndex = uriStartIndex;
									while (manifestLine[uriEndIndex] != '\"' && uriEndIndex < manifestLine.size())
										uriEndIndex++;
									if (manifestLine[uriEndIndex] == '\"')
									{
										string uri = manifestLine.substr(uriStartIndex, uriEndIndex - uriStartIndex);
										/*
										SPDLOG_INFO(__FILEREF__ + "Creation token parameter for m3u8"
											+ ", uri: " + uri
											+ ", tokenComingFromURL: " + to_string(tokenComingFromURL)
										);
										*/
										string auth = Encrypt::opensslEncrypt(uri + "+++" + tokenComingFromURL);
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
					else // dash
					{
#if defined(LIBXML_TREE_ENABLED) && defined(LIBXML_OUTPUT_ENABLED) && defined(LIBXML_XPATH_ENABLED) && defined(LIBXML_SAX1_ENABLED)
						SPDLOG_INFO("libxml define OK");
#else
						SPDLOG_INFO("libxml define KO");
#endif

						/*
						<?xml version="1.0" encoding="utf-8"?>
						<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
								xmlns="urn:mpeg:dash:schema:mpd:2011"
								xmlns:xlink="http://www.w3.org/1999/xlink"
								xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011
						http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd"
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
												<Representation id="0" mimeType="video/mp4" codecs="avc1.640029" bandwidth="1494920" width="1024"
						height="576" frameRate="25/1"> <SegmentTemplate timescale="12800" initialization="init-stream$RepresentationID$.m4s"
						media="chunk-stream$RepresentationID$-$Number%05d$.m4s" startNumber="6373"> <SegmentTimeline> <S t="815616000" d="128000"
						r="5" />
																</SegmentTimeline>
														</SegmentTemplate>
												</Representation>
										</AdaptationSet>
										<AdaptationSet id="1" contentType="audio" segmentAlignment="true" bitstreamSwitching="true">
												<Representation id="1" mimeType="audio/mp4" codecs="mp4a.40.5" bandwidth="95545"
						audioSamplingRate="48000"> <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011"
						value="2" /> <SegmentTemplate timescale="48000" initialization="init-stream$RepresentationID$.m4s"
						media="chunk-stream$RepresentationID$-$Number%05d$.m4s" startNumber="6373"> <SegmentTimeline> <S t="3058557246" d="479232" />
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
						xmlDocPtr doc = xmlParseFile(manifestPathFileName.string().c_str());
						if (doc == nullptr)
						{
							string errorMessage = fmt::format(
								"xmlParseFile failed"
								", manifestPathFileName: {}",
								manifestPathFileName.string()
							);
							SPDLOG_INFO(errorMessage);

							throw runtime_error(errorMessage);
						}

						// xmlNode* rootElement = xmlDocGetRootElement(doc);

						/* Create xpath evaluation context */
						xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
						if (xpathCtx == nullptr)
						{
							xmlFreeDoc(doc);

							string errorMessage = fmt::format(
								"xmlXPathNewContext failed"
								", manifestPathFileName: {}",
								manifestPathFileName.string()
							);
							SPDLOG_INFO(errorMessage);

							throw runtime_error(errorMessage);
						}

						if (xmlXPathRegisterNs(xpathCtx, BAD_CAST "xmlns", BAD_CAST "urn:mpeg:dash:schema:mpd:2011") != 0)
						{
							xmlXPathFreeContext(xpathCtx);
							xmlFreeDoc(doc);

							string errorMessage = fmt::format(
								"xmlXPathRegisterNs xmlns:xsi"
								", manifestPathFileName: {}",
								manifestPathFileName.string()
							);
							SPDLOG_INFO(errorMessage);

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
								+ ", manifestPathFileName: " + manifestPathFileName.string()
								;
							SPDLOG_INFO(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}
						if(xmlXPathRegisterNs(xpathCtx,
							BAD_CAST "xsi:schemaLocation",
							BAD_CAST "http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd") != 0)
						{
							xmlXPathFreeContext(xpathCtx);
							xmlFreeDoc(doc);

							string errorMessage = string("xmlXPathRegisterNs xsi:schemaLocation")
								+ ", manifestPathFileName: " + manifestPathFileName.string()
								;
							SPDLOG_INFO(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}
						*/

						// Evaluate xpath expression
						const char *xpathExpr = "//xmlns:Period/xmlns:AdaptationSet/xmlns:Representation/xmlns:SegmentTemplate";
						xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(BAD_CAST xpathExpr, xpathCtx);
						if (xpathObj == nullptr)
						{
							xmlXPathFreeContext(xpathCtx);
							xmlFreeDoc(doc);

							string errorMessage = fmt::format(
								"xmlXPathEvalExpression failed"
								", manifestPathFileName: {}",
								manifestPathFileName.string()
							);
							SPDLOG_INFO(errorMessage);

							throw runtime_error(errorMessage);
						}

						xmlNodeSetPtr nodes = xpathObj->nodesetval;
						SPDLOG_INFO(
							"processing mpd manifest file"
							", manifestPathFileName: {}"
							", nodesNumber: {}",
							manifestPathFileName.string(), nodes->nodeNr
						);
						for (int nodeIndex = 0; nodeIndex < nodes->nodeNr; nodeIndex++)
						{
							if (nodes->nodeTab[nodeIndex] == nullptr)
							{
								xmlXPathFreeContext(xpathCtx);
								xmlFreeDoc(doc);

								string errorMessage = fmt::format(
									"nodes->nodeTab[nodeIndex] is null"
									", manifestPathFileName: {}"
									", nodeIndex: {}",
									manifestPathFileName.string(), nodeIndex
								);
								SPDLOG_INFO(errorMessage);

								throw runtime_error(errorMessage);
							}

							const char *mediaAttributeName = "media";
							const char *initializationAttributeName = "initialization";
							xmlChar *mediaValue = xmlGetProp(nodes->nodeTab[nodeIndex], BAD_CAST mediaAttributeName);
							xmlChar *initializationValue = xmlGetProp(nodes->nodeTab[nodeIndex], BAD_CAST initializationAttributeName);
							if (mediaValue == (xmlChar *)nullptr || initializationValue == (xmlChar *)nullptr)
							{
								xmlXPathFreeContext(xpathCtx);
								xmlFreeDoc(doc);

								string errorMessage = fmt::format(
									"xmlGetProp failed"
									", manifestPathFileName: {}",
									manifestPathFileName.string()
								);
								SPDLOG_INFO(errorMessage);

								throw runtime_error(errorMessage);
							}

							string auth = Encrypt::opensslEncrypt(string((char *)mediaValue) + "+++" + tokenComingFromURL);
							string newMediaAttributeValue = string((char *)mediaValue) + "?token=" + auth;
							// xmlAttrPtr
							xmlSetProp(nodes->nodeTab[nodeIndex], BAD_CAST mediaAttributeName, BAD_CAST newMediaAttributeValue.c_str());

							string newInitializationAttributeValue = string((char *)initializationValue) + "?token=" + auth;
							// xmlAttrPtr
							xmlSetProp(
								nodes->nodeTab[nodeIndex], BAD_CAST initializationAttributeName, BAD_CAST newInitializationAttributeValue.c_str()
							);

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
							SPDLOG_INFO(
								"dumping mpd manifest file"
								", manifestPathFileName: {}"
								", buffersize: {}",
								manifestPathFileName.string(), buffersize
							);

							responseBody = (char *)xmlbuff;

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

				string cookieValue = Encrypt::opensslEncrypt(tokenComingFromURL);
				string cookiePath;
				{
					size_t cookiePathIndex = contentURI.find_last_of("/");
					if (cookiePathIndex == string::npos)
					{
						string errorMessage = fmt::format(
							"Wrong URI format"
							", contentURI: {}",
							contentURI
						);
						SPDLOG_INFO(errorMessage);

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
					sendSuccess(
						sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody, contentType, "",
						"", "", enableCorsGETHeader, originHeader
					);
				else
					sendSuccess(
						sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody, contentType,
						cookieName, cookieValue, cookiePath, enableCorsGETHeader, originHeader
					);
			}
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("Not authorized");
			SPDLOG_WARN(errorMessage);

			string responseBody;
			sendError(request, 403, errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("Not authorized: exception managing token");
			SPDLOG_WARN(errorMessage);

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
		updateUser(sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, requestBody, admin);
	}
	else if (method == "createTokenToResetPassword")
	{
		createTokenToResetPassword(sThreadId, requestIdentifier, responseBodyCompressed, request, queryParameters);
	}
	else if (method == "resetPassword")
	{
		resetPassword(sThreadId, requestIdentifier, responseBodyCompressed, request, requestBody);
	}
	else if (method == "updateWorkspace")
	{
		updateWorkspace(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, userKey, requestBody);
	}
	else if (method == "setWorkspaceAsDefault")
	{
		setWorkspaceAsDefault(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, userKey, queryParameters, requestBody);
	}
	else if (method == "createWorkspace")
	{
		if (!admin && !createRemoveWorkspace)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", createRemoveWorkspace: {}",
				createRemoveWorkspace
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		createWorkspace(sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, queryParameters, requestBody, admin);
	}
	else if (method == "deleteWorkspace")
	{
		if (!admin && !createRemoveWorkspace)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", createRemoveWorkspace: {}",
				createRemoveWorkspace
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		deleteWorkspace(sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, workspace);
	}
	else if (method == "unshareWorkspace")
	{
		if (!admin && !shareWorkspace)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", shareWorkspace: {}",
				shareWorkspace
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		unshareWorkspace(sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, workspace);
	}
	else if (method == "workspaceUsage")
	{
		workspaceUsage(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace);
	}
	else if (method == "shareWorkspace")
	{
		if (!admin && !shareWorkspace)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", shareWorkspace: {}",
				shareWorkspace
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		shareWorkspace_(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "workspaceList")
	{
		workspaceList(sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, workspace, queryParameters, admin);
	}
	else if (method == "addInvoice")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addInvoice(sThreadId, requestIdentifier, responseBodyCompressed, request, queryParameters, requestBody);
	}
	else if (method == "invoiceList")
	{
		invoiceList(sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, queryParameters, admin);
	}
	else if (method == "confirmRegistration")
	{
		confirmRegistration(sThreadId, requestIdentifier, responseBodyCompressed, request, queryParameters);
	}
	else if (method == "addEncoder")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addEncoder(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, requestBody);
	}
	else if (method == "removeEncoder")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeEncoder(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "modifyEncoder")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyEncoder(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "encoderList")
	{
		encoderList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, admin, queryParameters);
	}
	else if (method == "encodersPoolList")
	{
		encodersPoolList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, admin, queryParameters);
	}
	else if (method == "addEncodersPool")
	{
		if (!admin && !editEncodersPool)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editEncodersPool: {}",
				editEncodersPool
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addEncodersPool(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, requestBody);
	}
	else if (method == "modifyEncodersPool")
	{
		if (!admin && !editEncodersPool)
		{
			string errorMessage = string(
				"APIKey does not have the permission"
				", editEncodersPool: {}",
				editEncodersPool
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyEncodersPool(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeEncodersPool")
	{
		if (!admin && !editEncodersPool)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editEncodersPool: {}",
				editEncodersPool
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeEncodersPool(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addAssociationWorkspaceEncoder")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addAssociationWorkspaceEncoder(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "removeAssociationWorkspaceEncoder")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeAssociationWorkspaceEncoder(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "createDeliveryAuthorization")
	{
		if (!admin && !deliveryAuthorization)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", deliveryAuthorization: {}",
				deliveryAuthorization
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		string clientIPAddress = getClientIPAddress(requestDetails);

		createDeliveryAuthorization(
			sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, workspace, clientIPAddress, queryParameters
		);
	}
	else if (method == "createBulkOfDeliveryAuthorization")
	{
		if (!admin && !deliveryAuthorization)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", deliveryAuthorization: {}",
				deliveryAuthorization
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		string clientIPAddress = getClientIPAddress(requestDetails);

		createBulkOfDeliveryAuthorization(
			sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, workspace, clientIPAddress, queryParameters, requestBody
		);
	}
	else if (method == "ingestion")
	{
		if (!admin && !ingestWorkflow)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", ingestWorkflow: {}",
				ingestWorkflow
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		ingestion(sThreadId, requestIdentifier, responseBodyCompressed, request, stoll(userName), password, workspace, queryParameters, requestBody);
	}
	else if (method == "ingestionRootsStatus")
	{
		ingestionRootsStatus(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "ingestionRootMetaDataContent")
	{
		ingestionRootMetaDataContent(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "ingestionJobsStatus")
	{
		ingestionJobsStatus(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "cancelIngestionJob")
	{
		if (!admin && !cancelIngestionJob_)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", cancelIngestionJob: {}",
				cancelIngestionJob_
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		cancelIngestionJob(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "updateIngestionJob")
	{
		if (!admin && !editMedia)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editMedia: {}",
				editMedia
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		updateIngestionJob(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, userKey, queryParameters, requestBody, admin);
	}
	else if (method == "ingestionJobSwitchToEncoder")
	{
		if (!admin && !editMedia)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editMedia: {}",
				editMedia
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		ingestionJobSwitchToEncoder(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, userKey, queryParameters, admin);
	}
	else if (method == "encodingJobsStatus")
	{
		encodingJobsStatus(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "encodingJobPriority")
	{
		encodingJobPriority(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "killOrCancelEncodingJob")
	{
		if (!admin && !killEncoding)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", killEncoding: {}",
				killEncoding
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		killOrCancelEncodingJob(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "changeLiveProxyPlaylist")
	{
		changeLiveProxyPlaylist(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "changeLiveProxyOverlayText")
	{
		changeLiveProxyOverlayText(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "mediaItemsList")
	{
		mediaItemsList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody, admin);
	}
	else if (method == "updateMediaItem")
	{
		if (!admin && !editMedia)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editMedia: {}",
				editMedia
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		updateMediaItem(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, userKey, queryParameters, requestBody, admin);
	}
	else if (method == "updatePhysicalPath")
	{
		if (!admin && !editMedia)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editMedia: {}",
				editMedia
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		updatePhysicalPath(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, userKey, queryParameters, requestBody, admin);
	}
	else if (method == "tagsList")
	{
		tagsList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "uploadedBinary")
	{
		uploadedBinary(
			sThreadId, requestIdentifier, responseBodyCompressed, request, requestMethod, queryParameters, workspace, // contentLength,
			requestDetails
		);
	}
	else if (method == "addUpdateEncodingProfilesSet")
	{
		if (!admin && !createProfiles)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", createProfiles: {}",
				createProfiles
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addUpdateEncodingProfilesSet(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "encodingProfilesSetsList")
	{
		encodingProfilesSetsList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addEncodingProfile")
	{
		if (!admin && !createProfiles)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", createProfiles: {}",
				createProfiles
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addEncodingProfile(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeEncodingProfile")
	{
		if (!admin && !createProfiles)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", createProfiles: {}",
				createProfiles
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeEncodingProfile(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "removeEncodingProfilesSet")
	{
		if (!admin && !createProfiles)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", createProfiles: {}",
				createProfiles
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeEncodingProfilesSet(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "encodingProfilesList")
	{
		encodingProfilesList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "workflowsAsLibraryList")
	{
		workflowsAsLibraryList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "workflowAsLibraryContent")
	{
		workflowAsLibraryContent(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "saveWorkflowAsLibrary")
	{
		saveWorkflowAsLibrary(sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, workspace, queryParameters, requestBody, admin);
	}
	else if (method == "removeWorkflowAsLibrary")
	{
		removeWorkflowAsLibrary(sThreadId, requestIdentifier, responseBodyCompressed, request, userKey, workspace, queryParameters, admin);
	}
	else if (method == "mmsSupport")
	{
		mmsSupport(sThreadId, requestIdentifier, responseBodyCompressed, request, stoll(userName), password, workspace, queryParameters, requestBody);
	}
	else if (method == "addYouTubeConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addYouTubeConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyYouTubeConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyYouTubeConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeYouTubeConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeYouTubeConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "youTubeConfList")
	{
		youTubeConfList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addFacebookConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addFacebookConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyFacebookConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyFacebookConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeFacebookConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeFacebookConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "facebookConfList")
	{
		facebookConfList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addTwitchConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addTwitchConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyTwitchConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyTwitchConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeTwitchConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeTwitchConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "twitchConfList")
	{
		twitchConfList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addStream")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addStream(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyStream")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyStream(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeStream")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeStream(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "streamList")
	{
		streamList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "streamFreePushEncoderPort")
	{
		streamFreePushEncoderPort(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addSourceTVStream")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addSourceTVStream(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifySourceTVStream")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifySourceTVStream(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeSourceTVStream")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeSourceTVStream(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "sourceTVStreamList")
	{
		sourceTVStreamList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addAWSChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addAWSChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyAWSChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyAWSChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeAWSChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeAWSChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "awsChannelConfList")
	{
		awsChannelConfList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addCDN77ChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addCDN77ChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyCDN77ChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyCDN77ChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeCDN77ChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeCDN77ChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "cdn77ChannelConfList")
	{
		cdn77ChannelConfList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addRTMPChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addRTMPChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyRTMPChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyRTMPChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeRTMPChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeRTMPChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "rtmpChannelConfList")
	{
		rtmpChannelConfList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addHLSChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addHLSChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyHLSChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyHLSChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeHLSChannelConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeHLSChannelConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "hlsChannelConfList")
	{
		hlsChannelConfList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addFTPConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addFTPConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyFTPConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyFTPConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeFTPConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeFTPConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "ftpConfList")
	{
		ftpConfList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace);
	}
	else if (method == "addEMailConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addEMailConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "modifyEMailConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		modifyEMailConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "removeEMailConf")
	{
		if (!admin && !editConfiguration)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", editConfiguration: {}",
				editConfiguration
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		removeEMailConf(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "emailConfList")
	{
		emailConfList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace);
	}
	else if (method == "loginStatisticList")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		loginStatisticList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "addRequestStatistic")
	{
		if (!admin)
		{
			string errorMessage = fmt::format(
				"APIKey does not have the permission"
				", admin: {}",
				admin
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

		addRequestStatistic(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters, requestBody);
	}
	else if (method == "requestStatisticList")
	{
		requestStatisticList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "requestStatisticPerContentList")
	{
		requestStatisticPerContentList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "requestStatisticPerUserList")
	{
		requestStatisticPerUserList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "requestStatisticPerMonthList")
	{
		requestStatisticPerMonthList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "requestStatisticPerDayList")
	{
		requestStatisticPerDayList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	}
	else if (method == "requestStatisticPerHourList")
		requestStatisticPerHourList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	else if (method == "requestStatisticPerCountryList")
		requestStatisticPerCountryList(sThreadId, requestIdentifier, responseBodyCompressed, request, workspace, queryParameters);
	else
	{
		string errorMessage = fmt::format(
			"No API is matched"
			", requestURI: {}"
			", method: {}"
			", requestMethod: {}",
			requestURI, method, requestMethod
		);
		SPDLOG_ERROR(errorMessage);

		sendError(request, 400, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::checkAuthorization(string sThreadId, string userName, string password)
{
	string userKey = userName;
	string apiKey = password;

	try
	{
		_userKeyWorkspaceAndFlags = _mmsEngineDBFacade->checkAPIKey(
			apiKey,
			// 2022-12-18: controllo della apikey, non vedo motivi per mettere true
			false
		);
	}
	catch (APIKeyNotFoundOrExpired &e)
	{
		SPDLOG_INFO(
			"_mmsEngineDBFacade->checkAPIKey failed"
			", _requestIdentifier: {}"
			", threadId: {}"
			", apiKey: {}",
			_requestIdentifier, sThreadId, apiKey
		);

		throw CheckAuthorizationFailed();
	}
	catch (exception &e)
	{
		SPDLOG_INFO(
			"_mmsEngineDBFacade->checkAPIKey failed"
			", _requestIdentifier: {}"
			", threadId: {}"
			", apiKey: {}",
			_requestIdentifier, sThreadId, apiKey
		);

		throw CheckAuthorizationFailed();
	}

	if (get<0>(_userKeyWorkspaceAndFlags) != stoll(userName))
	{
		SPDLOG_INFO(
			"Username of the basic authorization (UserKey) is not the same UserKey the apiKey is referring"
			", _requestIdentifier: {}"
			", threadId: {}"
			", username of basic authorization (userKey): {}"
			", userKey associated to the APIKey: {}"
			", apiKey: {}",
			_requestIdentifier, sThreadId, userKey, get<0>(_userKeyWorkspaceAndFlags), apiKey
		);

		throw CheckAuthorizationFailed();
	}
}

bool API::basicAuthenticationRequired(string requestURI, unordered_map<string, string> queryParameters)
{
	bool basicAuthenticationRequired = true;

	auto methodIt = queryParameters.find("method");
	if (methodIt == queryParameters.end())
	{
		SPDLOG_ERROR("The 'method' parameter is not found");

		return basicAuthenticationRequired;
	}
	string method = methodIt->second;

	if (method == "registerUser" || method == "confirmRegistration" || method == "createTokenToResetPassword" || method == "resetPassword" ||
		method == "login" || method == "manageHTTPStreamingManifest_authorizationThroughParameter" ||
		method == "deliveryAuthorizationThroughParameter" || method == "deliveryAuthorizationThroughPath" ||
		method == "status" // often used as healthy check
	)
		basicAuthenticationRequired = false;

	// This is the authorization asked when the deliveryURL is received by nginx
	// Here the token is checked and it is not needed any basic authorization
	if (requestURI == "/catramms/delivery/authorization")
		basicAuthenticationRequired = false;

	return basicAuthenticationRequired;
}

void API::parseContentRange(string contentRange, long long &contentRangeStart, long long &contentRangeEnd, long long &contentRangeSize)
{
	// Content-Range: bytes 0-99999/100000

	contentRangeStart = -1;
	contentRangeEnd = -1;
	contentRangeSize = -1;

	try
	{
		string prefix("bytes ");
		if (!(contentRange.size() >= prefix.size() && 0 == contentRange.compare(0, prefix.size(), prefix)))
		{
			string errorMessage = fmt::format(
				"Content-Range does not start with 'bytes '"
				", contentRange: {}",
				contentRange
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		int startIndex = prefix.size();
		int endIndex = contentRange.find("-", startIndex);
		if (endIndex == string::npos)
		{
			string errorMessage = fmt::format(
				"Content-Range does not have '-'"
				", contentRange: {}",
				contentRange
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		contentRangeStart = stoll(contentRange.substr(startIndex, endIndex - startIndex));

		endIndex++;
		int sizeIndex = contentRange.find("/", endIndex);
		if (sizeIndex == string::npos)
		{
			string errorMessage = fmt::format(
				"Content-Range does not have '/'"
				", contentRange: {}",
				contentRange
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		contentRangeEnd = stoll(contentRange.substr(endIndex, sizeIndex - endIndex));

		sizeIndex++;
		contentRangeSize = stoll(contentRange.substr(sizeIndex));
	}
	catch (exception &e)
	{
		string errorMessage = fmt::format(
			"Content-Range is not well done. Expected format: 'Content-Range: bytes <start>-<end>/<size>'"
			", contentRange: {}",
			contentRange
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::mmsSupport(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey, string apiKey,
	shared_ptr<Workspace> workspace, unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "mmsSupport";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		string userEmailAddress;
		string subject;
		string text;

		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson(requestBody);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(e.what());

			sendError(request, 400, e.what());

			throw runtime_error(e.what());
		}

		vector<string> mandatoryFields = {"UserEmailAddress", "Subject", "Text"};
		for (string field : mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				string errorMessage = fmt::format(
					"Json field is not present or it is null"
					", Json field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				sendError(request, 400, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		userEmailAddress = JSONUtils::asString(metadataRoot, "UserEmailAddress", "");
		subject = JSONUtils::asString(metadataRoot, "Subject", "");
		text = JSONUtils::asString(metadataRoot, "Text", "");

		try
		{
			vector<string> emailBody;
			emailBody.push_back(string("<p>UserKey: ") + to_string(userKey) + "</p>");
			emailBody.push_back(string("<p>WorkspaceKey: ") + to_string(workspace->_workspaceKey) + "</p>");
			emailBody.push_back(string("<p>APIKey: ") + apiKey + "</p>");
			emailBody.push_back(string("<p></p>"));
			emailBody.push_back(string("<p>From: ") + userEmailAddress + "</p>");
			emailBody.push_back(string("<p></p>"));
			emailBody.push_back(string("<p>") + text + "</p>");

			string tosCommaSeparated = "support@catramms-cloud.com";
			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, _emailPassword
			);
			// EMailSender emailSender(_logger, _configuration);
			// bool useMMSCCToo = true;
			// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);

			string responseBody;
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"{} failed"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = fmt::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"{} failed"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = string("Internal server error");
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		string errorMessage = string("Internal server error");
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::sendError(FCGX_Request &request, int htmlResponseCode, string errorMessage)
{
	json responseBodyRoot;
	responseBodyRoot["status"] = to_string(htmlResponseCode);
	responseBodyRoot["error"] = errorMessage;

	FastCGIAPI::sendError(request, htmlResponseCode, JSONUtils::toString(responseBodyRoot));
}
