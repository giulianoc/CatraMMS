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
    API(const char* configurationPathName);
    
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
            string xCatraMMSResumeHeader
    );
    
private:
    MMSEngineDBFacade::EncodingPriority _encodingPriorityCustomerDefaultValue;
    MMSEngineDBFacade::EncodingPeriod _encodingPeriodCustomerDefaultValue;
    int _maxIngestionsNumberCustomerDefaultValue;
    int _maxStorageInGBCustomerDefaultValue;
    unsigned long       _binaryBufferLength;
    unsigned long       _progressUpdatePeriodInSeconds;

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

