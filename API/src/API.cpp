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

API::API(): APICommon() {
    _encodingPriorityDefaultValue = CMSEngineDBFacade::EncodingPriority::Low;
    _encodingPeriodDefaultValue = CMSEngineDBFacade::EncodingPeriod::Daily;
    _maxIngestionsNumberDefaultValue = 1;
    _maxStorageInGBDefaultValue = 1;
}

API::~API() {
}

void API::manageRequestAndResponse(
        string requestURI,
        string requestMethod,
        pair<shared_ptr<Customer>,bool>& customerAndFlags,
        unsigned long contentLength,
        string requestBody
)
{
    
    string customerPrefix = "/catracms/customer";

    if (requestURI.compare(0, customerPrefix.size(), customerPrefix) == 0
            && requestMethod == "POST")
    {
        bool isAdminAPI = customerAndFlags.second;
        if (!isAdminAPI)
        {
            string errorMessage = string("APIKey flags does not have the ADMIN permission"
                    ", isAdminAPI: " + isAdminAPI
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(403, errorMessage);

            throw runtime_error(errorMessage);
        }
        
        registerCustomer(requestURI, requestMethod, contentLength, requestBody);
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

void API::registerCustomer(string requestURI,
        string requestMethod,
        unsigned long contentLength,
        string requestBody
)
{
    string api = "registerCustomer";

    _logger->info(__FILEREF__ + "Received registerCustomer"
        + ", requestURI: " + requestURI
        + ", requestMethod: " + requestMethod
        + ", contentLength: " + to_string(contentLength)
        + ", requestBody: " + requestBody
    );

    try
    {
        string name;
        string email;
        string password;
        CMSEngineDBFacade::EncodingPriority encodingPriority;
        CMSEngineDBFacade::EncodingPeriod encodingPeriod;
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
                if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
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
            if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "encodingPriority is not present, set the default value"
                    + ", _encodingPriorityDefaultValue: " + CMSEngineDBFacade::toString(_encodingPriorityDefaultValue)
                );

                encodingPriority = _encodingPriorityDefaultValue;
            }
            else
            {
                string sEncodingPriority = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    encodingPriority = CMSEngineDBFacade::toEncodingPriority(sEncodingPriority);
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
            if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                _logger->info(__FILEREF__ + "encodingPeriod is not present, set the default value"
                    + ", _encodingPeriodDefaultValue: " + CMSEngineDBFacade::toString(_encodingPeriodDefaultValue)
                );

                encodingPeriod = _encodingPeriodDefaultValue;
            }
            else
            {
                string sEncodingPeriod = metadataRoot.get(field, "XXX").asString();
                try
                {                        
                    encodingPeriod = CMSEngineDBFacade::toEncodingPeriod(sEncodingPeriod);
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
            if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
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
            if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
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
            pair<int64_t,string> customerKeyAndConfirmationCode = 
                _cmsEngine->registerCustomer(
                    name,                       // string customerName,
                    "",                             // string street,
                    "",                             // string city,
                    "",                             // string state,
                    "",                             // string zip,
                    "",                             // string phone,
                    "",                             // string countryCode,
                    CMSEngineDBFacade::CustomerType::IngestionAndDelivery,  // CMSEngineDBFacade::CustomerType customerType
                    "",                             // string deliveryURL,
                    encodingPriority,               //  CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
                    encodingPeriod,                 //  CMSEngineDBFacade::EncodingPeriod encodingPeriod,
                    maxIngestionsNumber,            // long maxIngestionsNumber,
                    maxStorageInGB,                 // long maxStorageInGB,
                    "",                             // string languageCode,
                    name,                           // string userName,
                    password,                       // string userPassword,
                    email,                          // string userEmailAddress,
                    chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
            );

            string responseBody =
                string("{ ")
                + "\"customerKey\": " + to_string(customerKeyAndConfirmationCode.first) + " "
                + "}";
            sendSuccess(201, responseBody);
            
            string to = "giulianoc@catrasoftware.it";
            string subject = "Confirmation code";
            
            vector<string> emailBody;
            emailBody.push_back("<p>Hi John,</p>");
            emailBody.push_back(string("<p>This is the confirmation code ") + customerKeyAndConfirmationCode.second + "</p>");
            emailBody.push_back(string("<p>for the customer key ") + to_string(customerKeyAndConfirmationCode.first) + "</p>");
            emailBody.push_back("<p>Bye!</p>");

            sendEmail(to, subject, emailBody);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_cmsEngine->registerCustomer failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_cmsEngine->registerCustomer failed"
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

int main(int argc, char** argv) 
{
    API api;

    return api.listen();
}
