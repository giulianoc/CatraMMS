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
#include <curl/curl.h>
#include "fcgio.h"
#include "catralibraries/Convert.h"
#include "APICommon.h"

extern char** environ;

APICommon::APICommon()
{
    
    // the log is written in the apache error log (stderr)
    _logger = spdlog::stderr_logger_mt("API");

    // make sure only responses are written to the standard output
    spdlog::set_level(spdlog::level::trace);
    
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [tid %t] %v");
    
    // globally register the loggers so so the can be accessed using spdlog::get(logger_name)
    // spdlog::register_logger(logger);

    size_t dbPoolSize = 5;
    string dbServer ("tcp://127.0.0.1:3306");
    #ifdef __APPLE__
        string dbUsername("root"); string dbPassword("giuliano"); string dbName("workKing");
    #else
        string dbUsername("root"); string dbPassword("root"); string dbName("catracms");
    #endif
    _logger->info(__FILEREF__ + "Creating CMSEngineDBFacade"
        + ", dbPoolSize: " + to_string(dbPoolSize)
        + ", dbServer: " + dbServer
        + ", dbUsername: " + dbUsername
        + ", dbPassword: " + dbPassword
        + ", dbName: " + dbName
            );
    _cmsEngineDBFacade = make_shared<CMSEngineDBFacade>(
            dbPoolSize, dbServer, dbUsername, dbPassword, dbName, _logger);

    _logger->info(__FILEREF__ + "Creating CMSEngine"
            );
    _cmsEngine = make_shared<CMSEngine>(_cmsEngineDBFacade, _logger);
    
    #ifdef __APPLE__
        string storageRootPath ("/Users/multi/GestioneProgetti/Development/catrasoftware/storage/");
    #else
        string storageRootPath ("/home/giuliano/storage/");
    #endif
    unsigned long freeSpaceToLeaveInEachPartitionInMB = 5;
    _logger->info(__FILEREF__ + "Creating CMSStorage"
        + ", storageRootPath: " + storageRootPath
        + ", freeSpaceToLeaveInEachPartitionInMB: " + to_string(freeSpaceToLeaveInEachPartitionInMB)
            );
    _cmsStorage = make_shared<CMSStorage>(
            storageRootPath, 
            freeSpaceToLeaveInEachPartitionInMB,
            _logger);

    _managedRequestsNumber = 0;
    _processId = getpid();
    _stdInMax = 1000000;
}

APICommon::~APICommon() {
}

int APICommon::listen()
{    
    // Backup the stdio streambufs
    streambuf* cin_streambuf  = cin.rdbuf();
    streambuf* cout_streambuf = cout.rdbuf();
    streambuf* cerr_streambuf = cerr.rdbuf();

    FCGX_Request request;

    FCGX_Init();
    FCGX_InitRequest(&request, 0, 0);

    while (FCGX_Accept_r(&request) == 0) 
    {
        _managedRequestsNumber++;
        
        _logger->info(__FILEREF__ + "Request managed"
            + ", _managedRequestsNumber: " + to_string(_managedRequestsNumber)
            + ", _processId: " + to_string(_processId)
        );        

        fcgi_streambuf cin_fcgi_streambuf(request.in);
        fcgi_streambuf cout_fcgi_streambuf(request.out);
        fcgi_streambuf cerr_fcgi_streambuf(request.err);

        cin.rdbuf(&cin_fcgi_streambuf);
        cout.rdbuf(&cout_fcgi_streambuf);
        cerr.rdbuf(&cerr_fcgi_streambuf);

        unordered_map<string, string> requestDetails;
        unordered_map<string, string> processDetails;
        string          requestBody;
        unsigned long   contentLength = 0;
        try
        {
            fillEnvironmentDetails(request.envp, requestDetails);
            fillEnvironmentDetails(environ, processDetails);
        
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
                            if (contentLength > _stdInMax)
                            {
                                _logger->error(__FILEREF__ + "ContentLength too long, it will be truncated and data will be lost"
                                    + ", contentLength: " + to_string(contentLength)
                                    + ", _stdInMax: " + to_string(_stdInMax)
                                );

                                contentLength = _stdInMax;
                            }
                        }
                        else
                            contentLength = _stdInMax;
                    }
                    else
                        contentLength = _stdInMax;

                    {
                        char* content = new char[contentLength];

                        cin.read(content, contentLength);
                        contentLength = cin.gcount();     // Returns the number of characters extracted by the last unformatted input operation

                        requestBody.assign(content, contentLength);

                        delete [] content;
                    }
                }

                {
                    // Chew up any remaining stdin - this shouldn't be necessary
                    // but is because mod_fastcgi doesn't handle it correctly.

                    // ignore() doesn't set the eof bit in some versions of glibc++
                    // so use gcount() instead of eof()...
                    do 
                        cin.ignore(1024); 
                    while (cin.gcount() == 1024);
                }

