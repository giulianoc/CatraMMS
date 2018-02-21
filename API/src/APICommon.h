/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   APICommon.h
 * Author: giuliano
 *
 * Created on February 17, 2018, 6:59 PM
 */

#ifndef APICommon_h
#define APICommon_h

#include <unordered_map>
#include "CMSEngine.h"
#include "spdlog/spdlog.h"

class APICommon {
public:
    APICommon();
    
    virtual ~APICommon();
    
    int listen();

    virtual void manageRequest(
        string requestURI,
        string requestMethod,
        unsigned long contentLength,
        string requestBody
    ) = 0;

    void sendEmail();
    
protected:
    shared_ptr<spdlog::logger>      _logger;
    shared_ptr<CMSEngineDBFacade>   _cmsEngineDBFacade;
    shared_ptr<CMSEngine>           _cmsEngine;

    void sendSuccess(int htmlResponseCode, string responseBody);
    void sendError(int htmlResponseCode, string errorMessage);
    void sendEmail(string from, string to, string bodyText);
    
private:
    int             _managedRequestsNumber;
    long            _processId;
    unsigned long   _stdInMax;

    void fillEnvironmentDetails(
        const char * const * envp, 
        unordered_map<string, string>& requestDetails);

    string getHtmlStandardMessage(int htmlResponseCode);
};

#endif

