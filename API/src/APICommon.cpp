/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   APICommon.cpp
 * Author: giuliano
 * 
 * Created on February 17, 2018, 6:59 PM
 */

#include <deque>
#include <vector>
#include <sstream>
#include <fstream>
#include <curl/curl.h>
#include "catralibraries/Convert.h"
#include "catralibraries/System.h"
#include "catralibraries/Encrypt.h"
#include "APICommon.h"

extern char** environ;

APICommon::APICommon(Json::Value configuration, 
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            mutex* fcgiAcceptMutex,
            shared_ptr<spdlog::logger> logger)
{
    _configuration      = configuration;
    _mmsEngineDBFacade  = mmsEngineDBFacade;
    _fcgiAcceptMutex    = fcgiAcceptMutex;
    _logger             = logger;

	_hostName			= System::getHostName();                                                  

    _managedRequestsNumber = 0;
    _maxAPIContentLength = _configuration["api"].get("maxContentLength", "XXX").asInt64();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->maxContentLength: " + to_string(_maxAPIContentLength)
    );
    Json::Value api = _configuration["api"];
    _maxBinaryContentLength = api["binary"].get("maxContentLength", "XXX").asInt64();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->binary->maxContentLength: " + to_string(_maxBinaryContentLength)
    );
}

APICommon::~APICommon() {
}

