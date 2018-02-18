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

#include <fastcgi++/manager.hpp>
#include "RegisterCustomer.h"

RegisterCustomer::RegisterCustomer(): APICommon() {
}

RegisterCustomer::~RegisterCustomer() {
}

bool RegisterCustomer::response()
{
    out << "Content-Type: application/json; charset=ISO-8859-1\r\n\r\n";

/*    
    std::map<std::string, std::string> parameters;
    for (const auto& post: environment().posts)
    {
        parameters[post->first] = post->second;
    }
    
    if (parameters.find("name") == parameters.end())
    {
        sendError("Name is missing");
    }
    else if (parameters.find("publisher") == parameters.end())
    {
        sendError("Publisher is missing");
    }
    else if (parameters.find("date") == parameters.end())
    {
        sendError("Date is missing");
    }
    else if (parameters.find("edition") == parameters.end())
    {    
        sendError("Edition is missing");
    }
    else
    {        
        // TODO
    }
  */
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


        return true;
}

int main(int argc, char** argv) 
{

    try
    {
        Fastcgipp::Manager<RegisterCustomer> manager;
        manager.setupSignals();
        manager.listen();
        manager.start();
        manager.join();
    }
    catch (std::exception& e)
    {
    //        error_log(e.what());  
    }
  

    return 0;
}
