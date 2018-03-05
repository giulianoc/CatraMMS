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
// #include "MMSEngine.h"
#include "MMSStorage.h"
#include "spdlog/spdlog.h"

struct NoAPIKeyPresentIntoRequest: public exception {    
    char const* what() const throw() 
    {
        return "No APIKey present into the Request";
    }; 
};

class APICommon {
public:
    APICommon(const char* configurationPathName);
    
    virtual ~APICommon();
    
    int listen();

    int manageBinaryRequest();

    virtual void manageRequestAndResponse(
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength,
        string requestBody
    ) = 0;
    
    virtual void getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength
    ) = 0;

protected:
    Json::Value                     _configuration;
    shared_ptr<spdlog::logger>      _logger;
    shared_ptr<MMSEngineDBFacade>   _mmsEngineDBFacade;
    // shared_ptr<MMSEngine>           _mmsEngine;
    shared_ptr<MMSStorage>          _mmsStorage;

    unsigned long long   _maxBinaryContentLength;

    void sendSuccess(int htmlResponseCode, string responseBody);
    void sendHeadSuccess(int htmlResponseCode, unsigned long fileSize);
    void sendError(int htmlResponseCode, string errorMessage);
    void sendEmail(string to, string subject, vector<string>& emailBody);
    
private:
    int             _managedRequestsNumber;
    long            _processId;
    unsigned long   _maxAPIContentLength;

    Json::Value loadConfigurationFile(const char* configurationPathName);
    
    void fillEnvironmentDetails(
        const char * const * envp, 
        unordered_map<string, string>& requestDetails);
    void fillQueryString(
        string queryString,
        unordered_map<string, string>& queryParameters);

    string getHtmlStandardMessage(int htmlResponseCode);

    static size_t emailPayloadFeed(void *ptr, size_t size, size_t nmemb, void *userp);
};

#endif