int APICommon::operator()()
{    
    // Backup the stdio streambufs
//    streambuf* cin_streambuf  = cin.rdbuf();
//    streambuf* cout_streambuf = cout.rdbuf();
//    streambuf* cerr_streambuf = cerr.rdbuf();

    string sThreadId;
    {
        thread::id threadId = this_thread::get_id();
        stringstream ss;
        ss << threadId;
        sThreadId = ss.str();
    }

    FCGX_Request request;

    FCGX_InitRequest(&request, 0, 0);

    bool shutdown = false;    
    while (!shutdown)
    {
        int returnAcceptCode;
        {
            _logger->info(__FILEREF__ + "APICommon::ready"
                + ", threadId: " + sThreadId
            );        
            lock_guard<mutex> locker(*_fcgiAcceptMutex);
            
            _logger->info(__FILEREF__ + "APICommon::listen"
                + ", threadId: " + sThreadId
            );        

            returnAcceptCode = FCGX_Accept_r(&request);
        }
        _logger->info(__FILEREF__ + "FCGX_Accept_r"
            + ", returnAcceptCode: " + to_string(returnAcceptCode)
        );
        
        if (returnAcceptCode != 0)
        {
            shutdown = true;
            
            FCGX_Finish_r(&request);
            
            continue;
        }

        _managedRequestsNumber++;

        _logger->info(__FILEREF__ + "Request managed"
            + ", _managedRequestsNumber: " + to_string(_managedRequestsNumber)
            + ", threadId: " + sThreadId
        );        

//        fcgi_streambuf cin_fcgi_streambuf(request->in);
//        fcgi_streambuf cout_fcgi_streambuf(request->out);
//        fcgi_streambuf cerr_fcgi_streambuf(request->err);

//        cin.rdbuf(&cin_fcgi_streambuf);
//        cout.rdbuf(&cout_fcgi_streambuf);
//        cerr.rdbuf(&cerr_fcgi_streambuf);

        unordered_map<string, string> requestDetails;
        // unordered_map<string, string> processDetails;
        unordered_map<string, string> queryParameters;
        // bool            requestToUploadBinary;
        string          requestBody;
        unsigned long   contentLength = 0;
        try
        {
            fillEnvironmentDetails(request.envp, requestDetails);
            // fillEnvironmentDetails(environ, processDetails);
            fillEnvironmentDetails(environ, requestDetails);

            {
                unordered_map<string, string>::iterator it;

                if ((it = requestDetails.find("QUERY_STRING")) != requestDetails.end())
                    fillQueryString(it->second, queryParameters);

                // requestToUploadBinary = this->requestToUploadBinary(queryParameters);
            }

            {
                unordered_map<string, string>::iterator it;
                if ((it = requestDetails.find("REQUEST_METHOD")) != requestDetails.end() &&
                        (it->second == "POST" || it->second == "PUT"))
                {                
                    if ((it = requestDetails.find("CONTENT_LENGTH")) != requestDetails.end())
                    {
                        if (it->second != "")
                        {
                            contentLength = stol(it->second);
                            if (/* !requestToUploadBinary && */ contentLength > _maxAPIContentLength)
                            {
                                string errorMessage = string("No binary request, ContentLength too long")
                                    + ", contentLength: " + to_string(contentLength)
                                    + ", _maxAPIContentLength: " + to_string(_maxAPIContentLength)
                                ;

                                _logger->error(__FILEREF__ + errorMessage);
            
                                throw runtime_error(errorMessage);
                            }
                            /*
                            else if (requestToUploadBinary && contentLength > _maxBinaryContentLength)
                            {
                                string errorMessage = string("Binary request, ContentLength too long")
                                    + ", contentLength: " + to_string(contentLength)
                                    + ", _maxBinaryContentLength: " + to_string(_maxBinaryContentLength)
                                ;

                                _logger->error(__FILEREF__ + errorMessage);
            
                                throw runtime_error(errorMessage);
                            }
                             */
                        }
                        else
                        {
                            contentLength = 0;
                            /*
                             * confirmRegistration, PUT (really it was changed to GET), does not have any body
                            string errorMessage("Content-Length header is empty");

                            _logger->error(__FILEREF__ + errorMessage);
            
                            throw runtime_error(errorMessage);
                            */
                        }
                    }
                    else
                    {
                        contentLength = 0;
                        /*
                        string errorMessage("Content-Length header is missing");

                        _logger->error(__FILEREF__ + errorMessage);
            
                        throw runtime_error(errorMessage);
                        */
                    }

                    // if (!requestToUploadBinary)
                    if (contentLength > 0)
                    {
                        char* content = new char[contentLength];

                        contentLength = FCGX_GetStr(content, contentLength, request.in);
                        // cin.read(content, contentLength);
                        // contentLength = cin.gcount();     // Returns the number of characters extracted by the last unformatted input operation

                        requestBody.assign(content, contentLength);

                        delete [] content;
                    }
                }

                // if (!requestToUploadBinary)
                {
                    // Chew up any remaining stdin - this shouldn't be necessary
                    // but is because mod_fastcgi doesn't handle it correctly.

                    // ignore() doesn't set the eof bit in some versions of glibc++
                    // so use gcount() instead of eof()...
//                    do 
//                        cin.ignore(1024); 
//                    while (cin.gcount() == 1024);
                }

//                _logger->info(__FILEREF__ + "Request body"
//                    + ", length: " + to_string(contentLength)
//                    + ", requestBody: " + requestBody
//                );
            }
        }
        catch(runtime_error& e)
        {
            string errorMessage = e.what();
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, e.what());

            FCGX_Finish_r(&request);
            
            // throw runtime_error(errorMessage);
            continue;
        }
        catch(exception e)
        {
            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            FCGX_Finish_r(&request);
            
            // throw runtime_error(errorMessage);
            continue;
        }

        string requestURI;
        {
            unordered_map<string, string>::iterator it;

            if ((it = requestDetails.find("REQUEST_URI")) != requestDetails.end())
                requestURI = it->second;
        }

        tuple<int64_t,shared_ptr<Workspace>,bool, bool, bool, bool, bool, bool, bool, bool, bool>
			userKeyWorkspaceAndFlags;
        bool basicAuthenticationPresent = basicAuthenticationRequired(requestURI, queryParameters);
        if (basicAuthenticationPresent)
        {
            try
            {
                unordered_map<string, string>::iterator it;

                if ((it = requestDetails.find("HTTP_AUTHORIZATION")) == requestDetails.end())
                {
                    _logger->error(__FILEREF__ + "No APIKey present into the request");

                    throw WrongBasicAuthentication();
                }

                string authorizationPrefix = "Basic ";
                if (!(it->second.size() >= authorizationPrefix.size() && 0 == it->second.compare(0, authorizationPrefix.size(), authorizationPrefix)))
                {
                    _logger->error(__FILEREF__ + "No 'Basic' authorization is present into the request"
                        + ", Authorization: " + it->second
                    );

                    throw WrongBasicAuthentication();
                }

                string usernameAndPasswordBase64 = it->second.substr(authorizationPrefix.length());
                string usernameAndPassword = Convert::base64_decode(usernameAndPasswordBase64);
                size_t userNameSeparator = usernameAndPassword.find(":");
                if (userNameSeparator == string::npos)
                {
                    _logger->error(__FILEREF__ + "Wrong Authentication format"
                        + ", usernameAndPasswordBase64: " + usernameAndPasswordBase64
                        + ", usernameAndPassword: " + usernameAndPassword
                    );

                    throw WrongBasicAuthentication();
                }

                string userKey = usernameAndPassword.substr(0, userNameSeparator);
                string apiKey = usernameAndPassword.substr(userNameSeparator + 1);

                // workspaceAndFlags = _mmsEngine->checkAPIKey (apiKey);
                userKeyWorkspaceAndFlags = _mmsEngineDBFacade->checkAPIKey(apiKey);

                if (get<0>(userKeyWorkspaceAndFlags) != stoll(userKey))
                {
                    _logger->error(__FILEREF__ + "Username of the basic authorization (UserKey) is not the same UserKey the apiKey is referring"
                        + ", username of basic authorization (userKey): " + userKey
                        + ", userKey associated to the APIKey: " + to_string(get<0>(userKeyWorkspaceAndFlags))
                        + ", apiKey: " + apiKey
                    );

                    throw WrongBasicAuthentication();
                }        

                _logger->info(__FILEREF__ + "APIKey and Workspace verified successful");
            }
            catch(WrongBasicAuthentication e)
            {
                _logger->error(__FILEREF__ + "APIKey failed"
                    + ", e.what(): " + e.what()
                );

                string errorMessage = e.what();
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 401, errorMessage);   // bad request

                FCGX_Finish_r(&request);

                // throw runtime_error(errorMessage);
                continue;
            }
            catch(APIKeyNotFoundOrExpired e)
            {
                _logger->error(__FILEREF__ + "_mmsEngine->checkAPIKey failed"
                    + ", e.what(): " + e.what()
                );

                string errorMessage = e.what();
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 401, errorMessage);   // unauthorized

                FCGX_Finish_r(&request);

                //  throw runtime_error(errorMessage);
                continue;
            }
            catch(runtime_error e)
            {
                _logger->error(__FILEREF__ + "_mmsEngine->checkAPIKey failed"
                    + ", e.what(): " + e.what()
                );

                string errorMessage = string("Internal server error");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                FCGX_Finish_r(&request);

                // throw runtime_error(errorMessage);
                continue;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "_mmsEngine->checkAPIKey failed"
                    + ", e.what(): " + e.what()
                );

                string errorMessage = string("Internal server error");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 500, errorMessage);

                FCGX_Finish_r(&request);

                //  throw runtime_error(errorMessage);
                continue;
            }
        }
        
        try
        {
            unordered_map<string, string>::iterator it;

            string requestMethod;
            if ((it = requestDetails.find("REQUEST_METHOD")) != requestDetails.end())
                requestMethod = it->second;

            // string xCatraMMSResumeHeader;
            /*
            if (requestToUploadBinary)
            {
                if ((it = requestDetails.find("HTTP_X_CATRAMMS_RESUME")) != requestDetails.end())
                    xCatraMMSResumeHeader = it->second;
            }
             */

            manageRequestAndResponse(request, requestURI, requestMethod, queryParameters,
                    basicAuthenticationPresent, userKeyWorkspaceAndFlags, 
                    contentLength, requestBody, requestDetails);            
        }
        catch(AlreadyLocked e)
        {
            _logger->error(__FILEREF__ + "manageRequestAndResponse failed"
                + ", e: " + e.what()
            );
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "manageRequestAndResponse failed"
                + ", e: " + e.what()
            );
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "manageRequestAndResponse failed"
                + ", e: " + e.what()
            );
        }

        _logger->info(__FILEREF__ + "APICommon::request finished"
            + ", threadId: " + sThreadId
        );
        
        FCGX_Finish_r(&request);

         // Note: the fcgi_streambuf destructor will auto flush
    }

   // restore stdio streambufs
