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

#include "CMSEngine.h"
#include "spdlog/spdlog.h"

class APICommon {
public:
    APICommon();
    
    virtual ~APICommon();
    
    int listen();

    virtual void manageRequest() = 0;

protected:
    shared_ptr<spdlog::logger>      _logger;
    shared_ptr<CMSEngineDBFacade>   _cmsEngineDBFacade;
    shared_ptr<CMSEngine>           _cmsEngine;
    
};

#endif

