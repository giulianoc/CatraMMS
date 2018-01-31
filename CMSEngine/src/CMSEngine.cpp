/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CMSEngine.cpp
 * Author: multi
 * 
 * Created on January 30, 2018, 3:00 PM
 */

#include "CMSEngine.h"


CMSEngine::CMSEngine(shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<spdlog::logger> logger
        ) 
{
    _logger             = logger;
    _cmsEngineDBFacade  = cmsEngineDBFacade;
}

CMSEngine::~CMSEngine() {
}

void CMSEngine::addCustomer(
	string customerName,
        string password,
	string street,
        string city,
        string state,
	string zip,
        string phone,
        string countryCode,
        CMSEngineDBFacade::CustomerType customerType,
	string deliveryURL,
        bool enabled,
	CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
        CMSEngineDBFacade::EncodingPeriod encodingPeriod,
	long maxIngestionsNumber,
        long maxStorageInGB,
	string languageCode,
        string userName,
        string userPassword,
        string userEmailAddress,
        chrono::system_clock::time_point userExpirationDate
)
{    
    _logger->info(string("Received addCustomer")
        + ", customerName: " + customerName
        + ", password: " + password
        + ", street: " + street
        + ", city: " + city
        + ", state: " + state
        + ", zip: " + zip
        + ", phone: " + phone
        + ", countryCode: " + countryCode
        + ", customerType: " + to_string(static_cast<int>(customerType))
        + ", deliveryURL: " + deliveryURL
        + ", enabled: " + to_string(enabled)
        + ", maxEncodingPriority: " + to_string(static_cast<int>(maxEncodingPriority))
        + ", encodingPeriod: " + to_string(static_cast<int>(encodingPeriod))
        + ", maxIngestionsNumber: " + to_string(maxIngestionsNumber)
        + ", maxStorageInGB: " + to_string(maxStorageInGB)
        + ", languageCode: " + languageCode
        + ", userName: " + userName
        + ", userPassword: " + userPassword
        + ", userEmailAddress: " + userEmailAddress
        // ", userExpirationDate: " + userExpirationDate
    );

    string customerDirectoryName;

    customerDirectoryName.resize(customerName.size());

    transform(
        customerName.begin(), 
        customerName.end(), 
        customerDirectoryName.begin(), 
        [](unsigned char c){
            if (isalpha(c)) 
                return c; 
            else 
                return (unsigned char) '_'; } 
    );

    try
    {
        int64_t customerKey = _cmsEngineDBFacade->addCustomer(
            customerName, 
            customerDirectoryName,
            street,
            city,
            state,
            zip,
            phone,
            countryCode,
            customerType,
            deliveryURL,
            enabled,
            maxEncodingPriority,
            encodingPeriod,
            maxIngestionsNumber,
            maxStorageInGB,
            languageCode,
            userName,
            userPassword,
            userEmailAddress,
            userExpirationDate);
    }
    catch(...)
    {
        _logger->error("_cmsEngineDBFacade->addCustomer failed");
    }

}