//    cin.rdbuf(cin_streambuf);
//    cout.rdbuf(cout_streambuf);
//    cerr.rdbuf(cerr_streambuf);


    return 0;
}

bool APICommon::basicAuthenticationRequired(
    string requestURI,
    unordered_map<string, string> queryParameters
)
{
    bool        basicAuthenticationRequired = true;
    
    auto methodIt = queryParameters.find("method");
    if (methodIt == queryParameters.end())
    {
        string errorMessage = string("The 'method' parameter is not found");
        _logger->error(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);
    }
    string method = methodIt->second;

    if (method == "registerUser"
            || method == "confirmRegistration"
            || method == "login"
            )
    {
        basicAuthenticationRequired = false;
    }
    
    // This is the authorization asked when the deliveryURL is received by nginx
    // Here the token is checked and it is not needed any basic authorization
    if (requestURI == "/catramms/delivery/authorization")
    {
        basicAuthenticationRequired = false;
    }
    
    return basicAuthenticationRequired;
}

/*
int APICommon::manageBinaryRequest()
{    

    pid_t processId = getpid();

    _managedRequestsNumber++;

    _logger->info(__FILEREF__ + "manageBinaryRequest"
        + ", _managedRequestsNumber: " + to_string(_managedRequestsNumber)
        + ", processId: " + to_string(processId)
    );        

    unordered_map<string, string> requestDetails;
    // unordered_map<string, string> processDetails;
    string          requestBody;
    unsigned long   contentLength = 0;
    try
    {
        // fillEnvironmentDetails(request.envp, requestDetails);
        fillEnvironmentDetails(environ, requestDetails);

        {
            unordered_map<string, string>::iterator it;
            if ((it = requestDetails.find("REQUEST_METHOD")) != requestDetails.end() &&
                    (it->second == "POST" || it->second == "PUT"))
            {                
                if ((it = requestDetails.find("CONTENT_LENGTH")) != requestDetails.end())
                {
                    if (it->second != "")
                    {
                        contentLength = stol(it->second);
                        if (contentLength > _maxBinaryContentLength)
                        {
                            string errorMessage = string("ContentLength too long")
                                + ", contentLength: " + to_string(contentLength)
                                + ", _maxBinaryContentLength: " + to_string(_maxBinaryContentLength)
                            ;
                            _logger->error(__FILEREF__ + errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                    else
                    {
                        _logger->error(__FILEREF__ + "No ContentLength header is found");
                        
                        contentLength = -1;
                    }
                }
                else
                {
                    _logger->error(__FILEREF__ + "No ContentLength header is found");
                    
                    contentLength = -1;
                }
            }
        }
    }
    catch(exception e)
    {
        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(500, errorMessage);

        throw runtime_error(errorMessage);
    }

    tuple<int64_t,shared_ptr<Workspace>,bool, bool,bool,bool> userKeyWorkspaceAndFlags;
    try
    {
        unordered_map<string, string>::iterator it;

        if ((it = requestDetails.find("HTTP_AUTHORIZATION")) == requestDetails.end())
        {
            _logger->error(__FILEREF__ + "No APIKey present into the request");

            throw WrongBasicAuthentication();
        }

        string authorizationPrefix = "Basic ";
        if (!(it->second.size() >= authorizationPrefix.size() && 0 == it->second.compare(0, authorizationPrefix.size(), authorizationPrefix)))
        {
            _logger->error(__FILEREF__ + "No 'Basic' authorization is present into the request"
                + ", Authorization: " + it->second
            );

            throw WrongBasicAuthentication();
        }

        string usernameAndPasswordBase64 = it->second.substr(authorizationPrefix.length());
        string usernameAndPassword = Convert::base64_decode(usernameAndPasswordBase64);
        size_t userNameSeparator = usernameAndPassword.find(":");
        if (userNameSeparator == string::npos)
        {
            _logger->error(__FILEREF__ + "Wrong Authentication format"
                + ", usernameAndPasswordBase64: " + usernameAndPasswordBase64
                + ", usernameAndPassword: " + usernameAndPassword
            );

            throw WrongBasicAuthentication();
        }

        string userKey = usernameAndPassword.substr(0, userNameSeparator);
        string apiKey = usernameAndPassword.substr(userNameSeparator + 1);

        // workspaceAndFlags = _mmsEngine->checkAPIKey (apiKey);
        userKeyWorkspaceAndFlags = _mmsEngineDBFacade->checkAPIKey(apiKey);

        if (get<1>(userKeyWorkspaceAndFlags)->_workspaceKey != stoll(userKey))
        {
            _logger->error(__FILEREF__ + "Username (WorkspaceKey) is not the same Workspace the apiKey is referring"
                + ", username (userKey): " + userKey
                + ", apiKey: " + apiKey
            );

            throw WrongBasicAuthentication();
        }        

        _logger->info(__FILEREF__ + "APIKey and Workspace verified successful");
    }
    catch(WrongBasicAuthentication e)
    {
        _logger->error(__FILEREF__ + "APIKey failed"
            + ", e.what(): " + e.what()
        );

        string errorMessage = e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(400, errorMessage);   // bad request

        throw runtime_error(errorMessage);
    }
    catch(APIKeyNotFoundOrExpired e)
    {
        _logger->error(__FILEREF__ + "_mmsEngine->checkAPIKey failed"
            + ", e.what(): " + e.what()
        );

        string errorMessage = e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(401, errorMessage);   // unauthorized

        throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsEngine->checkAPIKey failed"
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngine->checkAPIKey failed"
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(500, errorMessage);

        throw runtime_error(errorMessage);
    }

    try
    {
        unordered_map<string, string> queryParameters;
        unordered_map<string, string>::iterator it;

        string requestURI;
        if ((it = requestDetails.find("REQUEST_URI")) != requestDetails.end())
            requestURI = it->second;

        string requestMethod;
        if ((it = requestDetails.find("REQUEST_METHOD")) != requestDetails.end())
            requestMethod = it->second;

        if ((it = requestDetails.find("QUERY_STRING")) != requestDetails.end())
            fillQueryString(it->second, queryParameters);

        string xCatraMMSResumeHeader;
        if ((it = requestDetails.find("HTTP_X_CATRAMMS_RESUME")) != requestDetails.end())
            xCatraMMSResumeHeader = it->second;

        getBinaryAndResponse(requestURI, requestMethod, 
                xCatraMMSResumeHeader, queryParameters,
                userKeyWorkspaceAndFlags, contentLength);            
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "getBinaryAndResponse failed"
            + ", e: " + e.what()
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "getBinaryAndResponse failed"
            + ", e: " + e.what()
        );
    }


    return 0;
}
*/

