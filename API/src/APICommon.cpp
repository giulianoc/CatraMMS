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

#include "fcgio.h"
#include "APICommon.h"

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
        fcgi_streambuf cin_fcgi_streambuf(request.in);
        fcgi_streambuf cout_fcgi_streambuf(request.out);
        fcgi_streambuf cerr_fcgi_streambuf(request.err);

        cin.rdbuf(&cin_fcgi_streambuf);
        cout.rdbuf(&cout_fcgi_streambuf);
        cerr.rdbuf(&cerr_fcgi_streambuf);

        try
        {
            manageRequest();
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
        
        cout << "Content-type: text/html\r\n"
             << "\r\n"
             << "<html>\n"
             << "  <head>\n"
             << "    <title>Hello, World!</title>\n"
             << "  </head>\n"
             << "  <body>\n"
             << "    <h1>Hello, World!</h1>\n"
             << "  </body>\n"
             << "</html>\n";

        // Note: the fcgi_streambuf destructor will auto flush
    }

   // restore stdio streambufs
    cin.rdbuf(cin_streambuf);
    cout.rdbuf(cout_streambuf);
    cerr.rdbuf(cerr_streambuf);


    return 0;
}
