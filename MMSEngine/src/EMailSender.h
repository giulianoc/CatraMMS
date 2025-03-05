/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Validator.h
 * Author: giuliano
 *
 * Created on March 29, 2018, 6:27 AM
 */

#ifndef EMailSender_h
#define EMailSender_h

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"
#include <memory>
#include <string>
#include <vector>

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename((char *)__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

using namespace std;

class EMailSender
{
  public:
  public:
	EMailSender(json configuration);

	virtual ~EMailSender();

	void sendEmail(string tosCommaSeparated, string subject, vector<string> &emailBody, bool useMMSCCToo);

  private:
	json _configuration;

	static size_t emailPayloadFeed(void *ptr, size_t size, size_t nmemb, void *userp);
};

#endif