/*
bool APICommon::requestToUploadBinary(unordered_map<string, string>& queryParameters)
{
    bool requestToUploadBinary = false;
    
    auto methodIt = queryParameters.find("method");
    if (methodIt == queryParameters.end())
    {
        requestToUploadBinary       = false;
    }
    else
    {
        string method = methodIt->second;

        if (method == "uploadBinary")
        {
            requestToUploadBinary       = true;
        }
    }
    
    return requestToUploadBinary;
}
 */

void APICommon::sendSuccess(FCGX_Request& request, int htmlResponseCode, string responseBody)
{
    string endLine = "\r\n";
    
//    string httpStatus =
//            string("HTTP/1.1 ")
//            + to_string(htmlResponseCode) + " "
//            + getHtmlStandardMessage(htmlResponseCode)
//            + endLine;

    string httpStatus =
            string("Status: ")
            + to_string(htmlResponseCode) + " "
            + getHtmlStandardMessage(htmlResponseCode)
            + endLine;

    string contentType;
    if (responseBody != "")
	{
		contentType = string("Content-Type: application/json; charset=utf-8") + endLine;
	}
    
    string completeHttpResponse =
            httpStatus
            + contentType
            + "Content-Length: " + to_string(responseBody.length()) + endLine
            + endLine
            + responseBody;

    _logger->info(__FILEREF__ + "HTTP Success"
        + ", response: " + completeHttpResponse
    );

    FCGX_FPrintF(request.out, completeHttpResponse.c_str());
    
//    cout << completeHttpResponse;
}

