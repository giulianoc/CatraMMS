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
#include "API.h"

int main(int argc, char** argv) 
{
    API api;

    return api.listen();
}

API::API(): APICommon() {
    _encodingPriorityDefaultValue = MMSEngineDBFacade::EncodingPriority::Low;
    _encodingPeriodDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;
    _maxIngestionsNumberDefaultValue = 1;
    _maxStorageInGBDefaultValue = 1;
}

API::~API() {
}

void API::getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
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

    if (method == "registerCustomer")
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
        
        registerCustomer(requestBody);
    }
    else if (method == "confirmCustomer")
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
        
        confirmCustomer(queryParameters);
    }
    else if (method == "createAPIKey")
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
        
        createAPIKey(queryParameters);
    }
    else if (method == "ingestContent")
    {
        bool isUserAPI = get<2>(customerAndFlags);
        if (!isUserAPI)
        {
            string errorMessage = string("APIKey flags does not have the USER permission"
                    ", isUserAPI: " + to_string(isUserAPI)
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        ingestContent(get<0>(customerAndFlags), queryParameters, requestBody);
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

void API::registerCustomer(string requestBody)
{
    string api = "registerCustomer";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        string name;
        string email;
        string password;
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
                string errorMessage = string("Json metadata failed during the parsing"
                        ", json data: " + requestBody
                        );
                _logger->error(__FILEREF__ + errorMessage);

                sendError(400, errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(exception e)
        {
            string errorMessage = string("Json metadata failed during the parsing"
                    ", json data: " + requestBody
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);
        }

        // name, email and password
        {
            vector<string> mandatoryFields = {
                "Name",
                "EMail",
                "Password"
            };
            for (string field: mandatoryFields)
            {
                if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
                {
                    string errorMessage = string("Json field is not present or it is null")
                            + ", Field: " + field;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }

            name = metadataRoot.get("Name", "XXX").asString();
            email = metadataRoot.get("EMail", "XXX").asString();
            password = metadataRoot.get("Password", "XXX").asString();
        }

        // encodingPriority
        {
            string field = "EncodingPriority";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "encodingPriority is not present, set the default value"
                    + ", _encodingPriorityDefaultValue: " + MMSEngineDBFacade::toString(_encodingPriorityDefaultValue)
                );

                encodingPriority = _encodingPriorityDefaultValue;
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

                    sendError(400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        // EncodingPeriod
        {
            string field = "EncodingPeriod";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "encodingPeriod is not present, set the default value"
                    + ", _encodingPeriodDefaultValue: " + MMSEngineDBFacade::toString(_encodingPeriodDefaultValue)
                );

                encodingPeriod = _encodingPeriodDefaultValue;
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

                    sendError(400, errorMessage);

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
                    + ", _maxIngestionsNumberDefaultValue: " + to_string(_maxIngestionsNumberDefaultValue)
                );

                maxIngestionsNumber = _maxIngestionsNumberDefaultValue;
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

                    sendError(400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        // MaxStorageInGB
        {
            string field = "MaxStorageInGB";
            if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "MaxStorageInGB is not present, set the default value"
                    + ", _maxStorageInGBDefaultValue: " + to_string(_maxStorageInGBDefaultValue)
                );

                maxStorageInGB = _maxStorageInGBDefaultValue;
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

                    sendError(400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        try
        {
            tuple<int64_t,int64_t,string> customerKeyUserKeyAndConfirmationCode = 
                _mmsEngine->registerCustomer(
                    name,                       // string customerName,
                    "",                             // string street,
                    "",                             // string city,
                    "",                             // string state,
                    "",                             // string zip,
                    "",                             // string phone,
                    "",                             // string countryCode,
                    MMSEngineDBFacade::CustomerType::IngestionAndDelivery,  // MMSEngineDBFacade::CustomerType customerType
                    "",                             // string deliveryURL,
                    encodingPriority,               //  MMSEngineDBFacade::EncodingPriority maxEncodingPriority,
                    encodingPeriod,                 //  MMSEngineDBFacade::EncodingPeriod encodingPeriod,
                    maxIngestionsNumber,            // long maxIngestionsNumber,
                    maxStorageInGB,                 // long maxStorageInGB,
                    "",                             // string languageCode,
                    name,                           // string userName,
                    password,                       // string userPassword,
                    email,                          // string userEmailAddress,
                    chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
            );

            string responseBody = string("{ ")
                + "\"customerKey\": " + to_string(get<0>(customerKeyUserKeyAndConfirmationCode)) + " "
                + "}";
            sendSuccess(201, responseBody);
            
            string to = "giulianoc@catrasoftware.it";
            string subject = "Confirmation code";
            
            vector<string> emailBody;
            emailBody.push_back("<p>Hi John,</p>");
            emailBody.push_back(string("<p>This is the confirmation code ") + get<2>(customerKeyUserKeyAndConfirmationCode) + "</p>");
            emailBody.push_back(string("<p>for the customer key ") + to_string(get<0>(customerKeyUserKeyAndConfirmationCode)) + "</p>");
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

            sendError(500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(500, errorMessage);

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

        sendError(500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::confirmCustomer(unordered_map<string, string> queryParameters)
{
    string api = "confirmCustomer";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto confirmationCodeIt = queryParameters.find("confirmationeCode");
        if (confirmationCodeIt == queryParameters.end())
        {
            string errorMessage = string("The 'confirmationeCode' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);
        }

        try
        {
            _mmsEngine->confirmCustomer(confirmationCodeIt->second);

            string responseBody;
            sendSuccess(200, responseBody);
            
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

            sendError(500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(500, errorMessage);

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

        sendError(500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::createAPIKey(unordered_map<string, string> queryParameters)
{
    string api = "createAPIKey";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto customerKeyIt = queryParameters.find("customerKey");
        if (customerKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'customerKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);
        }

        auto userKeyIt = queryParameters.find("userKey");
        if (userKeyIt == queryParameters.end())
        {
            string errorMessage = string("The 'userKey' parameter is not found");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);

            throw runtime_error(errorMessage);
        }

        try
        {
            bool adminAPI = false; 
            bool userAPI = true;
            chrono::system_clock::time_point apiKeyExpirationDate = 
                    chrono::system_clock::now() + chrono::hours(24 * 365 * 20);
            
            string apiKey = _mmsEngine->createAPIKey(
                    stol(customerKeyIt->second), 
                    stol(userKeyIt->second), 
                    adminAPI,
                    userAPI,
                    apiKeyExpirationDate);

            string responseBody = string("{ ")
                + "\"apiKey\": \"" + apiKey + "\" "
                + "}";
            sendSuccess(201, responseBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(500, errorMessage);

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

        sendError(500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::ingestContent(
        shared_ptr<Customer> customer,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "ingestContent";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    int64_t ingestionJobKey = -1;
    try
    {
        string label;
        
        auto labelIt = queryParameters.find("label");
        if (labelIt != queryParameters.end())
        {
            label.resize(labelIt->second.length());

            transform(labelIt->second.begin(), labelIt->second.end(), label.begin(),
              [](unsigned char c){
                if ((c >= 'a' && c <= 'z')
                        || (c >= 'A' && c <= 'Z')
                        || (c >= '0' && c <= '9')
                        || (c == '_')
                        || (c == '.')
                        )
                    return c;
                else
                    return (unsigned char) '_';
            });
        }

        // using '-' as separator
        string ingestionJobKeyPlaceHolder = "__INGESTIONJOBKEY__";
        string apiPrefix = "API-";
        string fileNameWithIngestionJobKey =
                apiPrefix
                + ingestionJobKeyPlaceHolder
                + (label != "" ? "-" : "")
                + label
                + ".json"
                ;
 
        _logger->info(__FILEREF__ + "Ingestion template file name"
            + ", fileNameWithIngestionJobKey: " + fileNameWithIngestionJobKey
        );
        
        ingestionJobKey = _mmsEngine->addIngestionJob (
                customer->_customerKey, 
                fileNameWithIngestionJobKey, 
                ingestionJobKeyPlaceHolder,
                requestBody, 
                MMSEngineDBFacade::IngestionType::Unknown, 
                MMSEngineDBFacade::IngestionStatus::StartIngestionThroughAPI);

        fileNameWithIngestionJobKey.replace(
                fileNameWithIngestionJobKey.find(ingestionJobKeyPlaceHolder),
                ingestionJobKeyPlaceHolder.length(), 
                to_string(ingestionJobKey)
        );
        
        string customerFTPMetadataPathName = _mmsStorage->getCustomerFTPRepository(customer);
        customerFTPMetadataPathName
                .append("/")
                .append(fileNameWithIngestionJobKey)
                ;
        
        _logger->info(__FILEREF__ + "Customer FTP Metadata path name"
            + ", customerFTPMetadataPathName: " + customerFTPMetadataPathName
        );

        {
            ofstream metadataStream(customerFTPMetadataPathName);
            metadataStream << requestBody;
            if (!metadataStream.good())
            {
                metadataStream.close();
                
                string errorMessage = string("The writing of Metadata file failed")
                        + ", customerFTPMetadataPathName: " + customerFTPMetadataPathName
                        + ", strerror(errno): " + strerror(errno)
                        ;
                _logger->error(__FILEREF__ + errorMessage);
                
                throw runtime_error(errorMessage);
            }
            
            metadataStream.close();
        }

        string responseBody = string("{ ")
                + "\"ingestionJobKey\": " + to_string(ingestionJobKey) + " "
                + "}";

        sendSuccess(201, responseBody);
    }
    catch(runtime_error e)
    {
        if (ingestionJobKey != -1)
            _mmsEngine->removeIngestionJob (ingestionJobKey);

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
        if (ingestionJobKey != -1)
            _mmsEngine->removeIngestionJob (ingestionJobKey);

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
