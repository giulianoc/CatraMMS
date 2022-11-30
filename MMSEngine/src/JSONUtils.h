/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   JSONUtils.h
 * Author: giuliano
 *
 * Created on March 29, 2018, 6:27 AM
 */

#ifndef JSONUtils_h
#define JSONUtils_h

#include "json/json.h"

using namespace std;

class JSONUtils {
    
public:

	static bool isMetadataPresent(Json::Value root, string field);

	static bool isNull(Json::Value root, string field);

	static int asInt(Json::Value root, string field = "", int defaultValue = 0);

	static int64_t asInt64(Json::Value root, string field = "", int64_t defaultValue = 0);

	static double asDouble(Json::Value root, string field = "", double defaultValue = 0.0);

	static bool asBool(Json::Value root, string field, bool defaultValue);

	static Json::Value toJson(int64_t ingestionJobKey, int64_t encodingJobKey, string json);
};

#endif