void APICommon::sendSuccess(int htmlResponseCode, string responseBody)
{
    string endLine = "\r\n";
    
//    string httpStatus =
//            string("HTTP/1.1 ")
//            + to_string(htmlResponseCode) + " "
//            + getHtmlStandardMessage(htmlResponseCode)
//            + endLine;

    string httpStatus =
            string("Status: ")
            + to_string(htmlResponseCode) + " "
            + getHtmlStandardMessage(htmlResponseCode)
            + endLine;

    string contentType;
    if (responseBody != "")
        contentType = string("Content-Type: application/json; charset=utf-8") + endLine;
    
    string completeHttpResponse =
            httpStatus
            + contentType
            + "Content-Length: " + to_string(responseBody.length()) + endLine
            + endLine
            + responseBody;

    _logger->info(__FILEREF__ + "HTTP Success"
        + ", response: " + completeHttpResponse
    );

    cout << completeHttpResponse;
}

void APICommon::sendRedirect(FCGX_Request& request, string locationURL)
{
    string endLine = "\r\n";
    
    int htmlResponseCode = 301;
    
    string completeHttpResponse =
            string("Status: ") + to_string(htmlResponseCode) 
                + " " + getHtmlStandardMessage(htmlResponseCode) + endLine
            + "Location: " + locationURL + endLine
            + endLine;

    _logger->info(__FILEREF__ + "HTTP Success"
        + ", response: " + completeHttpResponse
    );

    FCGX_FPrintF(request.out, completeHttpResponse.c_str());
    // cout << completeHttpResponse;
}