//                _logger->info(__FILEREF__ + "Request body"
//                    + ", length: " + to_string(contentLength)
//                    + ", requestBody: " + requestBody
//                );
            }
        }
        catch(exception e)
        {
            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(500, errorMessage);

            // throw runtime_error(errorMessage);
            continue;
        }
        
        tuple<shared_ptr<Customer>,bool, bool> customerAndFlags;
        try
        {
            unordered_map<string, string>::iterator it;

            if ((it = requestDetails.find("HTTP_AUTHORIZATION")) == requestDetails.end())
            {
                _logger->error(__FILEREF__ + "No APIKey present into the request");

                throw NoAPIKeyPresentIntoRequest();
            }

            string authorizationPrefix = "Basic ";
            if (it->second.compare(0, authorizationPrefix.size(), authorizationPrefix) != 0)
            {
                _logger->error(__FILEREF__ + "No 'Basic' authorization is present into the request"
                    + ", Authorization: " + it->second
                );

                throw NoAPIKeyPresentIntoRequest();
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

                throw NoAPIKeyPresentIntoRequest();
            }
            
            string custormerKey = usernameAndPassword.substr(0, userNameSeparator);
            string apiKey = usernameAndPassword.substr(userNameSeparator + 1);

            customerAndFlags = _cmsEngine->checkAPIKey (apiKey);
            
            if (get<0>(customerAndFlags)->_customerKey != stol(custormerKey))
            {
                _logger->error(__FILEREF__ + "Username (CustomerKey) is not the same Customer the apiKey is referring"
                    + ", username (custormerKey): " + custormerKey
                    + ", apiKey: " + apiKey
                );

                throw NoAPIKeyPresentIntoRequest();
            }        
            
            _logger->info(__FILEREF__ + "APIKey and Customer verified successful");
        }
        catch(NoAPIKeyPresentIntoRequest e)
        {
            _logger->error(__FILEREF__ + "APIKey failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = e.what();
            _logger->error(__FILEREF__ + errorMessage);

            sendError(400, errorMessage);   // bad request

            // throw runtime_error(errorMessage);
            continue;
        }
        catch(APIKeyNotFoundOrExpired e)
        {
            _logger->error(__FILEREF__ + "_cmsEngine->checkAPIKey failed"
                + ", e.what(): " + e.what()
            );
            
            string errorMessage = e.what();
            _logger->error(__FILEREF__ + errorMessage);

            sendError(401, errorMessage);   // unauthorized

            //  throw runtime_error(errorMessage);
            continue;
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_cmsEngine->checkAPIKey failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(500, errorMessage);

            // throw runtime_error(errorMessage);
            continue;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_cmsEngine->checkAPIKey failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(500, errorMessage);

            //  throw runtime_error(errorMessage);
            continue;
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

            manageRequestAndResponse(requestURI, requestMethod, queryParameters,
                    customerAndFlags, contentLength, requestBody);            
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

        // Note: the fcgi_streambuf destructor will auto flush
    }

   // restore stdio streambufs
    cin.rdbuf(cin_streambuf);
    cout.rdbuf(cout_streambuf);
    cerr.rdbuf(cerr_streambuf);


    return 0;
}

