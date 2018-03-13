/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   API.h
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#ifndef API_h
#define API_h

#include "APICommon.h"

class API: public APICommon {
public:
    API(Json::Value configuration, 
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            mutex* fcgiAcceptMutex,
            mutex* fileUploadProgress,
            shared_ptr<spdlog::logger> logger);
    
    ~API();
    
    virtual void getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength);

    virtual void manageRequestAndResponse(
            FCGX_Request& request,
            string requestURI,
            string requestMethod,
            unordered_map<string, string> queryParameters,
            tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
            unsigned long contentLength,
            string requestBody,
            string xCatraMMSResumeHeader,
            unordered_map<string, string>& requestDetails
    );
    
    void fileUploadProgressCheck();
    void stopUploadFileProgressThread();

private:
    mutex*      _fileUploadProgress;
    vector<pair<int64_t,int>> _fileUploadProgressToBeMonitored;
    
    MMSEngineDBFacade::EncodingPriority _encodingPriorityCustomerDefaultValue;
    MMSEngineDBFacade::EncodingPeriod _encodingPeriodCustomerDefaultValue;
    int _maxIngestionsNumberCustomerDefaultValue;
    int _maxStorageInGBCustomerDefaultValue;
    unsigned long       _binaryBufferLength;
    unsigned long       _progressUpdatePeriodInSeconds;
    int                 _webServerPort;
    bool                _fileUploadProgressThreadShutdown;
    int                 _maxProgressCallFailures;
    string              _progressURI;

    void registerCustomer(
        FCGX_Request& request,
        string requestBody);
    
    void confirmCustomer(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters);

    void createAPIKey(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters);

    void ingestContent(
        FCGX_Request& request,
        shared_ptr<Customer> customer,
        unordered_map<string, string> queryParameters,
        string requestBody);
    
    void uploadBinary(
        FCGX_Request& request,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool> customerAndFlags,
        unsigned long contentLength
    );
};

#endif /* POSTCUSTOMER_H */