void APICommon::sendHeadSuccess(FCGX_Request& request, int htmlResponseCode, unsigned long fileSize)
{
    string endLine = "\r\n";
    
//    string httpStatus =
//            string("HTTP/1.1 ")
//            + to_string(htmlResponseCode) + " "
//            + getHtmlStandardMessage(htmlResponseCode)
//            + endLine;

    string httpStatus =
            string("Status: ")
            + to_string(htmlResponseCode) + " "
            + getHtmlStandardMessage(htmlResponseCode)
            + endLine;

    string completeHttpResponse =
            httpStatus
            + "Content-Range: bytes 0-" + to_string(fileSize) + endLine
            + endLine;

    _logger->info(__FILEREF__ + "HTTP HEAD Success"
        + ", response: " + completeHttpResponse
    );

    FCGX_FPrintF(request.out, completeHttpResponse.c_str());
//    cout << completeHttpResponse;
}

void APICommon::sendHeadSuccess(int htmlResponseCode, unsigned long fileSize)
{
    string endLine = "\r\n";
    
//    string httpStatus =
//            string("HTTP/1.1 ")
//            + to_string(htmlResponseCode) + " "
//            + getHtmlStandardMessage(htmlResponseCode)
//            + endLine;

    string httpStatus =
            string("Status: ")
            + to_string(htmlResponseCode) + " "
            + getHtmlStandardMessage(htmlResponseCode)
            + endLine;

    string completeHttpResponse =
            httpStatus
            + "X-CatraMMS-Resume: " + to_string(fileSize) + endLine
            + endLine;

    _logger->info(__FILEREF__ + "HTTP HEAD Success"
        + ", response: " + completeHttpResponse
    );

    cout << completeHttpResponse;
}

void APICommon::sendError(FCGX_Request& request, int htmlResponseCode, string errorMessage)
{
    string endLine = "\r\n";

    string responseBody =
            string("{ ")
            + "\"status\": " + to_string(htmlResponseCode) + ", "
            + "\"error\": " + "\"" + errorMessage + "\"" + " "
            + "}";
    
//    string httpStatus =
//            string("HTTP/1.1 ")
//            + to_string(htmlResponseCode) + " "
//            + getHtmlStandardMessage(htmlResponseCode)
//            + endLine;
    string httpStatus =
            string("Status: ")
            + to_string(htmlResponseCode) + " "
            + getHtmlStandardMessage(htmlResponseCode)
            + endLine;

    string completeHttpResponse =
            httpStatus
            + "Content-Type: application/json; charset=utf-8" + endLine
            + "Content-Length: " + to_string(responseBody.length()) + endLine
            + endLine
            + responseBody
            ;
    
    _logger->info(__FILEREF__ + "HTTP Error"
        + ", response: " + completeHttpResponse
    );

    FCGX_FPrintF(request.out, completeHttpResponse.c_str());
    
//    cout << completeHttpResponse;
}

