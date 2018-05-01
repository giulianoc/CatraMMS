/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   EncodersLoadBalancer.h
 * Author: giuliano
 *
 * Created on April 28, 2018, 2:33 PM
 */

#ifndef EncodersLoadBalancer_h
#define EncodersLoadBalancer_h

#include <string>
#include <map>
#include <vector>
#include "spdlog/spdlog.h"
#include "json/json.h"
#include "Workspace.h"

using namespace std;

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

class EncodersLoadBalancer {
public:
    EncodersLoadBalancer(
            Json::Value configuration,
            shared_ptr<spdlog::logger> logger);

    virtual ~EncodersLoadBalancer();

    string getEncoderHost(shared_ptr<Workspace> workspace);
    
private:
    struct EncodersPoolDetails {
        vector<string>          _encoders;
        int                     _lastEncoderUsed;
    };
    shared_ptr<spdlog::logger>          _logger;
    
    map<string, EncodersPoolDetails>    _encodersPools;

};

#endif

