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


bool JSONUtils::isMetadataPresent(Json::Value root, string field, bool firstCharCaseInsensitive)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull())
        return true;
    else
	{
		if (firstCharCaseInsensitive)
		{
			if (field.size() > 0)
			{
				if(isupper(field[0]))
				{
					string fieldFirstCharLowerCase = field;
					fieldFirstCharLowerCase[0] = tolower(field[0]);

					if (root.isObject() && root.isMember(fieldFirstCharLowerCase) && !root[fieldFirstCharLowerCase].isNull())
						return true;
					else
						return false;
				}
				else
				{
					string fieldFirstCharUpperCase = field;
					fieldFirstCharUpperCase[0] = toupper(field[0]);

					if (root.isObject() && root.isMember(fieldFirstCharUpperCase) && !root[fieldFirstCharUpperCase].isNull())
						return true;
					else
						return false;
				}
			}
			else
				return false;
		}
		else
			return false;
	}
}

bool JSONUtils::isNull(Json::Value root, string field)
{
	if (root.isObject() && root.isMember(field) && root[field].isNull())
		return true;
	else
		return false;
}

string JSONUtils::asString(Json::Value root, string field, string defaultValue)
{
	if (field == "")
	{
		return root.asString();
	}
	else
	{
		string fieldFirstCharUpperCase;
		string fieldFirstCharLowerCase;
		if(isupper(field[0]))
		{
			fieldFirstCharUpperCase = field;

			fieldFirstCharLowerCase = field;
			fieldFirstCharLowerCase[0] = tolower(field[0]);
		}
		else
		{
			fieldFirstCharUpperCase = field;
			fieldFirstCharUpperCase[0] = toupper(field[0]);

			fieldFirstCharLowerCase = field;
		}

		if (JSONUtils::isMetadataPresent(root, fieldFirstCharUpperCase, false))
			return root.get(fieldFirstCharUpperCase, defaultValue).asString();
		else
			return root.get(fieldFirstCharLowerCase, defaultValue).asString();
	}
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
		string fieldFirstCharUpperCase;
		string fieldFirstCharLowerCase;
		if(isupper(field[0]))
		{
			fieldFirstCharUpperCase = field;

			fieldFirstCharLowerCase = field;
			fieldFirstCharLowerCase[0] = tolower(field[0]);
		}
		else
		{
			fieldFirstCharUpperCase = field;
			fieldFirstCharUpperCase[0] = toupper(field[0]);

			fieldFirstCharLowerCase = field;
		}

		if (JSONUtils::isMetadataPresent(root, fieldFirstCharUpperCase, false))
		{
			if (root.get(fieldFirstCharUpperCase, defaultValue).type() == Json::stringValue)
				return strtol(root.get(fieldFirstCharUpperCase, defaultValue).asString().c_str(), nullptr, 10);
			else
				return root.get(fieldFirstCharUpperCase, defaultValue).asInt();
		}
		else
		{
			if (root.get(fieldFirstCharLowerCase, defaultValue).type() == Json::stringValue)
				return strtol(root.get(fieldFirstCharLowerCase, defaultValue).asString().c_str(), nullptr, 10);
			else
				return root.get(fieldFirstCharLowerCase, defaultValue).asInt();
		}
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
		string fieldFirstCharUpperCase;
		string fieldFirstCharLowerCase;
		if(isupper(field[0]))
		{
			fieldFirstCharUpperCase = field;

			fieldFirstCharLowerCase = field;
			fieldFirstCharLowerCase[0] = tolower(field[0]);
		}
		else
		{
			fieldFirstCharUpperCase = field;
			fieldFirstCharUpperCase[0] = toupper(field[0]);

			fieldFirstCharLowerCase = field;
		}

		if (JSONUtils::isMetadataPresent(root, fieldFirstCharUpperCase, false))
		{
			if (root.get(fieldFirstCharUpperCase, defaultValue).type() == Json::stringValue)
				return strtoll(root.get(fieldFirstCharUpperCase, defaultValue).asString().c_str(), nullptr, 10);
			else
				return root.get(fieldFirstCharUpperCase, defaultValue).asInt64();
		}
		else
		{
			if (root.get(fieldFirstCharLowerCase, defaultValue).type() == Json::stringValue)
				return strtoll(root.get(fieldFirstCharLowerCase, defaultValue).asString().c_str(), nullptr, 10);
			else
				return root.get(fieldFirstCharLowerCase, defaultValue).asInt64();
		}
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
		string fieldFirstCharUpperCase;
		string fieldFirstCharLowerCase;
		if(isupper(field[0]))
		{
			fieldFirstCharUpperCase = field;

			fieldFirstCharLowerCase = field;
			fieldFirstCharLowerCase[0] = tolower(field[0]);
		}
		else
		{
			fieldFirstCharUpperCase = field;
			fieldFirstCharUpperCase[0] = toupper(field[0]);

			fieldFirstCharLowerCase = field;
		}

		if (JSONUtils::isMetadataPresent(root, fieldFirstCharUpperCase, false))
		{
			if (root.get(fieldFirstCharUpperCase, defaultValue).type() == Json::stringValue)
				return stod(root.get(fieldFirstCharUpperCase, defaultValue).asString(), nullptr);
			else
				return root.get(fieldFirstCharUpperCase, defaultValue).asDouble();
		}
		else
		{
			if (root.get(fieldFirstCharLowerCase, defaultValue).type() == Json::stringValue)
				return stod(root.get(fieldFirstCharLowerCase, defaultValue).asString(), nullptr);
			else
				return root.get(fieldFirstCharLowerCase, defaultValue).asDouble();
		}
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
		string fieldFirstCharUpperCase;
		string fieldFirstCharLowerCase;
		if(isupper(field[0]))
		{
			fieldFirstCharUpperCase = field;

			fieldFirstCharLowerCase = field;
			fieldFirstCharLowerCase[0] = tolower(field[0]);
		}
		else
		{
			fieldFirstCharUpperCase = field;
			fieldFirstCharUpperCase[0] = toupper(field[0]);

			fieldFirstCharLowerCase = field;
		}

		if (JSONUtils::isMetadataPresent(root, fieldFirstCharUpperCase, false))
		{
			if (root.get(fieldFirstCharUpperCase, defaultValue).type() == Json::stringValue)
			{
				string sTrue = "true";

				bool isEqual = root.asString().length() != sTrue.length() ? false :
					equal(root.asString().begin(), root.asString().end(), sTrue.begin(),
						[](int c1, int c2){ return toupper(c1) == toupper(c2); });

				return isEqual ? true : false;
			}
			else
				return root.get(fieldFirstCharUpperCase, defaultValue).asBool();
		}
		else
		{
			if (root.get(fieldFirstCharLowerCase, defaultValue).type() == Json::stringValue)
			{
				string sTrue = "true";

				bool isEqual = root.asString().length() != sTrue.length() ? false :
					equal(root.asString().begin(), root.asString().end(), sTrue.begin(),
						[](int c1, int c2){ return toupper(c1) == toupper(c2); });

				return isEqual ? true : false;
			}
			else
				return root.get(fieldFirstCharLowerCase, defaultValue).asBool();
		}
	}
}

Json::Value JSONUtils::toJson(int64_t ingestionJobKey, int64_t encodingJobKey, string json)
{
	Json::Value joValue;

	try
	{
		Json::CharReaderBuilder builder;
		Json::CharReader* reader = builder.newCharReader();
		string errors;

		bool parsingSuccessful = reader->parse(json.c_str(),
			json.c_str() + json.size(), 
			&joValue, &errors);
		delete reader;

		if (!parsingSuccessful)
		{
			string errorMessage = string("failed to parse the json")
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", json: " + json
				+ ", errors: " + errors
			;

			throw runtime_error(errorMessage);
		}
	}
	catch(...)
	{
		string errorMessage = string("json is not well format")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", json: " + json
		;

		throw runtime_error(errorMessage);
	}

	return joValue;
}

string JSONUtils::toString(Json::Value joValueRoot)
{
	Json::StreamWriterBuilder wbuilder;                                                               

	return Json::writeString(wbuilder, joValueRoot);
}