void APICommon::sendError(int htmlResponseCode, string errorMessage)
{
    string endLine = "\r\n";

    string responseBody =
            string("{ ")
            + "\"status\": " + to_string(htmlResponseCode) + ", "
            + "\"error\": " + "\"" + errorMessage + "\"" + " "
            + "}";
    
//    string httpStatus =
//            string("HTTP/1.1 ")
//            + to_string(htmlResponseCode) + " "
//            + getHtmlStandardMessage(htmlResponseCode)
//            + endLine;
    string httpStatus =
            string("Status: ")
            + to_string(htmlResponseCode) + " "
            + getHtmlStandardMessage(htmlResponseCode)
            + endLine;

    string completeHttpResponse =
            httpStatus
            + "Content-Type: application/json; charset=utf-8" + endLine
            + "Content-Length: " + to_string(responseBody.length()) + endLine
            + endLine
            + responseBody
            ;
    
    _logger->info(__FILEREF__ + "HTTP Error"
        + ", response: " + completeHttpResponse
    );

    cout << completeHttpResponse;
}

string APICommon::getHtmlStandardMessage(int htmlResponseCode)
{
    switch(htmlResponseCode)
    {
        case 200:
            return string("OK");
        case 201:
            return string("Created");
        case 301:
            return string("Moved Permanently");
        case 302:
            return string("Found");
        case 307:
            return string("Temporary Redirect");
        case 403:
            return string("Forbidden");
        case 400:
            return string("Bad Request");
        case 401:
            return string("Unauthorized");
        case 500:
            return string("Internal Server Error");
        default:
            string errorMessage = __FILEREF__ + "HTTP status code not managed"
                + ", htmlResponseCode: " + to_string(htmlResponseCode);
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
    }
    
}

void APICommon::fillEnvironmentDetails(
        const char * const * envp, 
        unordered_map<string, string>& requestDetails)
{

    int valueIndex;

    for ( ; *envp; ++envp)
    {
        string environmentKeyValue = *envp;

        if ((valueIndex = environmentKeyValue.find("=")) == string::npos)
        {
            _logger->error(__FILEREF__ + "Unexpected environment variable"
                + ", environmentKeyValue: " + environmentKeyValue
            );
            
            continue;
        }

        string key = environmentKeyValue.substr(0, valueIndex);
        string value = environmentKeyValue.substr(valueIndex + 1);
        
        requestDetails[key] = value;

        if (key == "REQUEST_URI")
            _logger->info(__FILEREF__ + "Environment variable"
                + ", key/Name: " + key + "=" + value
            );
        else
            _logger->debug(__FILEREF__ + "Environment variable"
                + ", key/Name: " + key + "=" + value
            );
    }
}

void APICommon::fillQueryString(
        string queryString,
        unordered_map<string, string>& queryParameters)
{

    stringstream ss(queryString);
    string token;
    char delim = '&';
    while (getline(ss, token, delim)) 
    {
        if (!token.empty())
        {
            size_t keySeparator;
            
            if ((keySeparator = token.find("=")) == string::npos)
            {
                _logger->error(__FILEREF__ + "Wrong query parameter format"
                    + ", token: " + token
                );
                
                continue;
            }

            string key = token.substr(0, keySeparator);
            string value = token.substr(keySeparator + 1);
            
            queryParameters[key] = value;

            _logger->info(__FILEREF__ + "Query parameter"
                + ", key/Name: " + key + "=" + value
            );
        }
    }    
}

