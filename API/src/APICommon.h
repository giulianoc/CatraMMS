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
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include "MMSEngineDBFacade.h"
#include "fcgi_config.h"

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

struct WrongBasicAuthentication: public exception {    
    char const* what() const throw() 
    {
        return "Wrong Basic Authentication present into the Request";
    }; 
};

class APICommon {
public:
    APICommon(
		json configurationRoot,
		mutex* fcgiAcceptMutex,
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		shared_ptr<spdlog::logger> logger);

    APICommon(
		json configurationRoot,
		mutex* fcgiAcceptMutex,
		shared_ptr<spdlog::logger> logger);
    
	void init(
		json configurationRoot,
		mutex* fcgiAcceptMutex,                                                                           
		shared_ptr<spdlog::logger> logger);
    
    virtual ~APICommon();
    
    int operator()();

    // int manageBinaryRequest();

    virtual void manageRequestAndResponse(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        bool basicAuthenticationPresent,
        tuple<int64_t,shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>& userKeyWorkspaceAndFlags,
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
    
    static json loadConfigurationFile(const char* configurationPathName);
    
protected:
    json                     _configurationRoot;

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
	void sendSuccess(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
		FCGX_Request& request,
		string requestURI, string requestMethod,
		int htmlResponseCode,
        string responseBody, string contentType = "",
        string cookieName = "", string cookieValue = "", string cookiePath = "",
		bool enableCorsGETHeader = false, string originHeader = "");
    // void sendSuccess(int htmlResponseCode, string responseBody);
    void sendRedirect(FCGX_Request& request, string locationURL);
    void sendHeadSuccess(FCGX_Request& request, int htmlResponseCode, unsigned long fileSize);
    void sendHeadSuccess(int htmlResponseCode, unsigned long fileSize);
    void sendError(FCGX_Request& request, int htmlResponseCode, string errorMessage);
    void sendError(int htmlResponseCode, string errorMessage);
    // void sendEmail(string to, string subject, vector<string>& emailBody);
    
	string getClientIPAddress(unordered_map<string, string>& requestDetails);

private:
    int64_t			_requestIdentifier;
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
