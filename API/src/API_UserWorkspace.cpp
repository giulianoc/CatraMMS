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
#include "LdapWrapper.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"
#include <format>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <vector>

void API::registerUser(string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestBody)
{
	string api = "registerUser";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		if (!_registerUserEnabled)
		{
			string errorMessage = "registerUser is not enabled";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

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
			metadataRoot = JSONUtils::toJson(requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestBody
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> mandatoryFields = {"email", "password"};
		for (string field : mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				string errorMessage = std::format(
					"Json field is not present or it is null"
					", Json field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				sendError(request, 400, errorMessage);

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
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		password = JSONUtils::asString(metadataRoot, "password", "");
		shareWorkspaceCode = JSONUtils::asString(metadataRoot, "shareWorkspaceCode", "");

		string name = JSONUtils::asString(metadataRoot, "name", "");
		string country = JSONUtils::asString(metadataRoot, "country", "");
		string timezone = JSONUtils::asString(metadataRoot, "timezone", "CET");

		if (shareWorkspaceCode == "")
		{
			MMSEngineDBFacade::EncodingPriority encodingPriority;
			MMSEngineDBFacade::EncodingPeriod encodingPeriod;
			int maxIngestionsNumber;
			int maxStorageInMB;

			string workspaceName = JSONUtils::asString(metadataRoot, "workspaceName", "");
			if (workspaceName == "")
			{
				if (name != "")
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
				SPDLOG_INFO(
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

				SPDLOG_INFO(
					"Registered User and added Workspace"
					", workspaceName: {}"
					", email: {}"
					", userKey: {}"
					", confirmationCode: {}",
					workspaceName, email, userKey, confirmationCode
				);
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

				string errorMessage = std::format("Internal server error: {}", e.what());
				SPDLOG_ERROR(errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
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

				string errorMessage = std::format("Internal server error: {}", e.what());
				SPDLOG_ERROR(errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}

			// workspace initialization
			{
				try
				{
					SPDLOG_INFO(
						"Associate defaults encoders to the Workspace"
						", workspaceKey: {}"
						", _sharedEncodersPoolLabel: {}",
						workspaceKey, _sharedEncodersPoolLabel
					);

					_mmsEngineDBFacade->addAssociationWorkspaceEncoder(workspaceKey, _sharedEncodersPoolLabel, _sharedEncodersLabel);
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

					// string errorMessage = string("Internal server error: ") + e.what();
					// SPDLOG_ERROR(errorMessage);

					// 2021-09-30: we do not raise an exception because this association
					// is not critical for the account
					// sendError(request, 500, errorMessage);

					// throw runtime_error(errorMessage);
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

					// string errorMessage = string("Internal server error");
					// SPDLOG_ERROR(errorMessage);

					// 2021-09-30: we do not raise an exception because this association
					// is not critical for the account
					// sendError(request, 500, errorMessage);

					// throw runtime_error(errorMessage);
				}

				try
				{
					SPDLOG_INFO(
						"Add some HLS_Channels to the Workspace"
						", workspaceKey: {}"
						", _defaultSharedHLSChannelsNumber: {}",
						workspaceKey, _defaultSharedHLSChannelsNumber
					);

					for (int hlsChannelIndex = 0; hlsChannelIndex < _defaultSharedHLSChannelsNumber; hlsChannelIndex++)
						_mmsEngineDBFacade->addHLSChannelConf(workspaceKey, to_string(hlsChannelIndex + 1), hlsChannelIndex + 1, -1, -1, "SHARED");
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

					// string errorMessage = string("Internal server error: ") + e.what();
					// SPDLOG_ERROR(errorMessage);

					// 2021-09-30: we do not raise an exception because this association
					// is not critical for the account
					// sendError(request, 500, errorMessage);

					// throw runtime_error(errorMessage);
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

					// string errorMessage = string("Internal server error");
					// SPDLOG_ERROR(errorMessage);

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
				SPDLOG_INFO(
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

				SPDLOG_INFO(
					"Registered User and shared Workspace"
					", email: {}"
					", userKey: {}"
					", confirmationCode: {}",
					email, userKey, confirmationCode
				);
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

				string errorMessage = std::format("Internal server error: {}", e.what());
				SPDLOG_ERROR(errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
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

				string errorMessage = std::format("Internal server error: {}", e.what());
				SPDLOG_ERROR(errorMessage);

				sendError(request, 500, errorMessage);

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

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);

			string confirmationURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				confirmationURL += (":" + to_string(_guiPort));
			confirmationURL +=
				("/catramms/login.xhtml?confirmationRequested=true&confirmationUserKey=" + to_string(userKey) +
				 "&confirmationCode=" + confirmationCode);

			SPDLOG_INFO(
				"Sending confirmation URL by email..."
				", confirmationURL: {}",
				confirmationURL
			);

			string tosCommaSeparated = email;
			string subject = "Confirmation code";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Dear ") + name + ",</p>");
			emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;the registration has been done successfully</p>"));
			emailBody.push_back(
				string("<p>&emsp;&emsp;&emsp;&emsp;Here follows the user key <b>") + to_string(userKey) + "</b> and the confirmation code <b>" +
				confirmationCode + "</b> to be used to confirm the registration</p>"
			);
			emailBody.push_back(
				string("<p>&emsp;&emsp;&emsp;&emsp;<b>Please click <a href=\"") + confirmationURL + "\">here</a> to confirm the registration</b></p>"
			);
			emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
			emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
			);
			// EMailSender emailSender(_logger, _configuration);
			// bool useMMSCCToo = true;
			// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
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

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::createWorkspace(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
	unordered_map<string, string> queryParameters, string requestBody, bool admin
)
{
	string api = "createWorkspace";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		string workspaceName;
		MMSEngineDBFacade::EncodingPriority encodingPriority;
		MMSEngineDBFacade::EncodingPeriod encodingPeriod;
		int maxIngestionsNumber;
		int maxStorageInMB;

		auto workspaceNameIt = queryParameters.find("workspaceName");
		if (workspaceNameIt == queryParameters.end())
		{
			string errorMessage = "The 'workspaceName' parameter is not found";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		{
			workspaceName = workspaceNameIt->second;
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(workspaceName, regex(plus), plusDecoded);

			workspaceName = CurlWrapper::unescape(firstDecoding);
		}

		encodingPriority = _encodingPriorityWorkspaceDefaultValue;

		encodingPeriod = _encodingPeriodWorkspaceDefaultValue;
		maxIngestionsNumber = _maxIngestionsNumberWorkspaceDefaultValue;

		maxStorageInMB = _maxStorageInMBWorkspaceDefaultValue;

		int64_t workspaceKey;
		string confirmationCode;
		try
		{
			SPDLOG_INFO(
				"Creating Workspace"
				", workspaceName: {}",
				workspaceName
			);

#ifdef __POSTGRES__
			pair<int64_t, string> workspaceKeyAndConfirmationCode = _mmsEngineDBFacade->createWorkspace(
				userKey, workspaceName, "" /* notes */, MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,
				"", // string deliveryURL,
				encodingPriority, encodingPeriod, maxIngestionsNumber, maxStorageInMB,
				"",	   // string languageCode,
				"CET", // default timezone
				admin, chrono::system_clock::now() + chrono::hours(24 * 365 * 10)
			);
#else
			pair<int64_t, string> workspaceKeyAndConfirmationCode = _mmsEngineDBFacade->createWorkspace(
				userKey, workspaceName, MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,
				"", // string deliveryURL,
				encodingPriority, encodingPeriod, maxIngestionsNumber, maxStorageInMB,
				"", // string languageCode,
				admin, chrono::system_clock::now() + chrono::hours(24 * 365 * 10)
			);
#endif

			SPDLOG_INFO(
				"Created a new Workspace for the User"
				", workspaceName: {}"
				", userKey: {}"
				", confirmationCode: {}",
				workspaceName, userKey, get<1>(workspaceKeyAndConfirmationCode)
			);

			workspaceKey = get<0>(workspaceKeyAndConfirmationCode);
			confirmationCode = get<1>(workspaceKeyAndConfirmationCode);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		// workspace initialization
		{
			try
			{
				SPDLOG_INFO(
					"Associate defaults encoders to the Workspace"
					", workspaceKey: {}"
					", _sharedEncodersPoolLabel: {}",
					workspaceKey, _sharedEncodersPoolLabel
				);

				_mmsEngineDBFacade->addAssociationWorkspaceEncoder(workspaceKey, _sharedEncodersPoolLabel, _sharedEncodersLabel);
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

				string errorMessage = std::format("Internal server error: {}", e.what());
				SPDLOG_ERROR(errorMessage);

				// 2021-09-30: we do not raise an exception because this association
				// is not critical for the account
				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
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

				string errorMessage = std::format("Internal server error: {}", e.what());
				SPDLOG_ERROR(errorMessage);

				// 2021-09-30: we do not raise an exception because this association
				// is not critical for the account
				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}

			try
			{
				SPDLOG_INFO(
					"Add some HLS_Channels to the Workspace"
					", workspaceKey: {}"
					", _defaultSharedHLSChannelsNumber: {}",
					workspaceKey, _defaultSharedHLSChannelsNumber
				);

				for (int hlsChannelIndex = 0; hlsChannelIndex < _defaultSharedHLSChannelsNumber; hlsChannelIndex++)
					_mmsEngineDBFacade->addHLSChannelConf(workspaceKey, to_string(hlsChannelIndex + 1), hlsChannelIndex + 1, -1, -1, "SHARED");
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

				string errorMessage = std::format("Internal server error: {}", e.what());
				SPDLOG_ERROR(errorMessage);

				// 2021-09-30: we do not raise an exception because this association
				// is not critical for the account
				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
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

				string errorMessage = std::format("Internal server error: {}", e.what());
				SPDLOG_ERROR(errorMessage);

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
			registrationRoot["userKey"] = userKey;
			registrationRoot["workspaceKey"] = workspaceKey;
			registrationRoot["confirmationCode"] = confirmationCode;

			string responseBody = JSONUtils::toString(registrationRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);

			pair<string, string> emailAddressAndName = _mmsEngineDBFacade->getUserDetails(userKey);

			string confirmationURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				confirmationURL += (":" + to_string(_guiPort));
			confirmationURL +=
				("/catramms/conf/yourWorkspaces.xhtml?confirmationRequested=true&confirmationUserKey=" + to_string(userKey) +
				 "&confirmationCode=" + confirmationCode);

			SPDLOG_INFO(
				"Created Workspace code"
				", workspaceKey: {}"
				", userKey: {}"
				", confirmationCode: {}"
				", confirmationURL: {}",
				workspaceKey, userKey, confirmationCode, confirmationURL
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
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", requestBody: {}"
				", e.what(): {}",
				api, requestBody, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
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

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::shareWorkspace_(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "shareWorkspace";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, workspace->_workspaceKey, requestBody
	);

	try
	{
		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson(requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestBody
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}

		vector<string> mandatoryFields = {
			"email",
			"createRemoveWorkspace",
			"ingestWorkflow",
			"createProfiles",
			"deliveryAuthorization",
			"shareWorkspace",
			"editMedia",
			"editConfiguration",
			"killEncoding",
			"cancelIngestionJob",
			"editEncodersPool",
			"applicationRecorder",
			"createRemoveLiveChannel"
		};
		for (string field : mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				string errorMessage = std::format("Json field is not present or it is null", ", Json field: {}", field);
				SPDLOG_ERROR(errorMessage);

				sendError(request, 500, errorMessage);

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
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

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
				pair<int64_t, string> userDetails = _mmsEngineDBFacade->getUserDetailsByEmail(email, warningIfError);
				tie(userKey, name) = userDetails;

				userAlreadyPresent = true;
			}
			catch (runtime_error &e)
			{
				SPDLOG_WARN(
					"API failed"
					", API: {}"
					", requestBody: {}"
					", e.what(): {}",
					api, requestBody, e.what()
				);

				userAlreadyPresent = false;
			}
		}

		if (!userAlreadyPresent && !_registerUserEnabled)
		{
			string errorMessage = "registerUser is not enabled";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}

		bool createRemoveWorkspace = JSONUtils::asBool(metadataRoot, "createRemoveWorkspace", false);
		bool ingestWorkflow = JSONUtils::asBool(metadataRoot, "ingestWorkflow", false);
		bool createProfiles = JSONUtils::asBool(metadataRoot, "createProfiles", false);
		bool deliveryAuthorization = JSONUtils::asBool(metadataRoot, "deliveryAuthorization", false);
		bool shareWorkspace = JSONUtils::asBool(metadataRoot, "shareWorkspace", false);
		bool editMedia = JSONUtils::asBool(metadataRoot, "editMedia", false);
		bool editConfiguration = JSONUtils::asBool(metadataRoot, "editConfiguration", false);
		bool killEncoding = JSONUtils::asBool(metadataRoot, "killEncoding", false);
		bool cancelIngestionJob = JSONUtils::asBool(metadataRoot, "cancelIngestionJob", false);
		bool editEncodersPool = JSONUtils::asBool(metadataRoot, "editEncodersPool", false);
		bool applicationRecorder = JSONUtils::asBool(metadataRoot, "applicationRecorder", false);
		bool createRemoveLiveChannel = JSONUtils::asBool(metadataRoot, "createRemoveLiveChannel", false);

		try
		{
			SPDLOG_INFO(
				"Sharing workspace"
				", userAlreadyPresent: {}"
				", email: {}",
				userAlreadyPresent, email
			);

			if (userAlreadyPresent)
			{
				bool admin = false;

				SPDLOG_INFO(
					"createdCode"
					", workspace->_workspaceKey: {}"
					", userKey: {}"
					", email: {}"
					", codeType: {}",
					workspace->_workspaceKey, userKey, email,
					MMSEngineDBFacade::toString(MMSEngineDBFacade::CodeType::UserRegistrationComingFromShareWorkspace)
				);

				string shareWorkspaceCode = _mmsEngineDBFacade->createCode(
					workspace->_workspaceKey, userKey, email, MMSEngineDBFacade::CodeType::UserRegistrationComingFromShareWorkspace, admin,
					createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization, shareWorkspace, editMedia, editConfiguration,
					killEncoding, cancelIngestionJob, editEncodersPool, applicationRecorder, createRemoveLiveChannel
				);

				string confirmationURL = _guiProtocol + "://" + _guiHostname;
				if (_guiProtocol == "https" && _guiPort != 443)
					confirmationURL += (":" + to_string(_guiPort));
				confirmationURL +=
					("/catramms/login.xhtml?confirmationRequested=true&confirmationUserKey=" + to_string(userKey) +
					 "&confirmationCode=" + shareWorkspaceCode);

				SPDLOG_INFO(
					"Created Shared Workspace code"
					", workspace->_workspaceKey: {}"
					", email: {}"
					", userKey: {}"
					", confirmationCode: {}"
					", confirmationURL: {}",
					workspace->_workspaceKey, email, userKey, shareWorkspaceCode, confirmationURL
				);

				json registrationRoot;
				registrationRoot["userKey"] = userKey;
				registrationRoot["confirmationCode"] = shareWorkspaceCode;

				string responseBody = JSONUtils::toString(registrationRoot);

				sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);

				string tosCommaSeparated = email;
				string subject = "Share Workspace code";

				vector<string> emailBody;
				emailBody.push_back(string("<p>Dear ") + name + ",</p>");
				emailBody.push_back(std::format("<p>&emsp;&emsp;&emsp;&emsp;the '{}' workspace has been shared successfully</p>", workspace->_name));
				emailBody.push_back(
					string("<p>&emsp;&emsp;&emsp;&emsp;Here follows the user key <b>") + to_string(userKey) + "</b> and the confirmation code <b>" +
					shareWorkspaceCode + "</b> to be used to confirm the sharing of the Workspace</p>"
				);
				emailBody.push_back(
					string("<p>&emsp;&emsp;&emsp;&emsp;<b>Please click <a href=\"") + confirmationURL +
					"\">here</a> to confirm the registration</b></p>"
				);
				emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
				emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

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

				SPDLOG_INFO(
					"createdCode"
					", workspace->_workspaceKey: "
					", userKey: -1"
					", email: {}"
					", codeType: {}",
					workspace->_workspaceKey, email, MMSEngineDBFacade::toString(MMSEngineDBFacade::CodeType::ShareWorkspace)
				);

				string shareWorkspaceCode = _mmsEngineDBFacade->createCode(
					workspace->_workspaceKey, -1, email, MMSEngineDBFacade::CodeType::ShareWorkspace, admin, createRemoveWorkspace, ingestWorkflow,
					createProfiles, deliveryAuthorization, shareWorkspace, editMedia, editConfiguration, killEncoding, cancelIngestionJob,
					editEncodersPool, applicationRecorder, createRemoveLiveChannel
				);

				string shareWorkspaceURL = _guiProtocol + "://" + _guiHostname;
				if (_guiProtocol == "https" && _guiPort != 443)
					shareWorkspaceURL += (":" + to_string(_guiPort));
				shareWorkspaceURL += ("/catramms/login.xhtml?shareWorkspaceRequested=true&shareWorkspaceCode=" + shareWorkspaceCode);
				shareWorkspaceURL += ("&registrationEMail=" + email);

				SPDLOG_INFO(
					"Created Shared Workspace code"
					", workspace->_workspaceKey: {}"
					", email: {}"
					", shareWorkspaceCode: {}"
					", shareWorkspaceURL: {}",
					workspace->_workspaceKey, email, shareWorkspaceCode, shareWorkspaceURL
				);

				json registrationRoot;
				registrationRoot["shareWorkspaceCode"] = shareWorkspaceCode;

				string responseBody = JSONUtils::toString(registrationRoot);

				sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);

				string tosCommaSeparated = email;
				string subject = "Share Workspace code";

				vector<string> emailBody;
				emailBody.push_back(string("<p>Dear ") + email + ",</p>");
				emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;the <b>" + workspace->_name + "</b> workspace was shared with you</p>"));
				emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;Here follows the share workspace code <b>") + shareWorkspaceCode + "</b></p>");
				emailBody.push_back(
					string("<p>&emsp;&emsp;&emsp;&emsp;<b>Please click <a href=\"") + shareWorkspaceURL +
					"\">here</a> to continue with the registration</b></p>"
				);
				emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
				emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

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
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", requestBody: {}"
				", e.what(): {}",
				api, requestBody, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
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

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::workspaceList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, bool admin
)
{
	string api = "workspaceList";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, workspace->_workspaceKey
	);

	try
	{
		bool costDetails = false;
		auto costDetailsIt = queryParameters.find("costDetails");
		if (costDetailsIt != queryParameters.end() && costDetailsIt->second == "true")
			costDetails = true;

		{
			json workspaceListRoot = _mmsEngineDBFacade->getWorkspaceList(userKey, admin, costDetails);

			string responseBody = JSONUtils::toString(workspaceListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::confirmRegistration(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, unordered_map<string, string> queryParameters
)
{
	string api = "confirmRegistration";

	SPDLOG_INFO("Received {}", api);

	try
	{
		auto confirmationCodeIt = queryParameters.find("confirmationeCode");
		if (confirmationCodeIt == queryParameters.end())
		{
			string errorMessage = "The 'confirmationeCode' parameter is not found";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			tuple<string, string, string> apiKeyNameAndEmailAddress =
				_mmsEngineDBFacade->confirmRegistration(confirmationCodeIt->second, _expirationInDaysWorkspaceDefaultValue);

			string apiKey;
			string name;
			string emailAddress;

			tie(apiKey, name, emailAddress) = apiKeyNameAndEmailAddress;

			json registrationRoot;
			registrationRoot["apiKey"] = apiKey;

			string responseBody = JSONUtils::toString(registrationRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);

			string tosCommaSeparated = emailAddress;
			string subject = "Welcome";

			string loginURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				loginURL += (":" + to_string(_guiPort));
			loginURL += "/catramms/login.xhtml?confirmationRequested=false";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Dear ") + name + ",</p>");
			emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;Thank you for choosing the CatraMMS services</p>"));
			emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;Your registration is now completed and you can enjoy working with MMS</p>"));
			emailBody.push_back(
				string("<p>&emsp;&emsp;&emsp;&emsp;<b>Please click <a href=\"") + loginURL + "\">here</a> to login into the MMS platform</b></p>"
			);
			emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;Best regards</p>");
			emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

			CurlWrapper::sendEmail(
				_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
				_emailUserName,	   // i.e.: info@catramms-cloud.com
				_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
			);
			// EMailSender emailSender(_logger, _configuration);
			// bool useMMSCCToo = true;
			// emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
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
			", e.what(): {}",
			api, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::login(string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestBody)
{
	string api = "login";

	SPDLOG_INFO(
		"Received {}",
		// commented because of the password
		// ", requestBody: {}",
		api
	);

	try
	{
		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson(requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestBody
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

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
					if (!JSONUtils::isMetadataPresent(metadataRoot, field))
					{
						string errorMessage = std::format(
							"Json field is not present or it is null"
							", Json field: {}",
							field
						);
						SPDLOG_ERROR(errorMessage);

						sendError(request, 400, errorMessage);

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
					SPDLOG_INFO(
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

					SPDLOG_INFO(
						"Login User"
						", userKey: {}"
						", email: {}",
						userKey, email
					);
				}
				catch (LoginFailed &e)
				{
					SPDLOG_ERROR(
						"API failed"
						", API: {}"
						", requestBody: {}"
						", e.what(): {}",
						api, requestBody, e.what()
					);

					string errorMessage = e.what();
					SPDLOG_ERROR(errorMessage);

					sendError(request, 401, errorMessage); // unauthorized

					throw runtime_error(errorMessage);
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

					string errorMessage = std::format("Internal server error: {}", e.what());
					SPDLOG_ERROR(errorMessage);

					sendError(request, 500, errorMessage);

					throw runtime_error(errorMessage);
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

					string errorMessage = std::format("Internal server error: {}", e.what());
					SPDLOG_ERROR(errorMessage);

					sendError(request, 500, errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else // if (_ldapEnabled)
			{
				vector<string> mandatoryFields = {"name", "password"};
				for (string field : mandatoryFields)
				{
					if (!JSONUtils::isMetadataPresent(metadataRoot, field))
					{
						string errorMessage = std::format("Json field is not present or it is null", ", Json field: {}", field);
						SPDLOG_ERROR(errorMessage);

						sendError(request, 400, errorMessage);

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
					SPDLOG_INFO(
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
								SPDLOG_INFO(
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
								SPDLOG_ERROR(
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
						SPDLOG_ERROR(
							"ldap Login failed"
							", userName: {}",
							userName
						);

						throw LoginFailed();
					}

					bool userAlreadyRegistered;
					try
					{
						SPDLOG_INFO(
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

						SPDLOG_INFO(
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
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							"API failed"
							", API: {}"
							", requestBody: {}"
							", e.what(): {}",
							api, requestBody, e.what()
						);

						string errorMessage = std::format("Internal server error: {}", e.what());
						SPDLOG_ERROR(errorMessage);

						sendError(request, 500, errorMessage);

						throw runtime_error(errorMessage);
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

						string errorMessage = std::format("Internal server error: {}", e.what());
						SPDLOG_ERROR(errorMessage);

						sendError(request, 500, errorMessage);

						throw runtime_error(errorMessage);
					}

					if (!userAlreadyRegistered)
					{
						SPDLOG_INFO(
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
						pair<int64_t, string> userKeyAndEmail = _mmsEngineDBFacade->registerActiveDirectoryUser(
							userName, email,
							string(""), // userCountry,
							string("CET"), createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization, shareWorkspace, editMedia,
							editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool, applicationRecorder, createRemoveLiveChannel,
							_ldapDefaultWorkspaceKeys, _expirationInDaysWorkspaceDefaultValue,
							chrono::system_clock::now() + chrono::hours(24 * 365 * 10)
							// chrono::system_clock::time_point userExpirationDate
						);

						string apiKey;
						tie(userKey, apiKey) = userKeyAndEmail;

						SPDLOG_INFO(
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

						string field = "ldapEnabled";
						loginDetailsRoot[field] = _ldapEnabled;

						field = "userKey";
						userKey = JSONUtils::asInt64(loginDetailsRoot, field, 0);

						SPDLOG_INFO(
							"Login User"
							", userKey: {}"
							", email: {}",
							userKey, email
						);
					}
				}
				catch (LoginFailed &e)
				{
					SPDLOG_ERROR(
						"API failed"
						", API: {}"
						", requestBody: {}"
						", e.what(): {}",
						api, requestBody, e.what()
					);

					string errorMessage = e.what();
					SPDLOG_ERROR(errorMessage);

					sendError(request, 401, errorMessage); // unauthorized

					throw runtime_error(errorMessage);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"API failed"
						", API: {}"
						", ldapURL: {}"
						", ldapCertificatePathName: {}"
						", ldapManagerUserName: {}"
						", e.what(): {}",
						api, _ldapURL, _ldapCertificatePathName, _ldapManagerUserName, e.what()
					);

					string errorMessage = std::format("Internal server error: {}", e.what());
					SPDLOG_ERROR(errorMessage);

					sendError(request, 500, errorMessage);

					throw runtime_error(errorMessage);
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

					string errorMessage = std::format("Internal server error: {}", e.what());
					SPDLOG_ERROR(errorMessage);

					sendError(request, 500, errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}

		if (remoteClientIPAddress != "")
		{
			try
			{
				SPDLOG_INFO(
					"_mmsEngineDBFacade->saveLoginStatistics"
					", userKey: {}"
					", remoteClientIPAddress: {}",
					userKey, remoteClientIPAddress
				);
				_mmsEngineDBFacade->saveLoginStatistics(userKey, remoteClientIPAddress);
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					"Saving Login Statistics failed"
					", userKey: {}"
					", remoteClientIPAddress: {}"
					", e.what(): {}",
					userKey, remoteClientIPAddress, e.what()
				);

				// string errorMessage = string("Internal server error: ") + e.what();
				// SPDLOG_ERROR(errorMessage);

				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					"Saving Login Statistics failed"
					", userKey: {}"
					", remoteClientIPAddress: {}"
					", e.what(): {}",
					userKey, remoteClientIPAddress, e.what()
				);

				// string errorMessage = string("Internal server error");
				// SPDLOG_ERROR(errorMessage);

				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		try
		{
			SPDLOG_INFO(
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

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
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

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::updateUser(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey, string requestBody, bool admin
)
{
	string api = "updateUser";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
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
			metadataRoot = JSONUtils::toJson(requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestBody
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

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
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				name = JSONUtils::asString(metadataRoot, field, "");
				nameChanged = true;
			}

			field = "email";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
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
						", email: {}",
						email
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				emailChanged = true;
			}

			field = "country";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				country = JSONUtils::asString(metadataRoot, field, "");
				countryChanged = true;
			}

			field = "timezone";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				timezone = JSONUtils::asString(metadataRoot, field, "CET");
				timezoneChanged = true;
			}

			if (admin)
			{
				field = "insolvent";
				if (JSONUtils::isMetadataPresent(metadataRoot, field))
				{
					insolvent = JSONUtils::asBool(metadataRoot, field, false);
					insolventChanged = true;
				}
			}

			if (admin)
			{
				field = "expirationDate";
				if (JSONUtils::isMetadataPresent(metadataRoot, field))
				{
					expirationUtcDate = JSONUtils::asString(metadataRoot, field, "");
					expirationDateChanged = true;
				}
			}

			if (JSONUtils::isMetadataPresent(metadataRoot, "newPassword") && JSONUtils::isMetadataPresent(metadataRoot, "oldPassword"))
			{
				passwordChanged = true;
				newPassword = JSONUtils::asString(metadataRoot, "newPassword", "");
				oldPassword = JSONUtils::asString(metadataRoot, "oldPassword", "");
			}
		}
		else
		{
			string field = "country";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				country = JSONUtils::asString(metadataRoot, field, "");
				countryChanged = true;
			}

			field = "timezone";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				timezone = JSONUtils::asString(metadataRoot, field, "CET");
				timezoneChanged = true;
			}
		}

		try
		{
			SPDLOG_INFO(
				"Updating User"
				", userKey: {}"
				", name: {}"
				", email: {}",
				userKey, name, email
			);

			json loginDetailsRoot = _mmsEngineDBFacade->updateUser(
				admin, _ldapEnabled, userKey, nameChanged, name, emailChanged, email, countryChanged, country, timezoneChanged, timezone,
				insolventChanged, insolvent, expirationDateChanged, expirationUtcDate, passwordChanged, newPassword, oldPassword
			);

			SPDLOG_INFO(
				"User updated"
				", userKey: {}"
				", name: {}"
				", email: {}",
				userKey, name, email
			);

			string responseBody = JSONUtils::toString(loginDetailsRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
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

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::createTokenToResetPassword(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, unordered_map<string, string> queryParameters
)
{
	string api = "createTokenToResetPassword";

	SPDLOG_INFO("Received {}", api);

	try
	{
		string email;

		auto emailIt = queryParameters.find("email");
		if (emailIt == queryParameters.end())
		{
			string errorMessage = "The 'email' parameter is not found";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		{
			email = emailIt->second;
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(email, regex(plus), plusDecoded);

			email = CurlWrapper::unescape(firstDecoding);
		}

		string resetPasswordToken;
		string name;
		try
		{
			SPDLOG_INFO(
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
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, "");

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
			emailBody.push_back(string("In case you did not request any reset of your password, just ignore this email</p>"));

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
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
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
			", e.what(): {}",
			api, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::resetPassword(string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestBody)
{
	string api = "resetPassword";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		string newPassword;
		string resetPasswordToken;

		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson(requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = fmt::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestBody
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		resetPasswordToken = JSONUtils::asString(metadataRoot, "resetPasswordToken", "");
		if (resetPasswordToken == "")
		{
			string errorMessage = "The 'resetPasswordToken' parameter is not found";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		newPassword = JSONUtils::asString(metadataRoot, "newPassword", "");
		if (newPassword == "")
		{
			string errorMessage = "The 'newPassword' parameter is not found";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		string name;
		string email;
		try
		{
			SPDLOG_INFO(
				"Reset Password"
				", resetPasswordToken: {}",
				resetPasswordToken
			);

			pair<string, string> details = _mmsEngineDBFacade->resetPassword(resetPasswordToken, newPassword);
			name = details.first;
			email = details.second;
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

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, "");

			string tosCommaSeparated = email;
			string subject = "Reset password";

			vector<string> emailBody;
			emailBody.push_back(string("<p>Dear ") + name + ",</p>");
			emailBody.push_back(string("your password has been changed.</p>"));

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
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", requestBody: {}"
				", e.what(): {}",
				api, requestBody, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
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

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::updateWorkspace(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace, int64_t userKey,
	string requestBody
)
{
	string api = "updateWorkspace";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, workspace->_workspaceKey, requestBody
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

		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson(requestBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"Json metadata failed during the parsing"
				", json data: {}",
				requestBody
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}

		string field = "enabled";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			enabledChanged = true;
			newEnabled = JSONUtils::asBool(metadataRoot, field, false);
		}

		field = "workspaceName";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			nameChanged = true;
			newName = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "workspaceNotes";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			notesChanged = true;
			newNotes = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "maxEncodingPriority";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			maxEncodingPriorityChanged = true;
			newMaxEncodingPriority = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "encodingPeriod";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			encodingPeriodChanged = true;
			newEncodingPeriod = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "maxIngestionsNumber";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			maxIngestionsNumberChanged = true;
			newMaxIngestionsNumber = JSONUtils::asInt64(metadataRoot, field, 0);
		}

		field = "languageCode";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			languageCodeChanged = true;
			newLanguageCode = JSONUtils::asString(metadataRoot, field, "");
		}

		field = "timezone";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			timezoneChanged = true;
			newTimezone = JSONUtils::asString(metadataRoot, field, "CET");
		}

		if (JSONUtils::isMetadataPresent(metadataRoot, "preferences"))
		{
			preferencesChanged = true;
			newPreferences = JSONUtils::asString(metadataRoot, "preferences", "");
		}

		field = "maxStorageInGB";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			maxStorageInGBChanged = true;
			maxStorageInGB = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForStorage";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			currentCostForStorageChanged = true;
			currentCostForStorage = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "dedicatedEncoder_power_1";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			dedicatedEncoder_power_1Changed = true;
			dedicatedEncoder_power_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForDedicatedEncoder_power_1";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			currentCostForDedicatedEncoder_power_1Changed = true;
			currentCostForDedicatedEncoder_power_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "dedicatedEncoder_power_2";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			dedicatedEncoder_power_2Changed = true;
			dedicatedEncoder_power_2 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForDedicatedEncoder_power_2";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			currentCostForDedicatedEncoder_power_2Changed = true;
			currentCostForDedicatedEncoder_power_2 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "dedicatedEncoder_power_3";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			dedicatedEncoder_power_3Changed = true;
			dedicatedEncoder_power_3 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForDedicatedEncoder_power_3";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			currentCostForDedicatedEncoder_power_3Changed = true;
			currentCostForDedicatedEncoder_power_3 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "CDN_type_1";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			CDN_type_1Changed = true;
			CDN_type_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "currentCostForCDN_type_1";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			currentCostForCDN_type_1Changed = true;
			currentCostForCDN_type_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "support_type_1";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			support_type_1Changed = true;
			support_type_1 = JSONUtils::asBool(metadataRoot, field, false);
		}

		field = "currentCostForSupport_type_1";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			currentCostForSupport_type_1Changed = true;
			currentCostForSupport_type_1 = JSONUtils::asInt64(metadataRoot, field, -1);
		}

		field = "userAPIKey";
		if (JSONUtils::isMetadataPresent(metadataRoot, field))
		{
			json userAPIKeyRoot = metadataRoot[field];

			{
				vector<string> mandatoryFields = {"createRemoveWorkspace", "ingestWorkflow",   "createProfiles",	  "deliveryAuthorization",
												  "shareWorkspace",		   "editMedia",		   "editConfiguration",	  "killEncoding",
												  "cancelIngestionJob",	   "editEncodersPool", "applicationRecorder", "createRemoveLiveChannel"};
				for (string field : mandatoryFields)
				{
					if (!JSONUtils::isMetadataPresent(userAPIKeyRoot, field))
					{
						string errorMessage = std::format(
							"Json field is not present or it is null"
							", Json field: {}",
							field
						);
						SPDLOG_ERROR(errorMessage);

						sendError(request, 400, errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}

			field = "expirationDate";
			if (JSONUtils::isMetadataPresent(userAPIKeyRoot, field))
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
		}

		try
		{
			SPDLOG_INFO(
				"Updating WorkspaceDetails"
				", userKey: {}"
				", workspaceKey: {}",
				userKey, workspace->_workspaceKey
			);

#ifdef __POSTGRES__
			json workspaceDetailRoot = _mmsEngineDBFacade->updateWorkspaceDetails(
				userKey, workspace->_workspaceKey, notesChanged, newNotes, enabledChanged, newEnabled, nameChanged, newName,
				maxEncodingPriorityChanged, newMaxEncodingPriority, encodingPeriodChanged, newEncodingPeriod, maxIngestionsNumberChanged,
				newMaxIngestionsNumber, languageCodeChanged, newLanguageCode, timezoneChanged, newTimezone, preferencesChanged, newPreferences,
				expirationDateChanged, newExpirationUtcDate,

				maxStorageInGBChanged, maxStorageInGB, currentCostForStorageChanged, currentCostForStorage, dedicatedEncoder_power_1Changed,
				dedicatedEncoder_power_1, currentCostForDedicatedEncoder_power_1Changed, currentCostForDedicatedEncoder_power_1,
				dedicatedEncoder_power_2Changed, dedicatedEncoder_power_2, currentCostForDedicatedEncoder_power_2Changed,
				currentCostForDedicatedEncoder_power_2, dedicatedEncoder_power_3Changed, dedicatedEncoder_power_3,
				currentCostForDedicatedEncoder_power_3Changed, currentCostForDedicatedEncoder_power_3, CDN_type_1Changed, CDN_type_1,
				currentCostForCDN_type_1Changed, currentCostForCDN_type_1, support_type_1Changed, support_type_1, currentCostForSupport_type_1Changed,
				currentCostForSupport_type_1,

				newCreateRemoveWorkspace, newIngestWorkflow, newCreateProfiles, newDeliveryAuthorization, newShareWorkspace, newEditMedia,
				newEditConfiguration, newKillEncoding, newCancelIngestionJob, newEditEncodersPool, newApplicationRecorder, newCreateRemoveLiveChannel
			);
#else
			bool maxStorageInMBChanged = false;
			int64_t newMaxStorageInMB = 0;
			json workspaceDetailRoot = _mmsEngineDBFacade->updateWorkspaceDetails(
				userKey, workspace->_workspaceKey, enabledChanged, newEnabled, nameChanged, newName, maxEncodingPriorityChanged,
				newMaxEncodingPriority, encodingPeriodChanged, newEncodingPeriod, maxIngestionsNumberChanged, newMaxIngestionsNumber,
				maxStorageInMBChanged, newMaxStorageInMB, languageCodeChanged, newLanguageCode, expirationDateChanged, newExpirationUtcDate,
				newCreateRemoveWorkspace, newIngestWorkflow, newCreateProfiles, newDeliveryAuthorization, newShareWorkspace, newEditMedia,
				newEditConfiguration, newKillEncoding, newCancelIngestionJob, newEditEncodersPool, newApplicationRecorder
			);
#endif

			SPDLOG_INFO(
				"WorkspaceDetails updated"
				", userKey: {}"
				", workspaceKey: {}",
				userKey, workspace->_workspaceKey
			);

			string responseBody = JSONUtils::toString(workspaceDetailRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
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

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::setWorkspaceAsDefault(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace, int64_t userKey,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "setWorkspaceAsDefault";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, workspace->_workspaceKey, requestBody
	);

	try
	{
		int64_t workspaceKeyToBeSetAsDefault = -1;
		auto workspaceKeyToBeSetAsDefaultIt = queryParameters.find("workspaceKeyToBeSetAsDefault");
		if (workspaceKeyToBeSetAsDefaultIt == queryParameters.end() || workspaceKeyToBeSetAsDefaultIt->second == "")
		{
			string errorMessage = "The 'workspaceKeyToBeSetAsDefault' parameter is not found";
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		workspaceKeyToBeSetAsDefault = stoll(workspaceKeyToBeSetAsDefaultIt->second);

		try
		{
			SPDLOG_INFO(
				"setWorkspaceAsDefault"
				", userKey: {}"
				", workspaceKey: {}",
				userKey, workspace->_workspaceKey
			);

			_mmsEngineDBFacade->setWorkspaceAsDefault(userKey, workspace->_workspaceKey, workspaceKeyToBeSetAsDefault);

			string responseBody;

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
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

			string errorMessage = std::format("Internal server error: {}", e.what());
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

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::deleteWorkspace(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey, shared_ptr<Workspace> workspace
)
{
	string api = "deleteWorkspace";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", userKey: {}",
		api, workspace->_workspaceKey, userKey
	);

	try
	{
		if (_noFileSystemAccess)
		{
			SPDLOG_ERROR(
				"{} failed, no rights to execute this method"
				", _noFileSystemAccess: {}",
				api, _noFileSystemAccess
			);

			string errorMessage = std::format("Internal server error: no rights to execute this method");
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			SPDLOG_INFO(
				"Delete Workspace from DB"
				", userKey: {}"
				", workspaceKey: {}",
				userKey, workspace->_workspaceKey
			);

			// ritorna gli utenti eliminati perchè avevano solamente il workspace che è stato rimosso
			vector<tuple<int64_t, string, string>> usersRemoved = _mmsEngineDBFacade->deleteWorkspace(userKey, workspace->_workspaceKey);

			SPDLOG_INFO(
				"Workspace from DB deleted"
				", userKey: {}"
				", workspaceKey: {}",
				userKey, workspace->_workspaceKey
			);

			if (usersRemoved.size() > 0)
			{
				for (tuple<int64_t, string, string> userDetails : usersRemoved)
				{
					string name;
					string eMailAddress;

					tie(ignore, name, eMailAddress) = userDetails;

					string tosCommaSeparated = eMailAddress;
					string subject = "Your account was removed";

					vector<string> emailBody;
					emailBody.push_back(string("<p>Dear ") + name + ",</p>");
					emailBody.push_back(string(
						"<p>&emsp;&emsp;&emsp;&emsp;your account was removed because the only workspace you had (" + workspace->_name +
						") was removed and</p>"
					));
					emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;your account remained without any workspace.</p>"));
					emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;If you still need the CatraMMS services, please register yourself again<b>"
					));
					emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
					emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

					CurlWrapper::sendEmail(
						_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
						_emailUserName,	   // i.e.: info@catramms-cloud.com
						_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
					);
				}
			}
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			SPDLOG_INFO(
				"Delete Workspace from Storage"
				", userKey: {}"
				", workspaceKey: {}",
				userKey, workspace->_workspaceKey
			);

			_mmsStorage->deleteWorkspace(workspace);

			SPDLOG_INFO(
				"Workspace from Storage deleted"
				", workspaceKey: {}",
				workspace->_workspaceKey
			);

			string responseBody;
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
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
			", userKey: {}"
			", workspace->_workspaceKey: {}"
			", e.what(): {}",
			api, userKey, workspace->_workspaceKey, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", userKey: {}"
			", workspace->_workspaceKey: {}"
			", e.what(): {}",
			api, userKey, workspace->_workspaceKey, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::unshareWorkspace(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey, shared_ptr<Workspace> workspace
)
{
	string api = "unshareWorkspace";

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", userKey: {}",
		api, workspace->_workspaceKey, userKey
	);

	try
	{
		try
		{
			SPDLOG_INFO(
				"Unshare Workspace"
				", userKey: {}"
				", workspaceKey: {}",
				userKey, workspace->_workspaceKey
			);

			// ritorna gli utenti eliminati perchè avevano solamente il workspace che è stato unshared
			auto [userToBeRemoved, name, eMailAddress] = _mmsEngineDBFacade->unshareWorkspace(userKey, workspace->_workspaceKey);

			SPDLOG_INFO(
				"Workspace from DB unshared"
				", userKey: {}"
				", workspaceKey: {}",
				userKey, workspace->_workspaceKey
			);

			if (userToBeRemoved)
			{
				string tosCommaSeparated = eMailAddress;
				string subject = "Your account was removed";

				vector<string> emailBody;
				emailBody.push_back(string("<p>Dear ") + name + ",</p>");
				emailBody.push_back(string(
					"<p>&emsp;&emsp;&emsp;&emsp;your account was removed because the only workspace you had (" + workspace->_name +
					") was unshared and</p>"
				));
				emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;your account remained without any workspace.</p>"));
				emailBody.push_back(string("<p>&emsp;&emsp;&emsp;&emsp;If you still need the CatraMMS services, please register yourself again<b>"));
				emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;Have a nice day, best regards</p>");
				emailBody.push_back("<p>&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;MMS technical support</p>");

				CurlWrapper::sendEmail(
					_emailProviderURL, // i.e.: smtps://smtppro.zoho.eu:465
					_emailUserName,	   // i.e.: info@catramms-cloud.com
					_emailPassword, _emailUserName, tosCommaSeparated, _emailCcsCommaSeparated, subject, emailBody, "text/html; charset=\"UTF-8\""
				);
			}

			string responseBody;
			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"API failed"
				", API: {}"
				", e.what(): {}",
				api, e.what()
			);

			string errorMessage = std::format("Internal server error: {}", e.what());
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
			", userKey: {}"
			", workspace->_workspaceKey: {}"
			", e.what(): {}",
			api, userKey, workspace->_workspaceKey, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", userKey: {}"
			", workspace->_workspaceKey: {}"
			", e.what(): {}",
			api, userKey, workspace->_workspaceKey, e.what()
		);

		string errorMessage = std::format("Internal server error: {}", e.what());
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::workspaceUsage(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace
)
{
	json workspaceUsageRoot;

	string api = "workspaceUsage";

	try
	{
		string field;

		{
			json requestParametersRoot;

			field = "requestParameters";
			workspaceUsageRoot[field] = requestParametersRoot;
		}

		json responseRoot;
		{
			int64_t workSpaceUsageInBytes;

			pair<int64_t, int64_t> workSpaceUsageInBytesAndMaxStorageInMB = _mmsEngineDBFacade->getWorkspaceUsage(workspace->_workspaceKey);
			tie(workSpaceUsageInBytes, ignore) = workSpaceUsageInBytesAndMaxStorageInMB;

			int64_t workSpaceUsageInMB = workSpaceUsageInBytes / 1000000;

			field = "usageInMB";
			responseRoot[field] = workSpaceUsageInMB;
		}

		field = "response";
		workspaceUsageRoot[field] = responseRoot;

		string responseBody = JSONUtils::toString(workspaceUsageRoot);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"getWorkspaceUsage exception"
			", e.what(): {}",
			e.what()
		);

		sendError(request, 500, e.what());

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"getWorkspaceUsage exception"
			", e.what(): {}",
			e.what()
		);

		sendError(request, 500, e.what());

		throw e;
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
		SPDLOG_ERROR(errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	*/

	// controlli:
	// L'indirizzo contiene almeno un @
	// Il local-part(tutto a sinistra dell'estrema destra @) non è vuoto
	// La parte domain (tutto a destra dell'estrema destra @) contiene almeno un punto (di nuovo, questo non è strettamente vero, ma
	// pragmatico)

	size_t endOfLocalPartIndex = email.find_last_of("@");
	if (endOfLocalPartIndex == string::npos)
	{
		string errorMessage = std::format(
			"Wrong email format"
			", email: {}",
			email
		);
		SPDLOG_ERROR(errorMessage);

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
	if (localPart == "" || domainPart.find(".") == string::npos)
	{
		string errorMessage = std::format(
			"Wrong email format"
			", email: {}",
			email
		);
		SPDLOG_ERROR(errorMessage);

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
