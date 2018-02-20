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
    
    virtual void manageRequest(
        string requestURI,
        string requestMethod,
        unsigned long contentLength,
        string requestBody
    );
    
private:

};

#endif /* POSTCUSTOMER_H */

