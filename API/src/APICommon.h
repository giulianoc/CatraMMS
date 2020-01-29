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

#include <stdlib.h>
#include "fcgi_stdio.h"
#include <unordered_map>
#include "spdlog/spdlog.h"
#include "json/json.h"
#include "MMSEngineDBFacade.h"
#include "fcgi_config.h"

struct WrongBasicAuthentication: public exception {    
    char const* what() const throw() 
    {
        return "Wrong Basic Authentication present into the Request";
    }; 
};

class APICommon {
public:
    APICommon(Json::Value configuration,
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            mutex* fcgiAcceptMutex,
            shared_ptr<spdlog::logger> logger);

    APICommon(Json::Value configuration,
            mutex* fcgiAcceptMutex,
            shared_ptr<spdlog::logger> logger);
    
	void init(Json::Value configuration,
		mutex* fcgiAcceptMutex,                                                                           
		shared_ptr<spdlog::logger> logger);
    
    virtual ~APICommon();
    
    int operator()();

    // int manageBinaryRequest();

    virtual void manageRequestAndResponse(
        FCGX_Request& request,
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        bool basicAuthenticationPresent,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool, bool, bool,bool,bool,bool,bool,bool>& userKeyWorkspaceAndFlags,
		string apiKey,
        unsigned long contentLength,
        string requestBody,
        unordered_map<string, string>& requestDetails
    ) = 0;
    
    /*
    virtual void getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool>& userKeyWorkspaceAndFlags,
        unsigned long contentLength
    ) = 0;
    */
    
    static Json::Value loadConfigurationFile(const char* configurationPathName);
    
protected:
    Json::Value                     _configuration;

    string							_guiProtocol;
    string							_guiHostname;
    int								_guiPort;

	string							_hostName;
    shared_ptr<spdlog::logger>      _logger;
    mutex*                          _fcgiAcceptMutex;
	bool							_accessToDBAllowed;
    shared_ptr<MMSEngineDBFacade>   _mmsEngineDBFacade;

    unsigned long long   _maxBinaryContentLength;

    // void sendSuccess(FCGX_Request& request, int htmlResponseCode, string responseBody);
	void sendSuccess(FCGX_Request& request, int htmlResponseCode,
        string responseBody, string contentType = "",
        string cookieName = "", string cookieValue = "", string cookiePath = "",
		bool enableCorsGETHeader = false);
    void sendSuccess(int htmlResponseCode, string responseBody);
    void sendRedirect(FCGX_Request& request, string locationURL);
    void sendHeadSuccess(FCGX_Request& request, int htmlResponseCode, unsigned long fileSize);
    void sendHeadSuccess(int htmlResponseCode, unsigned long fileSize);
    void sendError(FCGX_Request& request, int htmlResponseCode, string errorMessage);
    void sendError(int htmlResponseCode, string errorMessage);
    // void sendEmail(string to, string subject, vector<string>& emailBody);
    
    bool isMetadataPresent(Json::Value root, string field);

private:
    int             _managedRequestsNumber;
    unsigned long   _maxAPIContentLength;
	string			_encoderUser;
	string			_encoderPassword;
    
    void fillEnvironmentDetails(
        const char * const * envp, 
        unordered_map<string, string>& requestDetails);
    
    void fillQueryString(
        string queryString,
        unordered_map<string, string>& queryParameters);
    
    bool basicAuthenticationRequired(
        string requestURI,
        unordered_map<string, string> queryParameters);

    // bool requestToUploadBinary(unordered_map<string, string>& queryParameters);

    string getHtmlStandardMessage(int htmlResponseCode);

    // static size_t emailPayloadFeed(void *ptr, size_t size, size_t nmemb, void *userp);
};

#endif
