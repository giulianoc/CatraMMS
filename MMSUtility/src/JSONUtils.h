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

#include "nlohmann/json.hpp"

using namespace std;

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

struct JsonFieldNotFound : public exception
{
	string _errorMessage;

	JsonFieldNotFound(string errorMessage) { _errorMessage = errorMessage; }

	char const *what() const throw() { return _errorMessage.c_str(); };
};

class JSONUtils
{

  public:
	static bool isMetadataPresent(json root, string field);

	static bool isNull(json root, string field);

	static string asString(json root, string field = "", string defaultValue = "", bool notFoundAsException = false);

	static int asInt(json root, string field = "", int defaultValue = 0, bool notFoundAsException = false);

	static int64_t asInt64(json root, string field = "", int64_t defaultValue = 0, bool notFoundAsException = false);

	static double asDouble(json root, string field = "", double defaultValue = 0.0, bool notFoundAsException = false);

	static bool asBool(json root, string field, bool defaultValue = false, bool notFoundAsException = false);

	static json asJson(json root, string field, json defaultValue = json(), bool notFoundAsException = false);

	static json toJson(string json, bool warningIfError = false);

	static string toString(json joValueRoot);

	static json toJson(vector<int32_t> v);
};

#endif