/*
size_t APICommon:: emailPayloadFeed(void *ptr, size_t size, size_t nmemb, void *userp)
{
    deque<string>* pEmailLines = (deque<string>*) userp;
 
    if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) 
    {
        return 0;
    }
 
    if (pEmailLines->size() == 0)
        return 0; // no more lines
  
    string emailLine = pEmailLines->front();
    // cout << "emailLine: " << emailLine << endl;
 
    memcpy(ptr, emailLine.c_str(), emailLine.length());
    pEmailLines->pop_front();
 
    return emailLine.length();
}

void APICommon:: sendEmail(string to, string subject, vector<string>& emailBody)
{
    // curl --url 'smtps://smtp.gmail.com:465' --ssl-reqd   
    //      --mail-from 'giulianocatrambone@gmail.com' 
    //      --mail-rcpt 'giulianoc@catrasoftware.it'   
    //      --upload-file ~/tmp/1.txt 
    //      --user 'giulianocatrambone@gmail.com:XXXXXXXXXXXXX' 
    //      --insecure
    
    // string emailServerURL = "smtp://smtp.gmail.com:587";
    string emailProtocol = _configuration["EmailNotification"].get("protocol", "XXX").asString();
    string emailServer = _configuration["EmailNotification"].get("server", "XXX").asString();
    int emailPort = _configuration["EmailNotification"].get("port", "XXX").asInt();
    string userName = _configuration["EmailNotification"].get("userName", "XXX").asString();
    string password;
    {
        string encryptedPassword = _configuration["EmailNotification"].get("password", "XXX").asString();
        password = Encrypt::decrypt(encryptedPassword);        
    }
    string from = _configuration["EmailNotification"].get("from", "XXX").asString();
    // string to = "giulianoc@catrasoftware.it";
    string cc = _configuration["EmailNotification"].get("cc", "XXX").asString();
    
    string emailServerURL = emailProtocol + "://" + emailServer + ":" + to_string(emailPort);
    

    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    deque<string> emailLines;
  
    emailLines.push_back(string("From: <") + from + ">" + "\r\n");
    emailLines.push_back(string("To: <") + to + ">" + "\r\n");
    if (cc != "")
        emailLines.push_back(string("Cc: <") + cc + ">" + "\r\n");
    emailLines.push_back(string("Subject: ") + subject + "\r\n");
    emailLines.push_back(string("Content-Type: text/html; charset=\"UTF-8\"") + "\r\n");
    emailLines.push_back("\r\n");   // empty line to divide headers from body, see RFC5322
    emailLines.insert(emailLines.end(), emailBody.begin(), emailBody.end());
    
    curl = curl_easy_init();

    if(curl) 
    {
        curl_easy_setopt(curl, CURLOPT_URL, emailServerURL.c_str());
        curl_easy_setopt(curl, CURLOPT_USERNAME, userName.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
        
//        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
//        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

//        * Note that this option isn't strictly required, omitting it will result
//         * in libcurl sending the MAIL FROM command with empty sender data. All
//         * autoresponses should have an empty reverse-path, and should be directed
//         * to the address in the reverse-path which triggered them. Otherwise,
//         * they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
//         * details.
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());

//        * Add two recipients, in this particular case they correspond to the
//         * To: and Cc: addressees in the header, but they could be any kind of
//         * recipient.
        recipients = curl_slist_append(recipients, to.c_str());
        if (cc != "")
            recipients = curl_slist_append(recipients, cc.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

//        * We're using a callback function to specify the payload (the headers and
//         * body of the message). You could just use the CURLOPT_READDATA option to
//         * specify a FILE pointer to read from.
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, APICommon::emailPayloadFeed);
        curl_easy_setopt(curl, CURLOPT_READDATA, &emailLines);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        // Send the message
        _logger->info(__FILEREF__ + "Sending email...");
        res = curl_easy_perform(curl);

        // Check for errors
        if(res != CURLE_OK)
            _logger->error(__FILEREF__ + "curl_easy_perform() failed"
                + ", curl_easy_strerror(res): " + curl_easy_strerror(res)
            );
        else
            _logger->info(__FILEREF__ + "Email sent successful");

        // Free the list of recipients
        curl_slist_free_all(recipients);

//        * curl won't send the QUIT command until you call cleanup, so you should
//         * be able to re-use this connection for additional messages (setting
//         * CURLOPT_MAIL_FROM and CURLOPT_MAIL_RCPT as required, and calling
//         * curl_easy_perform() again. It may not be a good idea to keep the
//         * connection open for a very long time though (more than a few minutes
//         * may result in the server timing out the connection), and you do want to
//         * clean up in the end.
        curl_easy_cleanup(curl);
    }    
}
*/

Json::Value APICommon::loadConfigurationFile(const char* configurationPathName)
{
    Json::Value configurationJson;

    try
    {
        ifstream configurationFile(configurationPathName, ifstream::binary);
        configurationFile >> configurationJson;
    }
    catch(...)
    {
        cerr << string("wrong json configuration format")
                + ", configurationPathName: " + configurationPathName
            << endl;
    }

    return configurationJson;
}
