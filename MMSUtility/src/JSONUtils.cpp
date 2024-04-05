#include "JSONUtils.h"
#include "spdlog/spdlog.h"

bool JSONUtils::isMetadataPresent(json root, string field)
{
	if (root == nullptr)
		return false;
	else
		return root.contains(field);
}

bool JSONUtils::isNull(json root, string field)
{
	if (root == nullptr)
	{
		string errorMessage = fmt::format(
			"JSONUtils::isNull, root is null"
			", field: {}",
			field
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	return root[field].is_null();
}

string JSONUtils::asString(json root, string field, string defaultValue)
{
	if (root == nullptr)
		return defaultValue;

	try
	{
		if (field == "")
		{
			if (root.type() == json::value_t::number_integer)
				return to_string(root.template get<int>());
			else if (root.type() == json::value_t::number_float)
				return to_string(root.template get<float>());
			else
				return root.template get<string>();
		}
		else
		{
			if (!JSONUtils::isMetadataPresent(root, field))
				return defaultValue;
			if (root.at(field).type() == json::value_t::number_integer ||
				root.at(field).type() == json::value_t::number_float)
				return to_string(root.at(field));
			else
				return root.at(field);
		}
	}
	catch (json::out_of_range &e)
	{
		return defaultValue;
	}
}

int JSONUtils::asInt(json root, string field, int defaultValue)
{
	if (root == nullptr)
		return defaultValue;

	try
	{
		if (field == "")
		{
			if (root.type() == json::value_t::string)
				return strtol(asString(root, "", "0").c_str(), nullptr, 10);
			else
				return root.template get<int>();
		}
		else
		{
			if (!JSONUtils::isMetadataPresent(root, field))
				return defaultValue;
			if (root.at(field).type() == json::value_t::string)
				return strtol(asString(root, field, "0").c_str(), nullptr, 10);
			else
				return root.at(field);
		}
	}
	catch (json::out_of_range &e)
	{
		return defaultValue;
	}
}

int64_t JSONUtils::asInt64(json root, string field, int64_t defaultValue)
{
	if (root == nullptr)
		return defaultValue;

	try
	{
		if (field == "")
		{
			if (root.type() == json::value_t::string)
				return strtoll(asString(root, "", "0").c_str(), nullptr, 10);
			else
				return root.template get<int64_t>();
		}
		else
		{
			if (!JSONUtils::isMetadataPresent(root, field))
				return defaultValue;
			if (root.at(field).type() == json::value_t::string)
				return strtoll(asString(root, field, "0").c_str(), nullptr, 10);
			else
				return root.at(field);
		}
	}
	catch (json::out_of_range &e)
	{
		return defaultValue;
	}
}

double JSONUtils::asDouble(json root, string field, double defaultValue)
{
	if (root == nullptr)
		return defaultValue;

	try
	{
		if (field == "")
		{
			if (root.type() == json::value_t::string)
				return stod(asString(root, "", "0"), nullptr);
			else
				return root.template get<double>();
		}
		else
		{
			if (!JSONUtils::isMetadataPresent(root, field))
				return defaultValue;
			if (root.at(field).type() == json::value_t::string)
				return stod(asString(root, field, "0"), nullptr);
			else
				return root.at(field);
		}
	}
	catch (json::out_of_range &e)
	{
		return defaultValue;
	}
}

bool JSONUtils::asBool(json root, string field, bool defaultValue)
{
	if (root == nullptr)
		return defaultValue;

	try
	{
		if (field == "")
		{
			if (root.type() == json::value_t::string)
			{
				string sTrue = "true";

				bool isEqual = asString(root, "", "").length() != sTrue.length()
								   ? false
								   : equal(
										 asString(root, "", "").begin(),
										 asString(root, "", "").end(),
										 sTrue.begin(), [](int c1, int c2)
										 { return toupper(c1) == toupper(c2); }
									 );

				return isEqual ? true : false;
			}
			else
				return root.template get<bool>();
		}
		else
		{
			if (!JSONUtils::isMetadataPresent(root, field))
				return defaultValue;
			if (root.at(field).type() == json::value_t::string)
			{
				string sTrue = "true";

				bool isEqual =
					asString(root, field, "").length() != sTrue.length()
						? false
						: equal(
							  asString(root, field, "").begin(),
							  asString(root, field, "").end(), sTrue.begin(),
							  [](int c1, int c2)
							  { return toupper(c1) == toupper(c2); }
						  );

				return isEqual ? true : false;
			}
			else
				return root.at(field);
		}
	}
	catch (json::out_of_range &e)
	{
		return defaultValue;
	}
}

json JSONUtils::toJson(string j, bool warningIfError)
{
	try
	{
		if (j == "")
			return json();
		else
			return json::parse(j);
	}
	catch (json::parse_error &ex)
	{
		string errorMessage = fmt::format(
			"failed to parse the json"
			", json: {}"
			", at byte: {}",
			j, ex.byte
		);
		if (warningIfError)
			SPDLOG_WARN(errorMessage);
		else
			SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

string JSONUtils::toString(json root)
{
	if (root == nullptr)
		return "null";
	else
		return root.dump(-1, ' ', true);
	/*
		Json::StreamWriterBuilder wbuilder;
		wbuilder.settings_["emitUTF8"] = true;
		wbuilder.settings_["indentation"] = "";

		return Json::writeString(wbuilder, joValueRoot);
	*/
}
