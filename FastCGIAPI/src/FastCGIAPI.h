
#ifndef FastCGIAPI_h
#define FastCGIAPI_h

#include <unordered_map>
#include <stdlib.h>
#include "fcgi_stdio.h"
#include "JSONUtils.h"
#include "fcgi_config.h"

using namespace std;


struct CheckAuthorizationFailed: public exception {
	char const* what() const throw()
	{
		return "Wrong Basic Authentication present into the Request";
	};
};


class FastCGIAPI {
public:
    FastCGIAPI(
		Json::Value configuration,
		mutex* fcgiAcceptMutex);
    
	void init(
		Json::Value configuration,
		mutex* fcgiAcceptMutex);
    
    virtual ~FastCGIAPI();
    
    int operator()();

	virtual void stopFastcgi();

    static Json::Value loadConfigurationFile(const char* configurationPathName);
    
protected:
	bool			_shutdown;
    Json::Value		_configuration;

    int64_t			_requestIdentifier;
	string			_hostName;
	int64_t			_maxAPIContentLength;
    mutex*			_fcgiAcceptMutex;


    virtual void manageRequestAndResponse(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        string requestURI,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        bool authorizationPresent,
        string userName, string password,
        unsigned long contentLength,
        string requestBody,
        unordered_map<string, string>& requestDetails
    ) = 0;

	virtual void checkAuthorization(string sThreadId, string userName, string password) = 0;
    
	virtual bool basicAuthenticationRequired(
		string requestURI,
		unordered_map<string, string> queryParameters);

	void sendSuccess(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
		FCGX_Request& request,
		string requestURI, string requestMethod,
		int htmlResponseCode,
        string responseBody = "", string contentType = "",
        string cookieName = "", string cookieValue = "", string cookiePath = "",
		bool enableCorsGETHeader = false, string originHeader = "");
    void sendRedirect(FCGX_Request& request, string locationURL);
    void sendHeadSuccess(FCGX_Request& request, int htmlResponseCode, unsigned long fileSize);
    void sendHeadSuccess(int htmlResponseCode, unsigned long fileSize);
    void sendError(FCGX_Request& request, int htmlResponseCode, string errorMessage);
    void sendError(int htmlResponseCode, string errorMessage);
    
	string getClientIPAddress(unordered_map<string, string>& requestDetails);

private:
    void fillEnvironmentDetails(
        const char * const * envp, 
        unordered_map<string, string>& requestDetails);
    
    void fillQueryString(
        string queryString,
        unordered_map<string, string>& queryParameters);
    
    string getHtmlStandardMessage(int htmlResponseCode);

	string base64_encode(const string& in);

	string base64_decode(const string &in);
};

#endif
