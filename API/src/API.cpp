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
}

API::~API() {
}

void API::manageRequest(
        string requestURI,
        string requestMethod,
        unsigned long contentLength,
        string requestBody
)
{
    
    string customerPrefix = "/catracms/customer";

    if (requestURI.compare(0, customerPrefix.size(), customerPrefix) == 0
            && requestMethod == "POST")
    {
        _logger->info(__FILEREF__ + "registerCustomer"
            + ", requestBody: " + requestBody
        );

        string name;
        string email;
        string password;
        CMSEngineDBFacade::EncodingPriority encodingPriority;
        CMSEngineDBFacade::EncodingPeriod encodingPeriod;
        int maxIngestionsNumber;
        int maxStorageInGB;
        try
        {
            Json::Value metadataRoot(requestBody);
            
            vector<string> mandatoryFields = {
                "Name",
                "EMail",
                "Password"
            };
            for (string field: mandatoryFields)
            {
                if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", Field: " + field;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }

            name = metadataRoot.get("Name", "XXX").asString();
            email = metadataRoot.get("EMail", "XXX").asString();
            password = metadataRoot.get("Password", "XXX").asString();

            string field = "EncodingPriority";
            if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

        }
        catch(...)
        {
            throw runtime_error(string("wrong metadata json format")
                    + ", API: registerCustomer"
                    + ", requestBody: " + requestBody
                    );
        }

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
            CMSEngineDBFacade::EncodingPriority::Low,   //  CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
            CMSEngineDBFacade::EncodingPeriod::Daily,       //  CMSEngineDBFacade::EncodingPeriod encodingPeriod,
            1,                             // long maxIngestionsNumber,
            1,                             // long maxStorageInGB,
            "",                             // string languageCode,
            name,                           // string userName,
            password,                       // string userPassword,
            email,                          // string userEmailAddress,
            chrono::system_clock::now() + chrono::hours(24 * 365 * 10)     // chrono::system_clock::time_point userExpirationDate
        );
    }
}

int main(int argc, char** argv) 
{
    API api;

    return api.listen();
}
