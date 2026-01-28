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

#include "API.h"
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include "JsonPath.h"
#include "LdapWrapper.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"
#include <format>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <vector>

using namespace std;
using json = nlohmann::json;

void API::registerUser(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData)
{
	string api = "registerUser";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
	);

	try
	{
		if (!_registerUserEnabled)
		{
			string errorMessage = "registerUser is not enabled";
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string email;
		string password;
		string shareWorkspaceCode;

		int64_t workspaceKey;
		int64_t userKey;
		string confirmationCode;

		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestData.requestBody
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> mandatoryFields = {"email", "password"};
		for (string field : mandatoryFields)
		{
			if (!JSONUtils::isPresent(metadataRoot, field))
			{
				string errorMessage = std::format(
					"Json field is not present or it is null"
					", Json field: {}",
					field
				);
				LOG_ERROR(errorMessage);
				throw runtime_error(errorMessage);
			}
		}

		email = JSONUtils::asString(metadataRoot, "email", "");
		try
		{
			emailFormatCheck(email);
		}
		catch (runtime_error &e)
		{
			string errorMessage = std::format(
				"Wrong email format"
				", email: {}",
				email
			);
			LOG_ERROR(errorMessage);
			throw runtime_error(errorMessage);
		}

		password = JSONUtils::asString(metadataRoot, "password", "");
		shareWorkspaceCode = JSONUtils::asString(metadataRoot, "shareWorkspaceCode", "");

		string name = JSONUtils::asString(metadataRoot, "name", "");
		string country = JSONUtils::asString(metadataRoot, "country", "");
		string timezone = JSONUtils::asString(metadataRoot, "timezone", "CET");

		if (shareWorkspaceCode.empty())
		{
			MMSEngineDBFacade::EncodingPriority encodingPriority;
			MMSEngineDBFacade::EncodingPeriod encodingPeriod;
			int maxIngestionsNumber;
			int maxStorageInMB;

			string workspaceName = JSONUtils::asString(metadataRoot, "workspaceName", "");
			if (workspaceName.empty())
			{
				if (!name.empty())
					workspaceName = name;
				else
					workspaceName = email;
			}

			encodingPriority = _encodingPriorityWorkspaceDefaultValue;

			encodingPeriod = _encodingPeriodWorkspaceDefaultValue;
			maxIngestionsNumber = _maxIngestionsNumberWorkspaceDefaultValue;

			maxStorageInMB = _maxStorageInMBWorkspaceDefaultValue;

			try
			{
				LOG_INFO(
					"Registering User because of Add Workspace"
					", workspaceName: {}"
					", shareWorkspaceCode: {}"
					", email: {}",
					workspaceName, shareWorkspaceCode, email
				);

#ifdef __POSTGRES__
				tuple<int64_t, int64_t, string> workspaceKeyUserKeyAndConfirmationCode = _mmsEngineDBFacade->registerUserAndAddWorkspace(
					name, email, password, country, timezone, workspaceName, "" /* notes */,
					MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,	   // MMSEngineDBFacade::WorkspaceType workspaceType
					"",														   // string deliveryURL,
					encodingPriority,										   //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
					encodingPeriod,											   //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
					maxIngestionsNumber,									   // long maxIngestionsNumber,
					maxStorageInMB,											   // long maxStorageInMB,
					"",														   // string languageCode,
					timezone,												   // by default, timezone del workspace coincide con quello dell'utente
					chrono::system_clock::now() + chrono::hours(24 * 365 * 10) // chrono::system_clock::time_point userExpirationDate
				);
#else
				tuple<int64_t, int64_t, string> workspaceKeyUserKeyAndConfirmationCode = _mmsEngineDBFacade->registerUserAndAddWorkspace(
					name, email, password, country, workspaceName,
					MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,	   // MMSEngineDBFacade::WorkspaceType workspaceType
					"",														   // string deliveryURL,
					encodingPriority,										   //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
					encodingPeriod,											   //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
					maxIngestionsNumber,									   // long maxIngestionsNumber,
					maxStorageInMB,											   // long maxStorageInMB,
					"",														   // string languageCode,
					chrono::system_clock::now() + chrono::hours(24 * 365 * 10) // chrono::system_clock::time_point userExpirationDate
				);
#endif

				workspaceKey = get<0>(workspaceKeyUserKeyAndConfirmationCode);
				userKey = get<1>(workspaceKeyUserKeyAndConfirmationCode);
				confirmationCode = get<2>(workspaceKeyUserKeyAndConfirmationCode);

				LOG_INFO(
					"Registered User and added Workspace"
					", workspaceName: {}"
					", email: {}"
					", userKey: {}"
					", confirmationCode: {}",
					workspaceName, email, userKey, confirmationCode
				);
			}
			catch (exception &e)
			{
				string errorMessage = std::format(
					"API failed"
					", API: {}"
					", requestData.requestBody: {}"
					", e.what(): {}",
					api, requestData.requestBody, e.what()
				);
				LOG_ERROR(errorMessage);
				throw runtime_error(errorMessage);
			}

			// workspace initialization
			{
				try
				{
					LOG_INFO(
						"Associate defaults encoders to the Workspace"
						", workspaceKey: {}"
						", _sharedEncodersPoolLabel: {}",
						workspaceKey, _sharedEncodersPoolLabel
					);

					_mmsEngineDBFacade->addAssociationWorkspaceEncoder(workspaceKey, _sharedEncodersPoolLabel, _sharedEncodersLabel);
				}
				catch (exception &e)
				{
					LOG_ERROR(
						"API failed"
						", API: {}"
						", requestData.requestBody: {}"
						", e.what(): {}",
						api, requestData.requestBody, e.what()
					);

					// string errorMessage = string("Internal server error");
					// LOG_ERROR(errorMessage);

					// 2021-09-30: we do not raise an exception because this association
					// is not critical for the account
					// sendError(request, 500, errorMessage);

					// throw runtime_error(errorMessage);
				}

				try
				{
					LOG_INFO(
						"Add some HLS_Channels to the Workspace"
						", workspaceKey: {}"
						", _defaultSharedHLSChannelsNumber: {}",
						workspaceKey, _defaultSharedHLSChannelsNumber
					);

					for (int hlsChannelIndex = 0; hlsChannelIndex < _defaultSharedHLSChannelsNumber; hlsChannelIndex++)
						_mmsEngineDBFacade->addHLSChannelConf(workspaceKey, to_string(hlsChannelIndex + 1), hlsChannelIndex + 1, -1, -1, "SHARED");
				}
				catch (exception &e)
				{
					LOG_ERROR(
						"API failed"
						", API: {}"
						", requestData.requestBody: {}"
						", e.what(): {}",
						api, requestData.requestBody, e.what()
					);

					// string errorMessage = string("Internal server error");
					// LOG_ERROR(errorMessage);

					// 2021-09-30: we do not raise an exception because this association
					// is not critical for the account
					// sendError(request, 500, errorMessage);

					// throw runtime_error(errorMessage);
				}
			}
		}
		else
		{
			try
			{
				LOG_INFO(
					"Registering User because of Share Workspace"
					", shareWorkspaceCode: {}"
					", email: {}",
					shareWorkspaceCode, email
				);

				tuple<int64_t, int64_t, string> registerUserDetails = _mmsEngineDBFacade->registerUserAndShareWorkspace(
					name, email, password, country, timezone, shareWorkspaceCode,
					chrono::system_clock::now() + chrono::hours(24 * 365 * 10) // chrono::system_clock::time_point userExpirationDate
				);

				workspaceKey = get<0>(registerUserDetails);
				userKey = get<1>(registerUserDetails);
				confirmationCode = get<2>(registerUserDetails);

				LOG_INFO(
					"Registered User and shared Workspace"
					", email: {}"
					", userKey: {}"
					", confirmationCode: {}",
					email, userKey, confirmationCode
				);
			}
			catch (exception &e)
			{
				string errorMessage = std::format(
					"API failed"
					", API: {}"
					", requestData.requestBody: {}"
					", e.what(): {}",
					api, requestData.requestBody, e.what()
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		try
		{
			json registrationRoot;
			// registrationRoot["workspaceKey"] = workspaceKey;
			registrationRoot["userKey"] = userKey;
			registrationRoot["confirmationCode"] = confirmationCode;

			string responseBody = JSONUtils::toString(registrationRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);

			string confirmationURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				confirmationURL += (":" + to_string(_guiPort));
			confirmationURL +=
				("/catramms/login.xhtml?confirmationRequested=true&confirmationUserKey=" + to_string(userKey) +
				 "&confirmationCode=" + confirmationCode);

			LOG_INFO(
				"Sending confirmation URL by email..."
				", confirmationURL: {}",
				confirmationURL
			);

			string tosCommaSeparated = email;
			string subject = "Confirmation code";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Dear ") + name + ",</p>");
			emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;the registration has been done successfully</p>");
			emailBody.push_back(
				string("<p>&emsp;&emsp;&emsp;&emsp;Here follows the user key <b>") + to_string(userKey) + "</b> and the confirmation code <b>" +
				confirmationCode + "</b> to be used to confirm the registration</p>"
			);
			emailBody.push_back(
				string("<p>&emsp;&emsp;&emsp;&emsp;<b>Please click <a href=\"") + confirmationURL + "\">here</a> to confirm the registration</b></p>"
			);
			emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
			emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
			);
			// EMailSender emailSender(_logger, _configuration);
			// bool useMMSCCToo = true;
			// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::createWorkspace(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "createWorkspace";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canCreateRemoveWorkspace)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canCreateRemoveWorkspace: {}",
			apiAuthorizationDetails->canCreateRemoveWorkspace
		);
		LOG_ERROR(errorMessage);

		sendError(request, 403, errorMessage);

		throw runtime_error(errorMessage);
	}

	try
	{
		MMSEngineDBFacade::EncodingPriority encodingPriority;
		MMSEngineDBFacade::EncodingPeriod encodingPeriod;
		int maxIngestionsNumber;
		int maxStorageInMB;

		string workspaceName = requestData.getQueryParameter("workspaceName", "", true);

		encodingPriority = _encodingPriorityWorkspaceDefaultValue;

		encodingPeriod = _encodingPeriodWorkspaceDefaultValue;
		maxIngestionsNumber = _maxIngestionsNumberWorkspaceDefaultValue;

		maxStorageInMB = _maxStorageInMBWorkspaceDefaultValue;

		int64_t workspaceKey;
		string confirmationCode;
		try
		{
			LOG_INFO(
				"Creating Workspace"
				", workspaceName: {}",
				workspaceName
			);

#ifdef __POSTGRES__
			pair<int64_t, string> workspaceKeyAndConfirmationCode = _mmsEngineDBFacade->createWorkspace(
				apiAuthorizationDetails->userKey, workspaceName, "" /* notes */, MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,
				"", // string deliveryURL,
				encodingPriority, encodingPeriod, maxIngestionsNumber, maxStorageInMB,
				"",	   // string languageCode,
				"CET", // default timezone
				apiAuthorizationDetails->admin, chrono::system_clock::now() + chrono::hours(24 * 365 * 10)
			);
#else
			pair<int64_t, string> workspaceKeyAndConfirmationCode = _mmsEngineDBFacade->createWorkspace(
				userKey, workspaceName, MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,
				"", // string deliveryURL,
				encodingPriority, encodingPeriod, maxIngestionsNumber, maxStorageInMB,
				"", // string languageCode,
				apiAuthorizationDetails->admin, chrono::system_clock::now() + chrono::hours(24 * 365 * 10)
			);
#endif

			LOG_INFO(
				"Created a new Workspace for the User"
				", workspaceName: {}"
				", userKey: {}"
				", confirmationCode: {}",
				workspaceName, apiAuthorizationDetails->userKey, get<1>(workspaceKeyAndConfirmationCode)
			);

			workspaceKey = get<0>(workspaceKeyAndConfirmationCode);
			confirmationCode = get<1>(workspaceKeyAndConfirmationCode);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// workspace initialization
		{
			try
			{
				LOG_INFO(
					"Associate defaults encoders to the Workspace"
					", workspaceKey: {}"
					", _sharedEncodersPoolLabel: {}",
					workspaceKey, _sharedEncodersPoolLabel
				);

				_mmsEngineDBFacade->addAssociationWorkspaceEncoder(workspaceKey, _sharedEncodersPoolLabel, _sharedEncodersLabel);
			}
			catch (exception &e)
			{
				string errorMessage = std::format(
					"API failed"
					", API: {}"
					", requestData.requestBody: {}"
					", e.what(): {}",
					api, requestData.requestBody, e.what()
				);
				LOG_ERROR(errorMessage);

				// 2021-09-30: we do not raise an exception because this association
				// is not critical for the account
				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}

			try
			{
				LOG_INFO(
					"Add some HLS_Channels to the Workspace"
					", workspaceKey: {}"
					", _defaultSharedHLSChannelsNumber: {}",
					workspaceKey, _defaultSharedHLSChannelsNumber
				);

				for (int hlsChannelIndex = 0; hlsChannelIndex < _defaultSharedHLSChannelsNumber; hlsChannelIndex++)
					_mmsEngineDBFacade->addHLSChannelConf(workspaceKey, to_string(hlsChannelIndex + 1), hlsChannelIndex + 1, -1, -1, "SHARED");
			}
			catch (exception &e)
			{
				string errorMessage = std::format(
					"API failed"
					", API: {}"
					", requestData.requestBody: {}"
					", e.what(): {}",
					api, requestData.requestBody, e.what()
				);
				LOG_ERROR(errorMessage);

				// 2021-09-30: we do not raise an exception because this association
				// is not critical for the account
				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		try
		{
			json registrationRoot;
			// registrationRoot["workspaceKey"] = workspaceKey;
			registrationRoot["userKey"] = apiAuthorizationDetails->userKey;
			registrationRoot["workspaceKey"] = workspaceKey;
			registrationRoot["confirmationCode"] = confirmationCode;

			string responseBody = JSONUtils::toString(registrationRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);

			pair<string, string> emailAddressAndName = _mmsEngineDBFacade->getUserDetails(apiAuthorizationDetails->userKey);

			string confirmationURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				confirmationURL += (":" + to_string(_guiPort));
			confirmationURL +=
				("/catramms/conf/yourWorkspaces.xhtml?confirmationRequested=true&confirmationUserKey=" + to_string(apiAuthorizationDetails->userKey) +
				 "&confirmationCode=" + confirmationCode);

			LOG_INFO(
				"Created Workspace code"
				", workspaceKey: {}"
				", userKey: {}"
				", confirmationCode: {}"
				", confirmationURL: {}",
				workspaceKey, apiAuthorizationDetails->userKey, confirmationCode, confirmationURL
			);

			string tosCommaSeparated = emailAddressAndName.first;
			string subject = "Confirmation code";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Dear ") + emailAddressAndName.second + ",</p>");
			emailBody.push_back(string("<p>the Workspace has been created successfully</p>"));
			emailBody.push_back(string("<p>here follows the confirmation code ") + confirmationCode + " to be used to confirm the registration</p>");
			// string confirmURL = _apiProtocol + "://" + _apiHostname + ":" + to_string(_apiPort) + "/catramms/" + _apiVersion + "/user/"
			// 	+ to_string(userKey) + "/" + confirmationCode;
			emailBody.push_back(string("<p>Click <a href=\"") + confirmationURL + "\">here</a> to confirm the registration</p>");
			emailBody.push_back("<p>Have a nice day, best regards</p>");
			emailBody.push_back("<p>MMS technical support</p>");

			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
			);
			// EMailSender emailSender(_logger, _configuration);
			// bool useMMSCCToo = true;
			// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::shareWorkspace_(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "shareWorkspace";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canShareWorkspace)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canShareWorkspace: {}",
			apiAuthorizationDetails->canShareWorkspace
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestData.requestBody
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> mandatoryFields = {
			"email"
		};
		for (string field : mandatoryFields)
		{
			if (!JSONUtils::isPresent(metadataRoot, field))
			{
				string errorMessage = std::format("Json field is not present or it is null", ", Json field: {}", field);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		int64_t userKey;
		string name;
		bool userAlreadyPresent;

		string email = JSONUtils::asString(metadataRoot, "email", "");
		try
		{
			emailFormatCheck(email);
		}
		catch (runtime_error &e)
		{
			string errorMessage = std::format(
				"Wrong email format"
				", email: {}",
				email
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		// In case of ActiveDirectory, userAlreadyPresent is always true
		if (_ldapEnabled)
			userAlreadyPresent = true;
		else
		{
			try
			{
				bool warningIfError = true;
				tie(userKey, name) = _mmsEngineDBFacade->getUserDetailsByEmail(email, warningIfError);

				userAlreadyPresent = true;
			}
			catch (exception &e)
			{
				LOG_WARN(
					"API failed"
					", API: {}"
					", requestData.requestBody: {}"
					", e.what(): {}",
					api, requestData.requestBody, e.what()
				);

				userAlreadyPresent = false;
			}
		}

		if (!userAlreadyPresent && !_registerUserEnabled)
		{
			string errorMessage = "registerUser is not enabled";
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		bool createRemoveWorkspace = JsonPath(&metadataRoot)["createRemoveWorkspace"].as<bool>(false);
		bool ingestWorkflow = JsonPath(&metadataRoot)["ingestWorkflow"].as<bool>(false);
		bool createProfiles = JsonPath(&metadataRoot)["createProfiles"].as<bool>(false);
		bool deliveryAuthorization = JsonPath(&metadataRoot)["deliveryAuthorization"].as<bool>(false);
		bool shareWorkspace = JsonPath(&metadataRoot)["shareWorkspace"].as<bool>(false);
		bool editMedia = JsonPath(&metadataRoot)["editMedia"].as<bool>(false);
		bool editConfiguration = JsonPath(&metadataRoot)["editConfiguration"].as<bool>(false);
		bool killEncoding = JsonPath(&metadataRoot)["killEncoding"].as<bool>(false);
		bool cancelIngestionJob = JsonPath(&metadataRoot)["cancelIngestionJob"].as<bool>(false);
		bool editEncodersPool = JsonPath(&metadataRoot)["editEncodersPool"].as<bool>(false);
		bool applicationRecorder = JsonPath(&metadataRoot)["applicationRecorder"].as<bool>(false);
		bool createRemoveLiveChannel = JsonPath(&metadataRoot)["createRemoveLiveChannel"].as<bool>(false);
		bool updateEncoderAndDeliveryStats = JsonPath(&metadataRoot)["updateEncoderAndDeliveryStats"].as<bool>(false);

		try
		{
			LOG_INFO(
				"Sharing workspace"
				", userAlreadyPresent: {}"
				", email: {}",
				userAlreadyPresent, email
			);

			if (userAlreadyPresent)
			{
				bool admin = false;

				LOG_INFO(
					"createdCode"
					", workspace->_workspaceKey: {}"
					", userKey: {}"
					", email: {}"
					", codeType: {}",
					apiAuthorizationDetails->workspace->_workspaceKey, userKey, email,
					MMSEngineDBFacade::toString(MMSEngineDBFacade::CodeType::UserRegistrationComingFromShareWorkspace)
				);

				string shareWorkspaceCode = _mmsEngineDBFacade->createCode(
					apiAuthorizationDetails->workspace->_workspaceKey, userKey, email,
					MMSEngineDBFacade::CodeType::UserRegistrationComingFromShareWorkspace, admin,
					createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization, shareWorkspace, editMedia, editConfiguration,
					killEncoding, cancelIngestionJob, editEncodersPool, applicationRecorder, createRemoveLiveChannel, updateEncoderAndDeliveryStats
				);

				string confirmationURL = _guiProtocol + "://" + _guiHostname;
				if (_guiProtocol == "https" && _guiPort != 443)
					confirmationURL += (":" + to_string(_guiPort));
				confirmationURL +=
					("/catramms/login.xhtml?confirmationRequested=true&confirmationUserKey=" + to_string(userKey) +
					 "&confirmationCode=" + shareWorkspaceCode);

				LOG_INFO(
					"Created Shared Workspace code"
					", workspace->_workspaceKey: {}"
					", email: {}"
					", userKey: {}"
					", confirmationCode: {}"
					", confirmationURL: {}",
					apiAuthorizationDetails->workspace->_workspaceKey, email, userKey, shareWorkspaceCode, confirmationURL
				);

				json registrationRoot;
				registrationRoot["userKey"] = userKey;
				registrationRoot["confirmationCode"] = shareWorkspaceCode;

				string responseBody = JSONUtils::toString(registrationRoot);

				sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);

				string tosCommaSeparated = email;
				string subject = "Share Workspace code";

				vector<string> emailBody;
				emailBody.push_back(string("<p>Dear ") + name + ",</p>");
				emailBody.push_back(std::format("<p>&emsp;&emsp;&emsp;&emsp;the '{}' workspace has been shared successfully</p>", apiAuthorizationDetails->workspace->_name));
				emailBody.push_back(
					string("<p>&emsp;&emsp;&emsp;&emsp;Here follows the user key <b>") + to_string(userKey) + "</b> and the confirmation code <b>" +
					shareWorkspaceCode + "</b> to be used to confirm the sharing of the Workspace</p>"
				);
				emailBody.push_back(
					string("<p>&emsp;&emsp;&emsp;&emsp;<b>Please click <a href=\"") + confirmationURL +
					"\">here</a> to confirm the registration</b></p>"
				);
				emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
				emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

				CurlWrapper::sendEmail(
					_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
					_emailUserName,	   // i.e.: info@catramms-cloud.com
					_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
				);
				// EMailSender emailSender(_logger, _configuration);
				// bool useMMSCCToo = true;
				// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
			}
			else
			{
				bool admin = false;

				LOG_INFO(
					"createdCode"
					", workspace->_workspaceKey: "
					", userKey: -1"
					", email: {}"
					", codeType: {}",
					apiAuthorizationDetails->workspace->_workspaceKey, email, MMSEngineDBFacade::toString(MMSEngineDBFacade::CodeType::ShareWorkspace)
				);

				string shareWorkspaceCode = _mmsEngineDBFacade->createCode(
					apiAuthorizationDetails->workspace->_workspaceKey, -1, email,
					MMSEngineDBFacade::CodeType::ShareWorkspace, admin,
					createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization, shareWorkspace, editMedia, editConfiguration,
					killEncoding, cancelIngestionJob, editEncodersPool, applicationRecorder, createRemoveLiveChannel, updateEncoderAndDeliveryStats
				);

				string shareWorkspaceURL = _guiProtocol + "://" + _guiHostname;
				if (_guiProtocol == "https" && _guiPort != 443)
					shareWorkspaceURL += (":" + to_string(_guiPort));
				shareWorkspaceURL += ("/catramms/login.xhtml?shareWorkspaceRequested=true&shareWorkspaceCode=" + shareWorkspaceCode);
				shareWorkspaceURL += ("&registrationEMail=" + email);

				LOG_INFO(
					"Created Shared Workspace code"
					", workspace->_workspaceKey: {}"
					", email: {}"
					", shareWorkspaceCode: {}"
					", shareWorkspaceURL: {}",
					apiAuthorizationDetails->workspace->_workspaceKey, email, shareWorkspaceCode, shareWorkspaceURL
				);

				json registrationRoot;
				registrationRoot["shareWorkspaceCode"] = shareWorkspaceCode;

				string responseBody = JSONUtils::toString(registrationRoot);

				sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);

				string tosCommaSeparated = email;
				string subject = "Share Workspace code";

				vector<string> emailBody;
				emailBody.push_back(string("<p>Dear ") + email + ",</p>");
				emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;the <b>" + apiAuthorizationDetails->workspace->_name + "</b> workspace was shared with you</p>"));
				emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;Here follows the share workspace code <b>") + shareWorkspaceCode + "</b></p>");
				emailBody.push_back(
					string("<p>&emsp;&emsp;&emsp;&emsp;<b>Please click <a href=\"") + shareWorkspaceURL +
					"\">here</a> to continue with the registration</b></p>"
				);
				emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
				emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

				CurlWrapper::sendEmail(
					_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
					_emailUserName,	   // i.e.: info@catramms-cloud.com
					_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
				);
				// EMailSender emailSender(_logger, _configuration);
				// bool useMMSCCToo = true;
				// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::workspaceList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "workspaceList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		bool costDetails = requestData.getQueryParameter("costDetails", false);

		{
			const json workspaceListRoot = _mmsEngineDBFacade->getWorkspaceList(apiAuthorizationDetails->userKey, apiAuthorizationDetails->admin, costDetails);

			const string responseBody = JSONUtils::toString(workspaceListRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::confirmRegistration(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "confirmRegistration";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO("Received {}", api);

	try
	{
		string confirmationCode = requestData.getQueryParameter("confirmationeCode", "", true);

		try
		{
			tuple<string, string, string> apiKeyNameAndEmailAddress =
				_mmsEngineDBFacade->confirmRegistration(confirmationCode, _expirationInDaysWorkspaceDefaultValue);

			string apiKey;
			string name;
			string emailAddress;

			tie(apiKey, name, emailAddress) = apiKeyNameAndEmailAddress;

			json registrationRoot;
			registrationRoot["apiKey"] = apiKey;

			string responseBody = JSONUtils::toString(registrationRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);

			string tosCommaSeparated = emailAddress;
			string subject = "Welcome";

			string loginURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				loginURL += (":" + to_string(_guiPort));
			loginURL += "/catramms/login.xhtml?confirmationRequested=false";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Dear ") + name + ",</p>");
			emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;Thank you for choosing the CatraMMS services</p>");
			emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;Your registration is now completed and you can enjoy working with MMS</p>");
			emailBody.push_back(
				string("<p>&emsp;&emsp;&emsp;&emsp;<b>Please click <a href=\"") + loginURL + "\">here</a> to login into the MMS platform</b></p>"
			);
			emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;Best regards</p>");
			emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
			);
			// EMailSender emailSender(_logger, _configuration);
			// bool useMMSCCToo = true;
			// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::login(const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData)
{
	string api = "login";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}",
		// commented because of the password
		// ", requestData.requestBody: {}",
		api
	);

	try
	{
		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestData.requestBody
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		json loginDetailsRoot;
		int64_t userKey;
		string remoteClientIPAddress;

		{
			if (!_ldapEnabled)
			{
				vector<string> mandatoryFields = {"email", "password"};
				for (string field : mandatoryFields)
				{
					if (!JSONUtils::isPresent(metadataRoot, field))
					{
						string errorMessage = std::format(
							"Json field is not present or it is null"
							", Json field: {}",
							field
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				string field = "email";
				string email = JSONUtils::asString(metadataRoot, field, "");

				field = "password";
				string password = JSONUtils::asString(metadataRoot, field, "");

				field = "remoteClientIPAddress";
				remoteClientIPAddress = JSONUtils::asString(metadataRoot, field, "");

				try
				{
					LOG_INFO(
						"Login User"
						", email: {}",
						email
					);

					loginDetailsRoot = _mmsEngineDBFacade->login(email, password);

					field = "ldapEnabled";
					loginDetailsRoot[field] = _ldapEnabled;

					field = "mmsVersion";
					loginDetailsRoot[field] = _mmsVersion;

					field = "userKey";
					userKey = JSONUtils::asInt64(loginDetailsRoot, field, 0);

					LOG_INFO(
						"Login User"
						", userKey: {}"
						", email: {}",
						userKey, email
					);
				}
				catch (exception &e)
				{
					string errorMessage = std::format(
						"API failed"
						", API: {}"
						", requestData.requestBody: {}"
						", e.what(): {}",
						api, requestData.requestBody, e.what()
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else // if (_ldapEnabled)
			{
				vector<string> mandatoryFields = {"name", "password"};
				for (string field : mandatoryFields)
				{
					if (!JSONUtils::isPresent(metadataRoot, field))
					{
						string errorMessage = std::format("Json field is not present or it is null", ", Json field: {}", field);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				string field = "name";
				string userName = JSONUtils::asString(metadataRoot, field, "");

				field = "password";
				string password = JSONUtils::asString(metadataRoot, field, "");

				field = "remoteClientIPAddress";
				remoteClientIPAddress = JSONUtils::asString(metadataRoot, field, "");

				try
				{
					LOG_INFO(
						"Login User"
						", userName: {}",
						userName
					);

					string email;
					bool testCredentialsSuccessful = false;

					{
						istringstream iss(_ldapURL);
						vector<string> ldapURLs;
						copy(istream_iterator<std::string>(iss), istream_iterator<std::string>(), back_inserter(ldapURLs));

						for (string ldapURL : ldapURLs)
						{
							try
							{
								LOG_INFO(
									"ldap URL"
									", ldapURL: {}"
									", userName: {}",
									ldapURL, userName
								);

								LdapWrapper ldapWrapper;

								ldapWrapper.init(ldapURL, _ldapCertificatePathName, _ldapManagerUserName, _ldapManagerPassword);

								pair<bool, string> testCredentialsSuccessfulAndEmail = ldapWrapper.testCredentials(userName, password, _ldapBaseDn);

								tie(testCredentialsSuccessful, email) = testCredentialsSuccessfulAndEmail;

								break;
							}
							catch (runtime_error &e)
							{
								LOG_ERROR(
									"ldap URL failed"
									", ldapURL: {}"
									", e.what: {}",
									ldapURL, e.what()
								);
							}
						}
					}

					if (!testCredentialsSuccessful)
					{
						LOG_ERROR(
							"ldap Login failed"
							", userName: {}",
							userName
						);

						throw LoginFailed();
					}

					bool userAlreadyRegistered;
					try
					{
						LOG_INFO(
							"Login User"
							", email: {}",
							email
						);

						loginDetailsRoot = _mmsEngineDBFacade->login(
							email,
							string("") // password in case of ActiveDirectory is empty
						);

						field = "ldapEnabled";
						loginDetailsRoot[field] = _ldapEnabled;

						field = "userKey";
						userKey = JSONUtils::asInt64(loginDetailsRoot, field, 0);

						LOG_INFO(
							"Login User"
							", userKey: {}"
							", email: {}",
							userKey, email
						);

						userAlreadyRegistered = true;
					}
					catch (LoginFailed &e)
					{
						userAlreadyRegistered = false;
					}
					catch (exception &e)
					{
						string errorMessage = std::format(
							"API failed"
							", API: {}"
							", requestData.requestBody: {}"
							", e.what(): {}",
							api, requestData.requestBody, e.what()
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					if (!userAlreadyRegistered)
					{
						LOG_INFO(
							"Register ActiveDirectory User"
							", email: {}",
							email
						);

						// flags set by default
						bool createRemoveWorkspace = true;
						bool ingestWorkflow = true;
						bool createProfiles = true;
						bool deliveryAuthorization = true;
						bool shareWorkspace = true;
						bool editMedia = true;
						bool editConfiguration = true;
						bool killEncoding = true;
						bool cancelIngestionJob = true;
						bool editEncodersPool = true;
						bool applicationRecorder = true;
						bool createRemoveLiveChannel = true;
						bool updateEncoderAndDeliveryStats = false;
						pair<int64_t, string> userKeyAndEmail = _mmsEngineDBFacade->registerActiveDirectoryUser(
							userName, email,
							"", // userCountry,
							"CET", createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
							shareWorkspace, editMedia,
							editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool, applicationRecorder, createRemoveLiveChannel,
							updateEncoderAndDeliveryStats,
							_ldapDefaultWorkspaceKeys, _expirationInDaysWorkspaceDefaultValue,
							chrono::system_clock::now() + chrono::hours(24 * 365 * 10)
							// chrono::system_clock::time_point userExpirationDate
						);

						string apiKey;
						tie(userKey, apiKey) = userKeyAndEmail;

						LOG_INFO(
							"Login User"
							", userKey: {}"
							", apiKey: {}"
							", email: {}",
							userKey, apiKey, email
						);

						loginDetailsRoot = _mmsEngineDBFacade->login(
							email,
							string("") // password in case of ActiveDirectory is empty
						);

						loginDetailsRoot["ldapEnabled"] = _ldapEnabled;
						userKey = JSONUtils::asInt64(loginDetailsRoot, "userKey", 0);

						LOG_INFO(
							"Login User"
							", userKey: {}"
							", email: {}",
							userKey, email
						);
					}
				}
				catch (exception &e)
				{
					string errorMessage = std::format(
						"API failed"
						", API: {}"
						", requestData.requestBody: {}"
						", e.what(): {}",
						api, requestData.requestBody, e.what()
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}

		if (!remoteClientIPAddress.empty())
		{
			try
			{
				LOG_INFO(
					"_mmsEngineDBFacade->saveLoginStatistics"
					", userKey: {}"
					", remoteClientIPAddress: {}",
					userKey, remoteClientIPAddress
				);
				_mmsEngineDBFacade->saveLoginStatistics(userKey, remoteClientIPAddress);
			}
			catch (exception &e)
			{
				LOG_ERROR(
					"Saving Login Statistics failed"
					", userKey: {}"
					", remoteClientIPAddress: {}"
					", e.what(): {}",
					userKey, remoteClientIPAddress, e.what()
				);

				// string errorMessage = string("Internal server error");
				// LOG_ERROR(errorMessage);

				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		try
		{
			LOG_INFO(
				"Login User"
				", userKey: {}",
				userKey
			);

			json loginWorkspaceRoot = _mmsEngineDBFacade->getLoginWorkspace(
				userKey,
				// 2022-12-18: viene chiamato quando l'utente fa la login
				false
			);

			string field = "workspace";
			loginDetailsRoot[field] = loginWorkspaceRoot;

			string responseBody = JSONUtils::toString(loginDetailsRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::updateUser(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "updateUser";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
	);

	try
	{
		string name;
		bool nameChanged;
		string email;
		bool emailChanged;
		string country;
		bool countryChanged;
		string timezone;
		bool timezoneChanged;
		bool insolvent;
		bool insolventChanged;
		string expirationUtcDate;
		bool expirationDateChanged;
		bool passwordChanged;
		string newPassword;
		string oldPassword;

		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestData.requestBody
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		nameChanged = false;
		emailChanged = false;
		countryChanged = false;
		timezoneChanged = false;
		insolventChanged = false;
		expirationDateChanged = false;
		passwordChanged = false;
		if (!_ldapEnabled)
		{
			string field = "name";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				name = JSONUtils::asString(metadataRoot, field, "");
				nameChanged = true;
			}

			field = "email";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				email = JSONUtils::asString(metadataRoot, field, "");
				try
				{
					emailFormatCheck(email);
				}
				catch (runtime_error &e)
				{
					string errorMessage = std::format(
						"Wrong email format"
						", email: {}"
						", exception: {}",
						email, e.what()
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				emailChanged = true;
			}

			field = "country";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				country = JSONUtils::asString(metadataRoot, field, "");
				countryChanged = true;
			}

			field = "timezone";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				timezone = JSONUtils::asString(metadataRoot, field, "CET");
				timezoneChanged = true;
			}

			if (apiAuthorizationDetails->admin)
			{
				field = "insolvent";
				if (JSONUtils::isPresent(metadataRoot, field))
				{
					insolvent = JSONUtils::asBool(metadataRoot, field, false);
					insolventChanged = true;
				}
			}

			if (apiAuthorizationDetails->admin)
			{
				field = "expirationDate";
				if (JSONUtils::isPresent(metadataRoot, field))
				{
					expirationUtcDate = JSONUtils::asString(metadataRoot, field, "");
					expirationDateChanged = true;
				}
			}

			if (JSONUtils::isPresent(metadataRoot, "newPassword") && JSONUtils::isPresent(metadataRoot, "oldPassword"))
			{
				passwordChanged = true;
				newPassword = JSONUtils::asString(metadataRoot, "newPassword", "");
				oldPassword = JSONUtils::asString(metadataRoot, "oldPassword", "");
			}
		}
		else
		{
			string field = "country";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				country = JSONUtils::asString(metadataRoot, field, "");
				countryChanged = true;
			}

			field = "timezone";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				timezone = JSONUtils::asString(metadataRoot, field, "CET");
				timezoneChanged = true;
			}
		}

		try
		{
			LOG_INFO(
				"Updating User"
				", userKey: {}"
				", name: {}"
				", email: {}",
				apiAuthorizationDetails->userKey, name, email
			);

			json loginDetailsRoot = _mmsEngineDBFacade->updateUser(
				apiAuthorizationDetails->admin, _ldapEnabled, apiAuthorizationDetails->userKey, nameChanged, name, emailChanged, email,
				countryChanged, country, timezoneChanged, timezone, insolventChanged, insolvent, expirationDateChanged,
				expirationUtcDate, passwordChanged, newPassword, oldPassword
			);

			LOG_INFO(
				"User updated"
				", userKey: {}"
				", name: {}"
				", email: {}",
				apiAuthorizationDetails->userKey, name, email
			);

			string responseBody = JSONUtils::toString(loginDetailsRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::createTokenToResetPassword(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData)
{
	string api = "createTokenToResetPassword";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO("Received {}", api);

	try
	{
		string email = requestData.getQueryParameter("email", "", true);

		string resetPasswordToken;
		string name;
		try
		{
			LOG_INFO(
				"getUserDetailsByEmail"
				", email: {}",
				email
			);

			bool warningIfError = false;
			pair<int64_t, string> userDetails = _mmsEngineDBFacade->getUserDetailsByEmail(email, warningIfError);
			int64_t userKey = userDetails.first;
			name = userDetails.second;

			resetPasswordToken = _mmsEngineDBFacade->createResetPasswordToken(userKey);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);
			LOG_ERROR(errorMessage);
			throw runtime_error(errorMessage	);
		}

		try
		{
			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, "");

			string resetPasswordURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				resetPasswordURL += (":" + to_string(_guiPort));
			resetPasswordURL += (string("/catramms/login.xhtml?resetPasswordRequested=true") + "&resetPasswordToken=" + resetPasswordToken);

			string tosCommaSeparated = email;
			string subject = "Reset password";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Dear ") + name + ",</p>");
			emailBody.push_back(
				string("<p><b>Please click <a href=\"") + resetPasswordURL +
				"\">here</a> to reset your password. This link is valid for a limited time.</b></p>"
			);
			emailBody.emplace_back("In case you did not request any reset of your password, just ignore this email</p>");

			emailBody.emplace_back("<p>Have a nice day, best regards</p>");
			emailBody.emplace_back("<p>MMS technical support</p>");

			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
			);
			// EMailSender emailSender(_logger, _configuration);
			// bool useMMSCCToo = true;
			// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::resetPassword(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData)
{
	string api = "resetPassword";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
	);

	try
	{
		string newPassword;
		string resetPasswordToken;

		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = fmt::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestData.requestBody
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		resetPasswordToken = JSONUtils::asString(metadataRoot, "resetPasswordToken", "");
		if (resetPasswordToken.empty())
		{
			string errorMessage = "The 'resetPasswordToken' parameter is not found";
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		newPassword = JSONUtils::asString(metadataRoot, "newPassword", "");
		if (newPassword.empty())
		{
			string errorMessage = "The 'newPassword' parameter is not found";
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string name;
		string email;
		try
		{
			LOG_INFO(
				"Reset Password"
				", resetPasswordToken: {}",
				resetPasswordToken
			);

			pair<string, string> details = _mmsEngineDBFacade->resetPassword(resetPasswordToken, newPassword);
			name = details.first;
			email = details.second;
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, "");

			string tosCommaSeparated = email;
			string subject = "Reset password";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Dear ") + name + ",</p>");
			emailBody.emplace_back("your password has been changed.</p>");

			emailBody.emplace_back("<p>Have a nice day, best regards</p>");
			emailBody.emplace_back("<p>MMS technical support</p>");

			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
			);
			// EMailSender emailSender(_logger, _configuration);
			// bool useMMSCCToo = true;
			// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::updateWorkspace(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "updateWorkspace";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		bool newEnabled;
		bool enabledChanged = false;
		string newName;
		bool nameChanged = false;
		string newNotes;
		bool notesChanged = false;
		string newMaxEncodingPriority;
		bool maxEncodingPriorityChanged = false;
		string newEncodingPeriod;
		bool encodingPeriodChanged = false;
		int64_t newMaxIngestionsNumber;
		bool maxIngestionsNumberChanged = false;
		string newLanguageCode;
		bool languageCodeChanged = false;
		string newTimezone;
		bool timezoneChanged = false;
		string newPreferences;
		bool preferencesChanged = false;
		string newExternalDeliveries;
		bool externalDeliveriesChanged = false;
		string newExpirationUtcDate;
		bool expirationDateChanged = false;

		bool maxStorageInGBChanged = false;
		int64_t maxStorageInGB;
		bool currentCostForStorageChanged = false;
		int64_t currentCostForStorage;
		bool dedicatedEncoder_power_1Changed = false;
		int64_t dedicatedEncoder_power_1;
		bool currentCostForDedicatedEncoder_power_1Changed = false;
		int64_t currentCostForDedicatedEncoder_power_1;
		bool dedicatedEncoder_power_2Changed = false;
		int64_t dedicatedEncoder_power_2;
		bool currentCostForDedicatedEncoder_power_2Changed = false;
		int64_t currentCostForDedicatedEncoder_power_2;
		bool dedicatedEncoder_power_3Changed = false;
		int64_t dedicatedEncoder_power_3;
		bool currentCostForDedicatedEncoder_power_3Changed = false;
		int64_t currentCostForDedicatedEncoder_power_3;
		bool CDN_type_1Changed = false;
		int64_t CDN_type_1;
		bool currentCostForCDN_type_1Changed = false;
		int64_t currentCostForCDN_type_1;
		bool support_type_1Changed = false;
		bool support_type_1;
		bool currentCostForSupport_type_1Changed = false;
		int64_t currentCostForSupport_type_1;

		bool newCreateRemoveWorkspace;
		bool newIngestWorkflow;
		bool newCreateProfiles;
		bool newDeliveryAuthorization;
		bool newShareWorkspace;
		bool newEditMedia;
		bool newEditConfiguration;
		bool newKillEncoding;
		bool newCancelIngestionJob;
		bool newEditEncodersPool;
		bool newApplicationRecorder;
		bool newCreateRemoveLiveChannel;
		bool newUpdateEncoderAndDeliveryStats;

		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestData.requestBody
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string field = "enabled";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			enabledChanged = true;
			newEnabled = JSONUtils::asBool(metadataRoot, field, false);
		}

		field = "workspaceName";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			nameChanged = true;
			newName = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "workspaceNotes";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			notesChanged = true;
			newNotes = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "maxEncodingPriority";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			maxEncodingPriorityChanged = true;
			newMaxEncodingPriority = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "encodingPeriod";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			encodingPeriodChanged = true;
			newEncodingPeriod = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "maxIngestionsNumber";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			maxIngestionsNumberChanged = true;
			newMaxIngestionsNumber = JSONUtils::asInt64(metadataRoot, field, 0);
		}

		field = "languageCode";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			languageCodeChanged = true;
			newLanguageCode = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "timezone";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			timezoneChanged = true;
			newTimezone = JSONUtils::asString(metadataRoot, field, "CET");
		}

		if (JSONUtils::isPresent(metadataRoot, "preferences"))
		{
			preferencesChanged = true;
			newPreferences = JSONUtils::asString(metadataRoot, "preferences", "");
		}

		if (JSONUtils::isPresent(metadataRoot, "externalDeliveries"))
		{
			externalDeliveriesChanged = true;
			newExternalDeliveries = JSONUtils::asString(metadataRoot, "externalDeliveries", "");
		}

		field = "maxStorageInGB";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			maxStorageInGBChanged = true;
			maxStorageInGB = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForStorage";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			currentCostForStorageChanged = true;
			currentCostForStorage = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "dedicatedEncoder_power_1";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			dedicatedEncoder_power_1Changed = true;
			dedicatedEncoder_power_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForDedicatedEncoder_power_1";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			currentCostForDedicatedEncoder_power_1Changed = true;
			currentCostForDedicatedEncoder_power_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "dedicatedEncoder_power_2";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			dedicatedEncoder_power_2Changed = true;
			dedicatedEncoder_power_2 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForDedicatedEncoder_power_2";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			currentCostForDedicatedEncoder_power_2Changed = true;
			currentCostForDedicatedEncoder_power_2 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "dedicatedEncoder_power_3";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			dedicatedEncoder_power_3Changed = true;
			dedicatedEncoder_power_3 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForDedicatedEncoder_power_3";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			currentCostForDedicatedEncoder_power_3Changed = true;
			currentCostForDedicatedEncoder_power_3 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "CDN_type_1";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			CDN_type_1Changed = true;
			CDN_type_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForCDN_type_1";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			currentCostForCDN_type_1Changed = true;
			currentCostForCDN_type_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "support_type_1";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			support_type_1Changed = true;
			support_type_1 = JSONUtils::asBool(metadataRoot, field, false);
		}

		field = "currentCostForSupport_type_1";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			currentCostForSupport_type_1Changed = true;
			currentCostForSupport_type_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "userAPIKey";
		if (JSONUtils::isPresent(metadataRoot, field))
		{
			json userAPIKeyRoot = metadataRoot[field];

			{
				vector<string> mandatoryFields = {"createRemoveWorkspace", "ingestWorkflow",   "createProfiles",	  "deliveryAuthorization",
												  "shareWorkspace",		   "editMedia",		   "editConfiguration",	  "killEncoding",
												  "cancelIngestionJob",	   "editEncodersPool", "applicationRecorder", "createRemoveLiveChannel",
					                              "updateEncoderAndDeliveryStats"};
				for (const string& field : mandatoryFields)
				{
					if (!JSONUtils::isPresent(userAPIKeyRoot, field))
					{
						string errorMessage = std::format(
							"Json field is not present or it is null"
							", Json field: {}",
							field
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}

			field = "expirationDate";
			if (JSONUtils::isPresent(userAPIKeyRoot, field))
			{
				expirationDateChanged = true;
				newExpirationUtcDate = JSONUtils::asString(userAPIKeyRoot, field, "");
			}

			field = "createRemoveWorkspace";
			newCreateRemoveWorkspace = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "ingestWorkflow";
			newIngestWorkflow = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "createProfiles";
			newCreateProfiles = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "deliveryAuthorization";
			newDeliveryAuthorization = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "shareWorkspace";
			newShareWorkspace = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "editMedia";
			newEditMedia = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "editConfiguration";
			newEditConfiguration = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "killEncoding";
			newKillEncoding = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "cancelIngestionJob";
			newCancelIngestionJob = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "editEncodersPool";
			newEditEncodersPool = JSONUtils::asBool(userAPIKeyRoot, field, false);

			field = "applicationRecorder";
			newApplicationRecorder = JSONUtils::asBool(userAPIKeyRoot, field, false);

			newCreateRemoveLiveChannel = JSONUtils::asBool(userAPIKeyRoot, "createRemoveLiveChannel", false);
			newUpdateEncoderAndDeliveryStats = JSONUtils::asBool(userAPIKeyRoot, "updateEncoderAndDeliveryStats", false);
		}

		try
		{
			LOG_INFO(
				"Updating WorkspaceDetails"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

#ifdef __POSTGRES__
			json workspaceDetailRoot = _mmsEngineDBFacade->updateWorkspaceDetails(
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey, notesChanged, newNotes, enabledChanged, newEnabled, nameChanged, newName,
				maxEncodingPriorityChanged, newMaxEncodingPriority, encodingPeriodChanged, newEncodingPeriod, maxIngestionsNumberChanged,
				newMaxIngestionsNumber, languageCodeChanged, newLanguageCode, timezoneChanged, newTimezone, preferencesChanged, newPreferences,
				externalDeliveriesChanged, newExternalDeliveries, expirationDateChanged, newExpirationUtcDate,

				maxStorageInGBChanged, maxStorageInGB, currentCostForStorageChanged, currentCostForStorage, dedicatedEncoder_power_1Changed,
				dedicatedEncoder_power_1, currentCostForDedicatedEncoder_power_1Changed, currentCostForDedicatedEncoder_power_1,
				dedicatedEncoder_power_2Changed, dedicatedEncoder_power_2, currentCostForDedicatedEncoder_power_2Changed,
				currentCostForDedicatedEncoder_power_2, dedicatedEncoder_power_3Changed, dedicatedEncoder_power_3,
				currentCostForDedicatedEncoder_power_3Changed, currentCostForDedicatedEncoder_power_3, CDN_type_1Changed, CDN_type_1,
				currentCostForCDN_type_1Changed, currentCostForCDN_type_1, support_type_1Changed, support_type_1, currentCostForSupport_type_1Changed,
				currentCostForSupport_type_1,

				newCreateRemoveWorkspace, newIngestWorkflow, newCreateProfiles, newDeliveryAuthorization, newShareWorkspace, newEditMedia,
				newEditConfiguration, newKillEncoding, newCancelIngestionJob, newEditEncodersPool, newApplicationRecorder,
				newCreateRemoveLiveChannel, newUpdateEncoderAndDeliveryStats
			);
#else
			bool maxStorageInMBChanged = false;
			int64_t newMaxStorageInMB = 0;
			json workspaceDetailRoot = _mmsEngineDBFacade->updateWorkspaceDetails(
				userKey, apiAuthorizationDetails->workspace->_workspaceKey, enabledChanged, newEnabled, nameChanged, newName, maxEncodingPriorityChanged,
				newMaxEncodingPriority, encodingPeriodChanged, newEncodingPeriod, maxIngestionsNumberChanged, newMaxIngestionsNumber,
				maxStorageInMBChanged, newMaxStorageInMB, languageCodeChanged, newLanguageCode, expirationDateChanged, newExpirationUtcDate,
				newCreateRemoveWorkspace, newIngestWorkflow, newCreateProfiles, newDeliveryAuthorization, newShareWorkspace, newEditMedia,
				newEditConfiguration, newKillEncoding, newCancelIngestionJob, newEditEncodersPool, newApplicationRecorder
			);
#endif

			LOG_INFO(
				"WorkspaceDetails updated"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			string responseBody = JSONUtils::toString(workspaceDetailRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::setWorkspaceAsDefault(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "setWorkspaceAsDefault";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int64_t workspaceKeyToBeSetAsDefault = requestData.getQueryParameter("workspaceKeyToBeSetAsDefault",
			static_cast<int64_t>(-1), true);

		try
		{
			LOG_INFO(
				"setWorkspaceAsDefault"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			_mmsEngineDBFacade->setWorkspaceAsDefault(apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey, workspaceKeyToBeSetAsDefault);

			string responseBody;

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);
			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::deleteWorkspace(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData)
{
	string api = "deleteWorkspace";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", userKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, apiAuthorizationDetails->userKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canCreateRemoveWorkspace)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canCreateRemoveWorkspace: {}",
			apiAuthorizationDetails->canCreateRemoveWorkspace
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		if (_noFileSystemAccess)
		{
			string errorMessage = std::format(
				"{} failed, no rights to execute this method"
				", _noFileSystemAccess: {}",
				api, _noFileSystemAccess
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			LOG_INFO(
				"Delete Workspace from DB"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			// ritorna gli utenti eliminati perchè avevano solamente il workspace che è stato rimosso
			vector<tuple<int64_t, string, string>> usersRemoved = _mmsEngineDBFacade->deleteWorkspace(apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey);

			LOG_INFO(
				"Workspace from DB deleted"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			if (!usersRemoved.empty())
			{
				for (const tuple<int64_t, string, string>& userDetails : usersRemoved)
				{
					string name;
					string eMailAddress;

					tie(ignore, name, eMailAddress) = userDetails;

					string tosCommaSeparated = eMailAddress;
					string subject = "Your account was removed";

					vector<string> emailBody;
					emailBody.push_back(string("<p>Dear ") + name + ",</p>");
					emailBody.push_back(string(
						"<p>&emsp;&emsp;&emsp;&emsp;your account was removed because the only workspace you had (" + apiAuthorizationDetails->workspace->_name +
						") was removed and</p>"
					));
					emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;your account remained without any workspace.</p>");
					emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;If you still need the CatraMMS services, please register yourself again<b>"
					);
					emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
					emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

					CurlWrapper::sendEmail(
						_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
						_emailUserName,	   // i.e.: info@catramms-cloud.com
						_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
					);
				}
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			LOG_INFO(
				"Delete Workspace from Storage"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			_mmsStorage->deleteWorkspace(apiAuthorizationDetails->workspace);

			LOG_INFO(
				"Workspace from Storage deleted"
				", workspaceKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey
			);

			string responseBody;
			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", userKey: {}"
			", workspace->_workspaceKey: {}"
			", e.what(): {}",
			api, apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::unshareWorkspace(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData)
{
	string api = "unshareWorkspace";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", userKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, apiAuthorizationDetails->userKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canShareWorkspace)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canShareWorkspace: {}",
			apiAuthorizationDetails->canShareWorkspace
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		try
		{
			LOG_INFO(
				"Unshare Workspace"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			// ritorna gli utenti eliminati perchè avevano solamente il workspace che è stato unshared
			auto [userToBeRemoved, name, eMailAddress] = _mmsEngineDBFacade->unshareWorkspace(apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey);

			LOG_INFO(
				"Workspace from DB unshared"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			if (userToBeRemoved)
			{
				string tosCommaSeparated = eMailAddress;
				string subject = "Your account was removed";

				vector<string> emailBody;
				emailBody.push_back(string("<p>Dear ") + name + ",</p>");
				emailBody.push_back(string(
					"<p>&emsp;&emsp;&emsp;&emsp;your account was removed because the only workspace you had (" + apiAuthorizationDetails->workspace->_name +
					") was unshared and</p>"
				));
				emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;your account remained without any workspace.</p>");
				emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;If you still need the CatraMMS services, please register yourself again<b>");
				emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
				emailBody.emplace_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

				CurlWrapper::sendEmail(
					_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
					_emailUserName,	   // i.e.: info@catramms-cloud.com
					_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
				);
			}

			string responseBody;
			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", userKey: {}"
			", workspace->_workspaceKey: {}"
			", e.what(): {}",
			api, apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::workspaceUsage(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData)
{

	string api = "workspaceUsage";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	try
	{
		json workspaceUsageRoot;
		string field;

		{
			json requestParametersRoot;

			field = "requestParameters";
			workspaceUsageRoot[field] = requestParametersRoot;
		}

		json responseRoot;
		{
			int64_t workSpaceUsageInBytes;

			tie(workSpaceUsageInBytes, ignore) = _mmsEngineDBFacade->getWorkspaceUsage(
				apiAuthorizationDetails->workspace->_workspaceKey);

			int64_t workSpaceUsageInMB = workSpaceUsageInBytes / 1000000;

			field = "usageInMB";
			responseRoot[field] = workSpaceUsageInMB;
		}

		field = "response";
		workspaceUsageRoot[field] = responseRoot;

		string responseBody = JSONUtils::toString(workspaceUsageRoot);

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"getWorkspaceUsage exception"
			", e.what(): {}",
			e.what()
		);
		throw;
	}
}

void API::emailFormatCheck(string email)
{
	// [[:w:]] ---> word character: digit, number or undescore
	// l'espressione regolare sotto non accetta il punto nella parte sinistra della @
	// 2024-05-20: https://stackoverflow.com/questions/48055431/can-it-cause-harm-to-validate-email-addresses-with-a-regex
	// 	Visto il link sopra, non utilizzero l'espressione regolare per verificare un email address
	/*
	regex e("[[:w:]]+@[[:w:]]+\\.[[:w:]]+");
	if (!regex_match(email, e))
	{
		string errorMessage = std::format(
			"Wrong email format"
			", email: {}",
			email
		);
		LOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	*/

	// controlli:
	// L'indirizzo contiene almeno un @
	// Il local-part(tutto a sinistra dell'estrema destra @) non è vuoto
	// La parte domain (tutto a destra dell'estrema destra @) contiene almeno un punto (di nuovo, questo non è strettamente vero, ma
	// pragmatico)

	size_t endOfLocalPartIndex = email.find_last_of('@');
	if (endOfLocalPartIndex == string::npos)
	{
		string errorMessage = std::format(
			"Wrong email format"
			", email: {}",
			email
		);
		LOG_ERROR(errorMessage);

		{
			vector<string> emailbody;
			emailbody.push_back("Il metodo API::emailFormatCheck ha scartato l'email: " + email);
			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, "info@catramms-cloud.com", "", "Wrong MMS email format: discarded", emailbody,
				"text/html; charset=\"UTF-8\""
			);
		}

		throw runtime_error(errorMessage);
	}

	string localPart = email.substr(0, endOfLocalPartIndex);
	string domainPart = email.substr(endOfLocalPartIndex + 1);
	if (localPart.empty() || domainPart.find('.') == string::npos)
	{
		string errorMessage = std::format(
			"Wrong email format"
			", email: {}",
			email
		);
		LOG_ERROR(errorMessage);

		{
			vector<string> emailbody;
			emailbody.push_back("Il metodo API::emailFormatCheck ha scartato l'email: " + email);
			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, "info@catramms-cloud.com", "", "Wrong MMS email format: discarded", emailbody,
				"text/html; charset=\"UTF-8\""
			);
		}

		throw runtime_error(errorMessage);
	}
}
