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

/*
#include <fstream>
#include <sstream>
#include <regex>
#include "catralibraries/Convert.h"
#include "Validator.h"
*/
#include "JSONUtils.h"
#include "catralibraries/LdapWrapper.h"
#include "EMailSender.h"
#include "API.h"
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <iterator>
#include <vector>


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
                "EMail",
                "Password"
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

            email = metadataRoot.get("EMail", "").asString();
            password = metadataRoot.get("Password", "").asString();
            workspaceName = metadataRoot.get("WorkspaceName", "").asString();
            name = metadataRoot.get("Name", "").asString();
            country = metadataRoot.get("Country", "").asString();

			if (workspaceName == "")
			{
				if (name != "")
					workspaceName = name;
				else
					workspaceName = email;
			}
			if (name == "")
				workspaceName = email;
        }

        encodingPriority = _encodingPriorityWorkspaceDefaultValue;
        /*
        // encodingPriority
        {
            string field = "EncodingPriority";
            if (!JSONUtils::isMetadataPresent(metadataRoot, field))
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
            if (!JSONUtils::isMetadataPresent(metadataRoot, field))
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
            if (!JSONUtils::isMetadataPresent(metadataRoot, field))
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
            if (!JSONUtils::isMetadataPresent(metadataRoot, field))
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

		int64_t workspaceKey;
		int64_t userKey;
		string confirmationCode;
        try
        {
            _logger->info(__FILEREF__ + "Registering User"
                + ", workspaceName: " + workspaceName
                + ", email: " + email
            );

			tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode
				= _mmsEngineDBFacade->registerUserAndAddWorkspace(
                    name, 
                    email, 
                    password,
                    country, 
                    workspaceName,
                    MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,  // MMSEngineDBFacade::WorkspaceType workspaceType
                    "",                             // string deliveryURL,
                    encodingPriority,               //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
                    encodingPeriod,                 //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
                    maxIngestionsNumber,            // long maxIngestionsNumber,
                    maxStorageInMB,                 // long maxStorageInMB,
                    "",                             // string languageCode,
                    chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
                );

			workspaceKey = get<0>(workspaceKeyUserKeyAndConfirmationCode);
			userKey = get<1>(workspaceKeyUserKeyAndConfirmationCode);
			confirmationCode = get<2>(workspaceKeyUserKeyAndConfirmationCode);

            _logger->info(__FILEREF__ + "Registered User and added Workspace"
                + ", workspaceName: " + workspaceName
                + ", email: " + email
                + ", userKey: " + to_string(userKey)
                + ", confirmationCode: " + confirmationCode
            );
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

		try
		{
			_logger->info(__FILEREF__ + "Associate defaults encoders to the Workspace"
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", _sharedEncoderLabel: " + _sharedEncoderLabel
			);

			_mmsEngineDBFacade->addAssociationWorkspaceEncoder(workspaceKey,
				_sharedEncoderLabel);
		}
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
            _logger->error(__FILEREF__ + errorMessage);

			// 2021-09-30: we do not raise an exception because this association
			// is not critical for the account
            // sendError(request, 500, errorMessage);

            // throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

			// 2021-09-30: we do not raise an exception because this association
			// is not critical for the account
            // sendError(request, 500, errorMessage);

            // throw runtime_error(errorMessage);
        }

        try
        {
			Json::Value registrationRoot;
			registrationRoot["workspaceKey"] = workspaceKey;
			registrationRoot["userKey"] = userKey;
			registrationRoot["confirmationCode"] = confirmationCode;

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, registrationRoot);

            sendSuccess(request, 201, responseBody);
            
			string confirmationURL = _guiProtocol + "://" + _guiHostname;
			if (_guiProtocol == "https" && _guiPort != 443)
				confirmationURL += (":" + to_string(_guiPort));
			confirmationURL += ("/catramms/login.xhtml?confirmationRequested=true&confirmationUserKey="
				+ to_string(userKey)
				+ "&confirmationCode=" + confirmationCode);

            string to = email;
            string subject = "Confirmation code";
            
            vector<string> emailBody;
            emailBody.push_back(string("<p>Hi ") + name + ",</p>");
            emailBody.push_back(string("<p>the registration has been done successfully, user and default Workspace have been created</p>"));
            emailBody.push_back(string("<p>here follows the user key <b>") + to_string(userKey) 
                + "</b> and the confirmation code <b>" + confirmationCode + "</b> to be used to confirm the registration</p>");
            emailBody.push_back(
					string("<p>Please click <a href=\"")
					+ confirmationURL
					+ "\">here</a> to confirm the registration</p>");
            emailBody.push_back("<p>Have a nice day, best regards</p>");
            emailBody.push_back("<p>MMS technical support</p>");

            EMailSender emailSender(_logger, _configuration);
			bool useMMSCCToo = true;
            emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
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
        string requestBody,
		bool admin)
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
                if (!JSONUtils::isMetadataPresent(metadataRoot, field))
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
            _logger->info(__FILEREF__ + "Creating Workspace"
                + ", workspaceName: " + workspaceName
            );

            pair<int64_t,string> workspaceKeyAndConfirmationCode =
                    _mmsEngineDBFacade->createWorkspace(
                        userKey,
                        workspaceName,
                        MMSEngineDBFacade::WorkspaceType::IngestionAndDelivery,  // MMSEngineDBFacade::WorkspaceType workspaceType
                        "",     // string deliveryURL,
                        encodingPriority,               //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
                        encodingPeriod,                 //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
                        maxIngestionsNumber,            // long maxIngestionsNumber,
                        maxStorageInMB,                 // long maxStorageInMB,
                        "",                             // string languageCode,
						admin,
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
            string confirmURL = _apiProtocol + "://" + _apiHostname + ":" + to_string(_apiPort) + "/catramms/" + _apiVersion + "/user/" 
                    + to_string(userKey) + "/" + get<1>(workspaceKeyAndConfirmationCode);
            emailBody.push_back(string("<p>Click <a href=\"") + confirmURL + "\">here</a> to confirm the registration</p>");
            emailBody.push_back("<p>Have a nice day, best regards</p>");
            emailBody.push_back("<p>MMS technical support</p>");

            EMailSender emailSender(_logger, _configuration);
			bool useMMSCCToo = true;
            emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
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
		// In case of ActiveDirectory, userAlreadyPresent is always true
        bool userAlreadyPresent;
		if(_ldapEnabled)
		{
			userAlreadyPresent = true;
		}
		else
		{
			auto userAlreadyPresentIt = queryParameters.find("userAlreadyPresent");
			if (userAlreadyPresentIt == queryParameters.end())
			{
				string errorMessage = string("The 'userAlreadyPresent' parameter is not found");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				throw runtime_error(errorMessage);
			}
			userAlreadyPresent = (userAlreadyPresentIt->second == "true" ? true : false);
		}

        bool createRemoveWorkspace;
        auto createRemoveWorkspaceIt = queryParameters.find("createRemoveWorkspace");
        if (createRemoveWorkspaceIt == queryParameters.end())
        {
            string errorMessage = string("The 'createRemoveWorkspace' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        createRemoveWorkspace = (createRemoveWorkspaceIt->second == "true" ? true : false);

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

        bool editConfiguration;
        auto editConfigurationIt = queryParameters.find("editConfiguration");
        if (editConfigurationIt == queryParameters.end())
        {
            string errorMessage = string("The 'editConfiguration' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        editConfiguration = (editConfigurationIt->second == "true" ? true : false);

        bool killEncoding;
        auto killEncodingIt = queryParameters.find("killEncoding");
        if (killEncodingIt == queryParameters.end())
        {
            string errorMessage = string("The 'killEncoding' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        killEncoding = (killEncodingIt->second == "true" ? true : false);

        bool cancelIngestionJob;
        auto cancelIngestionJobIt = queryParameters.find("cancelIngestionJob");
        if (cancelIngestionJobIt == queryParameters.end())
        {
            string errorMessage = string("The 'cancelIngestionJob' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        cancelIngestionJob = (cancelIngestionJobIt->second == "true" ? true : false);

        bool editEncodersPool;
        auto editEncodersPoolIt = queryParameters.find("editEncodersPool");
        if (editEncodersPoolIt == queryParameters.end())
        {
            string errorMessage = string("The 'editEncodersPool' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        editEncodersPool = (editEncodersPoolIt->second == "true" ? true : false);

        bool applicationRecorder;
        auto applicationRecorderIt = queryParameters.find("applicationRecorder");
        if (applicationRecorderIt == queryParameters.end())
        {
            string errorMessage = string("The 'applicationRecorder' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }
        applicationRecorder = (applicationRecorderIt->second == "true" ? true : false);


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
                if (!JSONUtils::isMetadataPresent(metadataRoot, field))
                {
                    string errorMessage = string("Json field is not present or it is null")
                            + ", Json field: " + field;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }

            email = metadataRoot.get("EMail", "").asString();
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
                if (!JSONUtils::isMetadataPresent(metadataRoot, field))
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
						_ldapEnabled,
                    userAlreadyPresent,
                    name, 
                    email, 
                    password,
                    country, 
                    createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization, shareWorkspace,
					editMedia, editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
					applicationRecorder,
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
			bool useMMSCCToo = true;
            emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
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

void API::workspaceList(
        FCGX_Request& request,
		int64_t userKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
		bool admin)
{
    string api = "workspaceList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
		/*
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
			if (rows > _maxPageSize)
				rows = _maxPageSize;
        }
		*/

        {
			Json::Value workspaceListRoot = _mmsEngineDBFacade->getWorkspaceList(userKey, admin);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, workspaceListRoot);

            sendSuccess(request, 200, responseBody);
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
                = _mmsEngineDBFacade->confirmRegistration(confirmationCodeIt->second,
					_expirationInDaysWorkspaceDefaultValue);

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
			bool useMMSCCToo = true;
            emailSender.sendEmail(to, subject, emailBody, useMMSCCToo);
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
		// commented because of the password
        // + ", requestBody: " + requestBody
    );

    try
    {
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

		Json::Value loginDetailsRoot;
		int64_t userKey;
		string remoteClientIPAddress;

        {
			if (!_ldapEnabled)
			{
				vector<string> mandatoryFields = {
					"EMail",
					"Password"
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

				string field = "EMail";
				string email = metadataRoot.get(field, "").asString();

				field = "Password";
				string password = metadataRoot.get(field, "").asString();

				field = "RemoteClientIPAddress";
				if (JSONUtils::isMetadataPresent(metadataRoot, field))
					remoteClientIPAddress = metadataRoot.get(field, "").asString();

				try
				{
					_logger->info(__FILEREF__ + "Login User"
						+ ", email: " + email
					);
                        
					loginDetailsRoot = _mmsEngineDBFacade->login(
							email, 
							password
					);

					field = "ldapEnabled";
					loginDetailsRoot[field] = _ldapEnabled;

					field = "userKey";
					userKey = JSONUtils::asInt64(loginDetailsRoot, field, 0);
            
					_logger->info(__FILEREF__ + "Login User"
						+ ", userKey: " + to_string(userKey)
						+ ", email: " + email
					);
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
			else // if (_ldapEnabled)
			{
				vector<string> mandatoryFields = {
					"Name",
					"Password"
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

				string field = "Name";
				string userName = metadataRoot.get(field, "").asString();

				field = "Password";
				string password = metadataRoot.get(field, "").asString();

				field = "RemoteClientIPAddress";
				if (JSONUtils::isMetadataPresent(metadataRoot, field))
					remoteClientIPAddress = metadataRoot.get(field, "").asString();

				try
				{
					_logger->info(__FILEREF__ + "Login User"
						+ ", userName: " + userName
					);

					string email;
					bool testCredentialsSuccessful = false;

					{
						istringstream iss(_ldapURL);
						vector<string> ldapURLs;
						copy(
							istream_iterator<std::string>(iss),
							istream_iterator<std::string>(),
							back_inserter(ldapURLs)
						);

						for(string ldapURL: ldapURLs)
						{
							try
							{
								_logger->error(__FILEREF__ + "ldap URL"
									+ ", ldapURL: " + ldapURL
									+ ", userName: " + userName
								);

								LdapWrapper ldapWrapper;

								ldapWrapper.init(ldapURL, _ldapCertificatePathName,
									_ldapManagerUserName, _ldapManagerPassword);

								pair<bool, string> testCredentialsSuccessfulAndEmail =
									ldapWrapper.testCredentials(userName, password, _ldapBaseDn);

								tie(testCredentialsSuccessful, email) = testCredentialsSuccessfulAndEmail;

								break;
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "ldap URL failed"
									+ ", ldapURL: " + ldapURL
									+ ", e.what(): " + e.what()
								);
							}
						}
					}

					if (!testCredentialsSuccessful)
					{
						_logger->error(__FILEREF__ + "Ldap Login failed"
							+ ", userName: " + userName
						);

						throw LoginFailed();
					}

					bool userAlreadyRegistered;
					try
					{
						_logger->info(__FILEREF__ + "Login User"
							+ ", email: " + email
						);

						loginDetailsRoot = _mmsEngineDBFacade->login(
							email,
							string("")		// password in case of ActiveDirectory is empty
						);

						field = "ldapEnabled";
						loginDetailsRoot[field] = _ldapEnabled;

						field = "userKey";
						userKey = JSONUtils::asInt64(loginDetailsRoot, field, 0);
            
						_logger->info(__FILEREF__ + "Login User"
							+ ", userKey: " + to_string(userKey)
							+ ", email: " + email
						);

						userAlreadyRegistered = true;
					}
					catch(LoginFailed e)
					{
						userAlreadyRegistered = false;
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

					if (!userAlreadyRegistered)
					{
						_logger->info(__FILEREF__ + "Register ActiveDirectory User"
							+ ", email: " + email
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
						pair<int64_t,string> userKeyAndEmail =
							_mmsEngineDBFacade->registerActiveDirectoryUser(
							userName,
							email,
							string(""),	// userCountry,
							createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization, shareWorkspace,
							editMedia, editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
							applicationRecorder,
							_ldapDefaultWorkspaceKeys,
							_expirationInDaysWorkspaceDefaultValue,
							chrono::system_clock::now() + chrono::hours(24 * 365 * 10)
								// chrono::system_clock::time_point userExpirationDate
						);

						string apiKey;
						tie(userKey, apiKey) = userKeyAndEmail;

						_logger->info(__FILEREF__ + "Login User"
							+ ", userKey: " + to_string(userKey)
							+ ", apiKey: " + apiKey
							+ ", email: " + email
						);

						loginDetailsRoot = _mmsEngineDBFacade->login(
							email,
							string("")		// password in case of ActiveDirectory is empty
						);

						string field = "ldapEnabled";
						loginDetailsRoot[field] = _ldapEnabled;

						field = "userKey";
						userKey = JSONUtils::asInt64(loginDetailsRoot, field, 0);
            
						_logger->info(__FILEREF__ + "Login User"
							+ ", userKey: " + to_string(userKey)
							+ ", email: " + email
						);
					}
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
						+ ", ldapURL: " + _ldapURL
						+ ", ldapCertificatePathName: " + _ldapCertificatePathName
						+ ", ldapManagerUserName: " + _ldapManagerUserName
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
        }

		if (_savingGEOUserInfo && remoteClientIPAddress != "")
		{
			try
			{
				_logger->info(__FILEREF__ + "Save Login Statistics"
					+ ", userKey: " + to_string(userKey)
					+ ", remoteClientIPAddress: " + remoteClientIPAddress
				);

				string geoServiceURL;
				ostringstream response;
				bool responseInitialized = false;
				string continent;
				string continentCode;
				string country;
				string countryCode;
				string region;
				string city;
				string org;
				string isp;
				int timezoneGMTOffset = -1;
				try
				{
					geoServiceURL =
						_geoServiceURL
						+ remoteClientIPAddress
					;

					// list<string> header;

					curlpp::Cleanup cleaner;
					curlpp::Easy request;

					// Setting the URL to retrive.
					request.setOpt(new curlpp::options::Url(geoServiceURL));

					// timeout consistent with nginx configuration (fastcgi_read_timeout)
					request.setOpt(new curlpp::options::Timeout(_geoServiceTimeoutInSeconds));

					string httpsPrefix("https");
					if (geoServiceURL.size() >= httpsPrefix.size()
						&& 0 == geoServiceURL.compare(0, httpsPrefix.size(), httpsPrefix))
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

					// request.setOpt(new curlpp::options::HttpHeader(header));

					request.setOpt(new curlpp::options::WriteStream(&response));

					chrono::system_clock::time_point startGeoService
						= chrono::system_clock::now();

					responseInitialized = true;
					request.perform();
					chrono::system_clock::time_point endGeoService
						= chrono::system_clock::now();

					string sResponse = response.str();
					// LF and CR create problems to the json parser...
					while (sResponse.size() > 0 && (sResponse.back() == 10
						|| sResponse.back() == 13))
						sResponse.pop_back();

					_logger->info(__FILEREF__ + "geoService"
						+ ", geoServiceURL: " + geoServiceURL
						+ ", sResponse: " + sResponse
						+ ", @MMS statistics@ - geoServiceDuration (secs): @"
							+ to_string(
								chrono::duration_cast<chrono::seconds>(endGeoService - startGeoService).count()) + "@"
					);

					try
					{
						Json::Value geoServiceResponse;

						Json::CharReaderBuilder builder;
						Json::CharReader* reader = builder.newCharReader();
						string errors;

						bool parsingSuccessful = reader->parse(sResponse.c_str(),
							sResponse.c_str() + sResponse.size(), 
							&geoServiceResponse, &errors);
						delete reader;

						if (!parsingSuccessful)
						{
							string errorMessage = __FILEREF__ + "geoService. Failed to parse the response body"
								+ ", errors: " + errors
								+ ", sResponse: " + sResponse
							;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						bool geoSuccess;
						string field = "success";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							geoSuccess = JSONUtils::asBool(geoServiceResponse, field, false);
						else
							geoSuccess = false;
						if (!geoSuccess)
						{
							string errorMessage = __FILEREF__ + "geoService failed"
								+ ", geoSuccess: " + to_string(geoSuccess)
								+ ", sResponse: " + sResponse
							;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						field = "continent";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							continent = geoServiceResponse.get(field, "").asString();
						field = "continent_code";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							continentCode = geoServiceResponse.get(field, "").asString();
						field = "country";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							country = geoServiceResponse.get(field, "").asString();
						field = "country_code";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							countryCode = geoServiceResponse.get(field, "").asString();
						field = "region";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							region = geoServiceResponse.get(field, "").asString();
						field = "city";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							city = geoServiceResponse.get(field, "").asString();
						field = "org";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							org = geoServiceResponse.get(field, "").asString();
						field = "isp";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							isp = geoServiceResponse.get(field, "").asString();
						field = "timezone_gmtOffset";
						if (JSONUtils::isMetadataPresent(geoServiceResponse, field))
							timezoneGMTOffset = JSONUtils::asInt(geoServiceResponse, field, -1);
					}
					catch(...)
					{
						string errorMessage = string("geoService. Response Body json is not well format")
							+ ", sResponse: " + sResponse
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				catch (curlpp::LogicError & e) 
				{
					_logger->error(__FILEREF__ + "geoService URL failed (LogicError)"
						+ ", geoServiceURL: " + geoServiceURL 
						+ ", exception: " + e.what()
						+ ", response.str(): " + (responseInitialized ? response.str() : "")
					);

					throw e;
				}
				catch (curlpp::RuntimeError & e) 
				{ 
					string errorMessage = string("getService failed (RuntimeError)")
						+ ", geoServiceURL: " + geoServiceURL 
						+ ", exception: " + e.what()
						+ ", response.str(): " + (responseInitialized ? response.str() : "")
					;

					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				catch (runtime_error e)
				{
					_logger->error(__FILEREF__ + "geoService failed (exception)"
						+ ", geoServiceURL: " + geoServiceURL 
						+ ", exception: " + e.what()
						+ ", response.str(): " + (responseInitialized ? response.str() : "")
					);

					throw e;
				}
				catch (exception e)
				{
					_logger->error(__FILEREF__ + "geoService failed (exception)"
						+ ", geoServiceURL: " + geoServiceURL 
						+ ", exception: " + e.what()
						+ ", response.str(): " + (responseInitialized ? response.str() : "")
					);

					throw e;
				}

				try
				{
					_logger->info(__FILEREF__ + "_mmsEngineDBFacade->saveLoginStatistics"
						+ ", userKey: " + to_string(userKey)
						+ ", remoteClientIPAddress: " + remoteClientIPAddress
					);

					_mmsEngineDBFacade->saveLoginStatistics(userKey, remoteClientIPAddress,
						continent, continentCode, country, countryCode,
						region, city, org, isp, timezoneGMTOffset
					);

				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__
						+ "_mmsEngineDBFacade->saveLoginStatistics failed"
						+ ", userKey: " + to_string(userKey)
						+ ", remoteClientIPAddress: " + remoteClientIPAddress
						+ ", e.what(): " + e.what()
					);

					throw e;
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__
						+ "_mmsEngineDBFacade->saveLoginStatistics failed"
						+ ", userKey: " + to_string(userKey)
						+ ", remoteClientIPAddress: " + remoteClientIPAddress
						+ ", e.what(): " + e.what()
					);

					throw e;
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "Saving Login Statistics failed"
					+ ", userKey: " + to_string(userKey)
					+ ", remoteClientIPAddress: " + remoteClientIPAddress
					+ ", e.what(): " + e.what()
				);

				// string errorMessage = string("Internal server error: ") + e.what();
				// _logger->error(__FILEREF__ + errorMessage);

				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "Saving Login Statistics failed"
					+ ", userKey: " + to_string(userKey)
					+ ", remoteClientIPAddress: " + remoteClientIPAddress
					+ ", e.what(): " + e.what()
				);

				// string errorMessage = string("Internal server error");
				// _logger->error(__FILEREF__ + errorMessage);

				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}
		}
		else
		{
			try
			{
				_logger->info(__FILEREF__ + "Save Login Statistics"
					+ ", userKey: " + to_string(userKey)
					+ ", remoteClientIPAddress: " + remoteClientIPAddress
				);

				string continent;
				string continentCode;
				string country;
				string countryCode;
				string region;
				string city;
				string org;
				string isp;
				int timezoneGMTOffset = -1;

				try
				{
					_logger->info(__FILEREF__ + "_mmsEngineDBFacade->saveLoginStatistics"
						+ ", userKey: " + to_string(userKey)
						+ ", remoteClientIPAddress: " + remoteClientIPAddress
					);

					_mmsEngineDBFacade->saveLoginStatistics(userKey, remoteClientIPAddress,
						continent, continentCode, country, countryCode,
						region, city, org, isp, timezoneGMTOffset
					);

				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__
						+ "_mmsEngineDBFacade->saveLoginStatistics failed"
						+ ", userKey: " + to_string(userKey)
						+ ", remoteClientIPAddress: " + remoteClientIPAddress
						+ ", e.what(): " + e.what()
					);

					throw e;
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__
						+ "_mmsEngineDBFacade->saveLoginStatistics failed"
						+ ", userKey: " + to_string(userKey)
						+ ", remoteClientIPAddress: " + remoteClientIPAddress
						+ ", e.what(): " + e.what()
					);

					throw e;
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "Saving Login Statistics failed"
					+ ", userKey: " + to_string(userKey)
					+ ", remoteClientIPAddress: " + remoteClientIPAddress
					+ ", e.what(): " + e.what()
				);

				// string errorMessage = string("Internal server error: ") + e.what();
				// _logger->error(__FILEREF__ + errorMessage);

				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "Saving Login Statistics failed"
					+ ", userKey: " + to_string(userKey)
					+ ", remoteClientIPAddress: " + remoteClientIPAddress
					+ ", e.what(): " + e.what()
				);

				// string errorMessage = string("Internal server error");
				// _logger->error(__FILEREF__ + errorMessage);

				// sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

        try
        {
            _logger->info(__FILEREF__ + "Login User"
                + ", userKey: " + to_string(userKey)
            );

            Json::Value loginWorkspaceRoot =
                    _mmsEngineDBFacade->getLoginWorkspace(userKey);

            string field = "workspace";
            loginDetailsRoot[field] = loginWorkspaceRoot;

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
        string country;
		bool passwordChanged = false;
        string newPassword;
        string oldPassword;

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

		if(!_ldapEnabled)
        {
            vector<string> mandatoryFields = {
                "Name",
                "EMail",
                "Country"
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

            name = metadataRoot.get("Name", "").asString();
            email = metadataRoot.get("EMail", "").asString();
            country = metadataRoot.get("Country", "").asString();

			if (JSONUtils::isMetadataPresent(metadataRoot, "NewPassword")
					&& JSONUtils::isMetadataPresent(metadataRoot, "OldPassword"))
			{
				passwordChanged = true;
				newPassword = metadataRoot.get("NewPassword", "").asString();
				oldPassword = metadataRoot.get("OldPassword", "").asString();
			}
        }
		else
        {
			/*
            vector<string> mandatoryFields = {
                "Country"
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
			*/

            country = metadataRoot.get("Country", "").asString();
        }

        try
        {
            _logger->info(__FILEREF__ + "Updating User"
                + ", userKey: " + to_string(userKey)
                + ", name: " + name
                + ", email: " + email
            );

            Json::Value loginDetailsRoot = _mmsEngineDBFacade->updateUser(
					_ldapEnabled,
                    userKey,
                    name, 
                    email, 
                    country,
					passwordChanged,
					newPassword,
					oldPassword);

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
        string newName;
        string newMaxEncodingPriority;
        string newEncodingPeriod;
        int64_t newMaxIngestionsNumber;
        int64_t newMaxStorageInMB;
        string newLanguageCode;
        string newExpirationDate;
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
                "Name",
                "MaxEncodingPriority",
                "EncodingPeriod",
                "MaxIngestionsNumber",
                "MaxStorageInMB",
                "LanguageCode",
                "ExpirationDate",
                "CreateRemoveWorkspace",
                "IngestWorkflow",
                "CreateProfiles",
                "DeliveryAuthorization",
                "ShareWorkspace",
                "EditMedia",
                "EditConfiguration",
                "KillEncoding",
                "CancelIngestionJob",
                "EditEncodersPool",
                "ApplicationRecorder"
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

            newEnabled = JSONUtils::asBool(metadataRoot, "Enabled", false);
            newName = metadataRoot.get("Name", "").asString();
            newMaxEncodingPriority = metadataRoot.get("MaxEncodingPriority", "").asString();
            newEncodingPeriod = metadataRoot.get("EncodingPeriod", "").asString();
            newMaxIngestionsNumber = JSONUtils::asInt64(metadataRoot, "MaxIngestionsNumber", 0);
            newMaxStorageInMB = JSONUtils::asInt64(metadataRoot, "MaxStorageInMB", 0);
            newLanguageCode = metadataRoot.get("LanguageCode", "").asString();
            newExpirationDate = metadataRoot.get("ExpirationDate", "").asString();
            newCreateRemoveWorkspace = JSONUtils::asBool(metadataRoot, "CreateRemoveWorkspace", false);
            newIngestWorkflow = JSONUtils::asBool(metadataRoot, "IngestWorkflow", false);
            newCreateProfiles = JSONUtils::asBool(metadataRoot, "CreateProfiles", false);
            newDeliveryAuthorization = JSONUtils::asBool(metadataRoot, "DeliveryAuthorization", false);
            newShareWorkspace = JSONUtils::asBool(metadataRoot, "ShareWorkspace", false);
            newEditMedia = JSONUtils::asBool(metadataRoot, "EditMedia", false);
            newEditConfiguration = JSONUtils::asBool(metadataRoot, "EditConfiguration", false);
            newKillEncoding = JSONUtils::asBool(metadataRoot, "KillEncoding", false);
            newCancelIngestionJob = JSONUtils::asBool(metadataRoot, "CancelIngestionJob", false);
            newEditEncodersPool = JSONUtils::asBool(metadataRoot, "EditEncodersPool", false);
            newApplicationRecorder = JSONUtils::asBool(metadataRoot, "ApplicationRecorder", false);
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
                    newEnabled, newName, newMaxEncodingPriority,
                    newEncodingPeriod, newMaxIngestionsNumber,
                    newMaxStorageInMB, newLanguageCode, newExpirationDate,
                    newCreateRemoveWorkspace, newIngestWorkflow, newCreateProfiles,
                    newDeliveryAuthorization, newShareWorkspace,
                    newEditMedia, newEditConfiguration, newKillEncoding, newCancelIngestionJob,
					newEditEncodersPool, newApplicationRecorder);

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

void API::setWorkspaceAsDefault(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "setWorkspaceAsDefault";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        int64_t workspaceKeyToBeSetAsDefault = -1;
        auto workspaceKeyToBeSetAsDefaultIt = queryParameters.find("workspaceKeyToBeSetAsDefault");
        if (workspaceKeyToBeSetAsDefaultIt == queryParameters.end() || workspaceKeyToBeSetAsDefaultIt->second == "")
		{
			string errorMessage = string("The 'workspaceKeyToBeSetAsDefault' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		workspaceKeyToBeSetAsDefault = stoll(workspaceKeyToBeSetAsDefaultIt->second);

        try
        {
            _logger->info(__FILEREF__ + "setWorkspaceAsDefault"
                + ", userKey: " + to_string(userKey)
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
            );
            
            _mmsEngineDBFacade->setWorkspaceAsDefault (userKey, workspace->_workspaceKey,
					workspaceKeyToBeSetAsDefault);

            string responseBody;

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

void API::deleteWorkspace(
        FCGX_Request& request,
		int64_t userKey,
        shared_ptr<Workspace> workspace)
{
    string api = "deleteWorkspace";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", userKey: " + to_string(userKey)
		+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        try
        {
            _logger->info(__FILEREF__ + "Delete Workspace from DB"
                + ", userKey: " + to_string(userKey)
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
            );

            _mmsEngineDBFacade->deleteWorkspace(
                    userKey, workspace->_workspaceKey);

            _logger->info(__FILEREF__ + "Workspace from DB deleted"
                + ", userKey: " + to_string(userKey)
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
            );
            
            string responseBody;
            
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

        try
        {
            _logger->info(__FILEREF__ + "Delete Workspace from Storage"
                + ", userKey: " + to_string(userKey)
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
            );
            
            _mmsStorage->deleteWorkspace( workspace);

            _logger->info(__FILEREF__ + "Workspace from Storage deleted"
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
            );
            
            string responseBody;
            
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
			+ ", userKey: " + to_string(userKey)
			+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
			+ ", userKey: " + to_string(userKey)
			+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}


void API::workspaceUsage (
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    Json::Value workspaceUsageRoot;
    
    try
    {
        string field;
        
        {
            Json::Value requestParametersRoot;
            
            field = "requestParameters";
            workspaceUsageRoot[field] = requestParametersRoot;
        }
        
        Json::Value responseRoot;
		{
			int64_t workSpaceUsageInBytes;

			pair<int64_t,int64_t> workSpaceUsageInBytesAndMaxStorageInMB =
				_mmsEngineDBFacade->getWorkspaceUsage(workspace->_workspaceKey);
			tie(workSpaceUsageInBytes, ignore) = workSpaceUsageInBytesAndMaxStorageInMB;              
                                                                                                            
			int64_t workSpaceUsageInMB = workSpaceUsageInBytes / 1000000;

			field = "usageInMB";
			responseRoot[field] = workSpaceUsageInMB;
		}
        
        field = "response";
        workspaceUsageRoot[field] = responseRoot;

		Json::StreamWriterBuilder wbuilder;
		string responseBody = Json::writeString(wbuilder, workspaceUsageRoot);
            
		sendSuccess(request, 200, responseBody);            
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "getWorkspaceUsage exception"
            + ", e.what(): " + e.what()
        );

		sendError(request, 500, e.what());

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "getWorkspaceUsage exception"
        );

		sendError(request, 500, e.what());

        throw e;
    }
}
