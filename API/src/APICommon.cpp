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

#include <curl/curl.h>
#include "fcgio.h"
#include "APICommon.h"

extern char** environ;

APICommon::APICommon()
{
    _logger = spdlog::stdout_logger_mt("API");
    spdlog::set_level(spdlog::level::trace);
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
    
    _managedRequestsNumber = 0;
    _processId = getpid();
    _stdInMax = 1000000;
}

APICommon::~APICommon() {
}

int APICommon::listen()
{
    // Backup the stdio streambufs
    streambuf * cin_streambuf  = cin.rdbuf();
    streambuf * cout_streambuf = cout.rdbuf();
    streambuf * cerr_streambuf = cerr.rdbuf();

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

        try
        {
            unordered_map<string, string> requestDetails;
            unordered_map<string, string> processDetails;

            fillEnvironmentDetails(request.envp, requestDetails);
            fillEnvironmentDetails(environ, requestDetails);
        
            string          requestBody;
            unsigned long   contentLength = 0;
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

                    char* content = new char[contentLength];

                    cin.read(content, contentLength);
                    contentLength = cin.gcount();     // Returns the number of characters extracted by the last unformatted input operation

                    requestBody.assign(content, contentLength);

                    delete [] content;
                }

                // Chew up any remaining stdin - this shouldn't be necessary
                // but is because mod_fastcgi doesn't handle it correctly.

                // ignore() doesn't set the eof bit in some versions of glibc++
                // so use gcount() instead of eof()...
                do 
                    cin.ignore(1024); 
                while (cin.gcount() == 1024);

                _logger->info(__FILEREF__ + "Request body"
                    + ", length: " + to_string(contentLength)
                    + ", requestBody: " + requestBody
                );
            }

            unordered_map<string, string>::iterator it;

            string requestURI;
            if ((it = requestDetails.find("REQUEST_URI")) != requestDetails.end())
                requestURI = it->second;

            string requestMethod;
            if ((it = requestDetails.find("REQUEST_METHOD")) != requestDetails.end())
                requestMethod = it->second;

            manageRequest(requestURI, requestMethod, contentLength, requestBody);            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "manageRequest failed"
                + ", e: " + e.what()
            );
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "manageRequest failed"
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

void APICommon::sendSuccess(int htmlResponseCode, string responseBody)
{
    string httpStatus =
            string("HTTP/1.1 ")
            + to_string(htmlResponseCode) + " "
            + getHtmlStandardMessage(htmlResponseCode);

    cout 
            << httpStatus
            << "Content-type: application/json\r\n"
            << "Content-Length: " << responseBody.length() << "\r\n"
            << "\r\n"
            << responseBody;
}

void APICommon::sendError(int htmlResponseCode, string errorMessage)
{
    string responseBody =
            string("{ ")
            + "\"status\": " + to_string(htmlResponseCode) + ", "
            + "\"error\": " + "\"" + errorMessage + "\"" + " "
            + "}";
    
    string httpStatus =
            string("HTTP/1.1 ")
            + to_string(htmlResponseCode) + " "
            + getHtmlStandardMessage(htmlResponseCode);

    cout 
            << httpStatus
            << "Content-type: application/json\r\n"
            << "Content-Length: " << responseBody.length() << "\r\n"
            << "\r\n"
            << responseBody;
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
            + ", key: " + key
            + ", value: " + value
        );
    }
}

void APICommon:: sendEmail()
{
    // curl --url 'smtps://smtp.gmail.com:465' --ssl-reqd   --mail-from 'gulianocatrambone@gmail.com' --mail-rcpt 'giulianoc@catrasoftware.it'   --upload-file ~/tmp/1.txt --user 'giulianocatrambone@gmail.com:XXXXXXXXXXXXX' --insecure
    
    string emailServerURL = "smtp://smtp.gmail.com:587";
    string from = "gulianocatrambone@gmail.com";
    string to = "giulianoc@catrasoftware.it";
    string cc = "giulianoc@catrasoftware.it";
    
    /*
    try 
    {
        curlpp::Cleanup cleaner;
        curlpp::Easy request;

        // Setting the URL to retrive.
        request.setOpt(new curlpp::options::Url(emailServerURL));

        
        
        
        
        
        
        
        // Set the writer callback to enable cURL 
        // to write result in a memory area
        request.setOpt(new curlpp::options::WriteStream(&mediaSourceFileStream));

        chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
        int lastPercentageUpdated = -1;
        curlpp::types::ProgressFunctionFunctor functor = bind(&CMSEngineProcessor::progressCallback, this,
                ingestionJobKey, lastProgressUpdate, lastPercentageUpdated, downloadingStoppedByUser,
                placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
        request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
        request.setOpt(new curlpp::options::NoProgress(0L));

        request.setOpt(new curlpp::options::NoProgress(0L));

        _logger->info(__FILEREF__ + "Downloading media file"
            + ", sourceReferenceURL: " + sourceReferenceURL
        );
        request.perform();
    }
    catch (curlpp::LogicError & e) 
    {
        _logger->error(__FILEREF__ + "Download failed (LogicError)"
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );
    }
    catch (curlpp::RuntimeError & e) 
    {
        _logger->error(__FILEREF__ + "Download failed (RuntimeError)"
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "Download failed (exception)"
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );
    }
     */

    
    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
//    struct upload_status upload_ctx;
//
//    upload_ctx.lines_read = 0;

    curl = curl_easy_init();
    if(curl) 
    {
        /* This is the URL for your mailserver */
        curl_easy_setopt(curl, CURLOPT_URL, emailServerURL.c_str());

        /* Note that this option isn't strictly required, omitting it will result
         * in libcurl sending the MAIL FROM command with empty sender data. All
         * autoresponses should have an empty reverse-path, and should be directed
         * to the address in the reverse-path which triggered them. Otherwise,
         * they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
         * details.
         */
        // string fromAddr = string("<") + from + ">";
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());

        /* Add two recipients, in this particular case they correspond to the
         * To: and Cc: addressees in the header, but they could be any kind of
         * recipient. */
//        string toAddr = string("<") + to + ">";
//        string ccAddr = string("<") + cc + ">";
        recipients = curl_slist_append(recipients, to.c_str());
        recipients = curl_slist_append(recipients, cc.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        /* We're using a callback function to specify the payload (the headers and
         * body of the message). You could just use the CURLOPT_READDATA option to
         * specify a FILE pointer to read from. */
//        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
//        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        string email =
            "From: \"Giuliano Catrambone\" <giulianocatrambone@gmail.com>\r\n"
            "To: \"Giuliano Catrambone\" <giulianoc@catrasoftware.it>\r\n"
            "Subject: This is a test\r\n"
            "\r\n"
            "Hi John,\r\n"
            "I’m sending this mail with curl thru my gmail account.\r\n"
            "Bye!\r\n";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, email.c_str());
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        /* Send the message */
        res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
        {
            _logger->error(__FILEREF__ + "curl_easy_perform() failed"
                + ", curl_easy_strerror(res): " + curl_easy_strerror(res)
            );
        }

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

    // return (int)res;
}