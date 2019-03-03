/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Validator.h
 * Author: giuliano
 *
 * Created on March 29, 2018, 6:27 AM
 */

#ifndef EMailSender_h
#define EMailSender_h

#include "spdlog/spdlog.h"
#include "json/json.h"
#include <string>
#include <memory>
#include <vector>

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

using namespace std;

class EMailSender {
public:
    
public:
    EMailSender(            
            shared_ptr<spdlog::logger> logger, 
            Json::Value configuration
    );
    
    virtual ~EMailSender();
    
    void sendEmail(string tosCommaSeparated, string subject, vector<string>& emailBody);
  
private:
    shared_ptr<spdlog::logger>          _logger;
    Json::Value                         _configuration;

    static size_t emailPayloadFeed(void *ptr, size_t size, size_t nmemb, void *userp);
};

#endif