int APICommon::manageBinaryRequest()
{    

    _managedRequestsNumber++;

    _logger->info(__FILEREF__ + "manageBinaryRequest"
        + ", _managedRequestsNumber: " + to_string(_managedRequestsNumber)
        + ", _processId: " + to_string(_processId)
    );        

    // unordered_map<string, string> requestDetails;
    unordered_map<string, string> processDetails;
    string          requestBody;
    unsigned long   contentLength = 0;
    try
    {
        // fillEnvironmentDetails(request.envp, requestDetails);
        fillEnvironmentDetails(environ, processDetails);

        {
            unordered_map<string, string>::iterator it;
            if ((it = processDetails.find("REQUEST_METHOD")) != processDetails.end() &&
                    (it->second == "POST" || it->second == "PUT"))
            {                
                if ((it = processDetails.find("CONTENT_LENGTH")) != processDetails.end())
                {
                    if (it->second != "")
                    {
                        contentLength = stol(it->second);
                        /* This is a binary, We cannot truncate it because it may be big
                        if (contentLength > _stdInMax)
                        {
                            _logger->error(__FILEREF__ + "ContentLength too long, it will be truncated and data will be lost"
                                + ", contentLength: " + to_string(contentLength)
                                + ", _stdInMax: " + to_string(_stdInMax)
                            );

                            contentLength = _stdInMax;
                        }
                         */
                    }
                    else
                    {
                        _logger->error(__FILEREF__ + "No ContentLength header is found, it is read all what we get");
                        contentLength = -1;
                    }
                }
                else
                {
                    _logger->error(__FILEREF__ + "No ContentLength header is found, it is read all what we get");
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

    tuple<shared_ptr<Customer>,bool, bool> customerAndFlags;
    try
    {
        unordered_map<string, string>::iterator it;

        if ((it = processDetails.find("HTTP_AUTHORIZATION")) == processDetails.end())
        {
            _logger->error(__FILEREF__ + "No APIKey present into the request");

            throw NoAPIKeyPresentIntoRequest();
        }

        string authorizationPrefix = "Basic ";
        if (it->second.compare(0, authorizationPrefix.size(), authorizationPrefix) != 0)
        {
            _logger->error(__FILEREF__ + "No 'Basic' authorization is present into the request"
                + ", Authorization: " + it->second
            );

            throw NoAPIKeyPresentIntoRequest();
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

            throw NoAPIKeyPresentIntoRequest();
        }

        string custormerKey = usernameAndPassword.substr(0, userNameSeparator);
        string apiKey = usernameAndPassword.substr(userNameSeparator + 1);

        customerAndFlags = _cmsEngine->checkAPIKey (apiKey);

        if (get<0>(customerAndFlags)->_customerKey != stol(custormerKey))
        {
            _logger->error(__FILEREF__ + "Username (CustomerKey) is not the same Customer the apiKey is referring"
                + ", username (custormerKey): " + custormerKey
                + ", apiKey: " + apiKey
            );

            throw NoAPIKeyPresentIntoRequest();
        }        

        _logger->info(__FILEREF__ + "APIKey and Customer verified successful");
    }
    catch(NoAPIKeyPresentIntoRequest e)
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
        _logger->error(__FILEREF__ + "_cmsEngine->checkAPIKey failed"
            + ", e.what(): " + e.what()
        );

        string errorMessage = e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(401, errorMessage);   // unauthorized

        throw runtime_error(errorMessage);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_cmsEngine->checkAPIKey failed"
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_cmsEngine->checkAPIKey failed"
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
        if ((it = processDetails.find("REQUEST_URI")) != processDetails.end())
            requestURI = it->second;

        string requestMethod;
        if ((it = processDetails.find("REQUEST_METHOD")) != processDetails.end())
            requestMethod = it->second;

        if ((it = processDetails.find("QUERY_STRING")) != processDetails.end())
            fillQueryString(it->second, queryParameters);

        getBinaryAndResponse(requestURI, requestMethod, queryParameters,
                customerAndFlags, contentLength);            
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

    string completeHttpResponse =
            httpStatus
            + "Content-Type: application/json; charset=utf-8" + endLine
            + "Content-Length: " + to_string(responseBody.length()) + endLine
            + endLine
            + responseBody;

    _logger->info(__FILEREF__ + "HTTP Success"
        + ", response: " + completeHttpResponse
    );

    cout << completeHttpResponse;
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

        _logger->info(__FILEREF__ + "Environment variable"
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
    string emailServerURL = "smtps://smtp.gmail.com:465";
    string userName = "giulianocatrambone@gmail.com";
    string password = "12_Giulia";
    string from = "giulianocatrambone@gmail.com";
    // string to = "giulianoc@catrasoftware.it";
    string cc = "giulianocatrambone@gmail.com";
    // string subject = "This is the subject";
//    vector<string> emailBody;
//    emailBody.push_back("Hi John,");
//    emailBody.push_back("<p>I’m sending this mail with curl thru my gmail account.</p>");
//    emailBody.push_back("Bye!");
    

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

        /* Note that this option isn't strictly required, omitting it will result
         * in libcurl sending the MAIL FROM command with empty sender data. All
         * autoresponses should have an empty reverse-path, and should be directed
         * to the address in the reverse-path which triggered them. Otherwise,
         * they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
         * details.
         */
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());

        /* Add two recipients, in this particular case they correspond to the
         * To: and Cc: addressees in the header, but they could be any kind of
         * recipient. */
        recipients = curl_slist_append(recipients, to.c_str());
        if (cc != "")
            recipients = curl_slist_append(recipients, cc.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        /* We're using a callback function to specify the payload (the headers and
         * body of the message). You could just use the CURLOPT_READDATA option to
         * specify a FILE pointer to read from. */
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, APICommon::emailPayloadFeed);
        curl_easy_setopt(curl, CURLOPT_READDATA, &emailLines);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        /* Send the message */
        _logger->info(__FILEREF__ + "Sending email...");
        res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
            _logger->error(__FILEREF__ + "curl_easy_perform() failed"
                + ", curl_easy_strerror(res): " + curl_easy_strerror(res)
            );
        else
            _logger->info(__FILEREF__ + "Email sent successful");

        /* Free the list of recipients */
        curl_slist_free_all(recipients);

        /* curl won't send the QUIT command until you call cleanup, so you should
         * be able to re-use this connection for additional messages (setting
         * CURLOPT_MAIL_FROM and CURLOPT_MAIL_RCPT as required, and calling
         * curl_easy_perform() again. It may not be a good idea to keep the
         * connection open for a very long time though (more than a few minutes
         * may result in the server timing out the connection), and you do want to
         * clean up in the end.
         */
        curl_easy_cleanup(curl);
    }
}