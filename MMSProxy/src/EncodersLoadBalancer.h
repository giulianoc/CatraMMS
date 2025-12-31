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

#pragma once


#include <map>
#include <string>
#include <vector>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "MMSEngineDBFacade.h"
#include "Workspace.h"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

// using namespace std;

/*
using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;
*/

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ std::string("[") + std::string(__FILE__).substr(std::string(__FILE__).find_last_of("/") + 1) + ":" + to_std::string(__LINE__) + "] "
#else
#define __FILEREF__ std::string("[") + basename((char *)__FILE__) + ":" + to_std::string(__LINE__) + "] "
#endif
#endif

class EncodersLoadBalancer
{
  public:
	EncodersLoadBalancer(std::shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, nlohmann::json configuration);

	virtual ~EncodersLoadBalancer();

	std::string getEncoderHost(std::string encodersPool, std::shared_ptr<Workspace> workspace, std::string encoderToSkip);

	std::tuple<int64_t, std::string, bool> getEncoderURL(
		int64_t ingestionJobKey, std::string encodersPoolLabel, std::shared_ptr<Workspace> workspace, int64_t encoderKeyToBeSkipped, bool externalEncoderAllowed
	);

  private:
	/*
	struct EncodersPoolDetails {
		vector<std::string>          _encoders;
		int                     _lastEncoderUsed;
	};

	map<std::string, EncodersPoolDetails>    _encodersPools;
	*/
	std::shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;

	void init();
};

