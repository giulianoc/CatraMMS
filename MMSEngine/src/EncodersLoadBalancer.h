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
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"
#include "Workspace.h"
#include "MMSEngineDBFacade.h"


using namespace std;

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

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
			shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            json configuration,
            shared_ptr<spdlog::logger> logger);

    virtual ~EncodersLoadBalancer();

    string getEncoderHost(string encodersPool, shared_ptr<Workspace> workspace,
		string encoderToSkip);
    
	tuple<int64_t, string, bool> getEncoderURL(
		int64_t ingestionJobKey, string encodersPoolLabel, shared_ptr<Workspace> workspace,
		int64_t encoderKeyToBeSkipped, bool externalEncoderAllowed);

private:
	/*
    struct EncodersPoolDetails {
        vector<string>          _encoders;
        int                     _lastEncoderUsed;
    };
    
    map<string, EncodersPoolDetails>    _encodersPools;
	*/
    shared_ptr<spdlog::logger>          _logger;
	shared_ptr<MMSEngineDBFacade>		_mmsEngineDBFacade;

	void init();
};

#endif

