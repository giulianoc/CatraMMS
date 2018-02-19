/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RegisterCustomer.cpp
 * Author: giuliano
 * 
 * Created on February 18, 2018, 1:27 AM
 */

#include "RegisterCustomer.h"

RegisterCustomer::RegisterCustomer(): APICommon() {
}

RegisterCustomer::~RegisterCustomer() {
}

void RegisterCustomer::manageRequest()
{
    _logger->info(__FILEREF__ + "aaaaaaa");
    
    /*
  cmsEngine->registerCustomer(
	"Warner",                       // string customerName,
	"",                             // string street,
        "",                             // string city,
        "",                             // string state,
	"",                             // string zip,
        "",                             // string phone,
        "",                             // string countryCode,
        CMSEngineDBFacade::CustomerType::EncodingOnly,  // CMSEngineDBFacade::CustomerType customerType
	"",                             // string deliveryURL,
        CMSEngineDBFacade::EncodingPriority::Default,   //  CMSEngineDBFacade::EncodingPriority maxEncodingPriority,
        CMSEngineDBFacade::EncodingPeriod::Daily,       //  CMSEngineDBFacade::EncodingPeriod encodingPeriod,
	10,                             // long maxIngestionsNumber,
        10,                             // long maxStorageInGB,
	"",                             // string languageCode,
        "giuliano",                     // string userName,
        "giuliano",                     // string userPassword,
        "giulianoc@catrasoftware.it",   // string userEmailAddress,
        chrono::system_clock::now()     // chrono::system_clock::time_point userExpirationDate
    );
     */
}

int main(int argc, char** argv) 
{
    RegisterCustomer registerCustomer;

    return registerCustomer.listen();
}
