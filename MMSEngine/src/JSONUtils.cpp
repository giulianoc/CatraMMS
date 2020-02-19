/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   JSONUtils.cpp
 * Author: giuliano
 * 
 * Created on March 29, 2018, 6:27 AM
 */

#include "JSONUtils.h"


bool JSONUtils::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull()
)
        return true;
    else
        return false;
}

int JSONUtils::asInt(Json::Value root, string field, int defaultValue)
{
	if (field == "")
	{
		if (root.type() == Json::stringValue)
			return strtol(root.asString().c_str(), nullptr, 10);
		else
			return root.asInt();
	}
	else
	{
		if (root.get(field, defaultValue).type() == Json::stringValue)
			return strtol(root.get(field, defaultValue).asString().c_str(), nullptr, 10);
		else
			return root.get(field, defaultValue).asInt();
	}
}

int64_t JSONUtils::asInt64(Json::Value root, string field, int64_t defaultValue)
{
	if (field == "")
	{
		if (root.type() == Json::stringValue)
			return strtoll(root.asString().c_str(), nullptr, 10);
		else
			return root.asInt64();
	}
	else
	{
		if (root.get(field, defaultValue).type() == Json::stringValue)
			return strtoll(root.get(field, defaultValue).asString().c_str(), nullptr, 10);
		else
			return root.get(field, defaultValue).asInt64();
	}
}

double JSONUtils::asDouble(Json::Value root, string field, double defaultValue)
{
	if (field == "")
	{
		if (root.type() == Json::stringValue)
			return stod(root.asString(), nullptr);
		else
			return root.asDouble();
	}
	else
	{
		if (root.get(field, defaultValue).type() == Json::stringValue)
			return stod(root.get(field, defaultValue).asString(), nullptr);
		else
			return root.get(field, defaultValue).asDouble();
	}
}

bool JSONUtils::asBool(Json::Value root, string field, bool defaultValue)
{
	if (field == "")
	{
		if (root.type() == Json::stringValue)
		{
			string sTrue = "true";

			bool isEqual = root.asString().length() != sTrue.length() ? false :
				equal(root.asString().begin(), root.asString().end(), sTrue.begin(),
						[](int c1, int c2){ return toupper(c1) == toupper(c2); });

			return isEqual ? true : false;
		}
		else
			return root.asBool();
	}
	else
	{
		if (root.get(field, defaultValue).type() == Json::stringValue)
		{
			string sTrue = "true";

			bool isEqual = root.asString().length() != sTrue.length() ? false :
				equal(root.asString().begin(), root.asString().end(), sTrue.begin(),
						[](int c1, int c2){ return toupper(c1) == toupper(c2); });

			return isEqual ? true : false;
		}
		else
			return root.get(field, defaultValue).asBool();
	}
}

