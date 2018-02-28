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
    API();
    
    ~API();
    
    virtual void getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
        unsigned long contentLength);

    virtual void manageRequestAndResponse(
            string requestURI,
            string requestMethod,
            unordered_map<string, string> queryParameters,
            tuple<shared_ptr<Customer>,bool,bool>& customerAndFlags,
            unsigned long contentLength,
            string requestBody
    );
    
private:
    MMSEngineDBFacade::EncodingPriority _encodingPriorityDefaultValue;
    MMSEngineDBFacade::EncodingPeriod _encodingPeriodDefaultValue;
    int _maxIngestionsNumberDefaultValue;
    int _maxStorageInGBDefaultValue;

    void registerCustomer(string requestBody);
    
    void confirmCustomer(unordered_map<string, string> queryParameters);

    void createAPIKey(unordered_map<string, string> queryParameters);

    void ingestContent(
            shared_ptr<Customer> customer,
            unordered_map<string, string> queryParameters,
            string requestBody);
};

#endif /* POSTCUSTOMER_H */

