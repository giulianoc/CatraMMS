/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   API.cpp
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#include "API.h"
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include "spdlog/spdlog.h"
#include <format>
#include <regex>

void API::addEncoder(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		bool external;
		bool enabled;
		string protocol;
		string publicServerName;
		string internalServerName;
		int port{};

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "label";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "External";
			external = JSONUtils::asBool(requestBodyRoot, field, false);

			field = "Enabled";
			enabled = JSONUtils::asBool(requestBodyRoot, field, true);

			field = "Protocol";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			protocol = JSONUtils::asString(requestBodyRoot, field, "");

			field = "PublicServerName";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			publicServerName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "InternalServerName";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			internalServerName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Port";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				port = JSONUtils::asInt(requestBodyRoot, field, 80);
			}
			else
			{
				if (protocol == "http")
					port = 80;
				else if (protocol == "https")
					port = 443;
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}",
				requestBody, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t encoderKey = _mmsEngineDBFacade->addEncoder(label, external, enabled, protocol, publicServerName, internalServerName, port);

			sResponse = (string("{ ") + "\"EncoderKey\": " + to_string(encoderKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addEncoder failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::modifyEncoder(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		bool labelToBeModified;

		bool external;
		bool externalToBeModified;

		bool enabled;
		bool enabledToBeModified;

		string protocol;
		bool protocolToBeModified;

		string publicServerName;
		bool publicServerNameToBeModified;

		string internalServerName;
		bool internalServerNameToBeModified;

		int port;
		bool portToBeModified;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "label";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				label = JSONUtils::asString(requestBodyRoot, field, "");
				labelToBeModified = true;
			}
			else
				labelToBeModified = false;

			field = "External";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				external = JSONUtils::asBool(requestBodyRoot, field, false);
				externalToBeModified = true;
			}
			else
				externalToBeModified = false;

			field = "Enabled";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				enabled = JSONUtils::asBool(requestBodyRoot, field, true);
				enabledToBeModified = true;
			}
			else
				enabledToBeModified = false;

			field = "Protocol";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				protocol = JSONUtils::asString(requestBodyRoot, field, "");
				protocolToBeModified = true;
			}
			else
				protocolToBeModified = false;

			field = "PublicServerName";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				publicServerName = JSONUtils::asString(requestBodyRoot, field, "");
				publicServerNameToBeModified = true;
			}
			else
				publicServerNameToBeModified = false;

			field = "InternalServerName";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				internalServerName = JSONUtils::asString(requestBodyRoot, field, "");
				internalServerNameToBeModified = true;
			}
			else
				internalServerNameToBeModified = false;

			field = "Port";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				port = JSONUtils::asInt(requestBodyRoot, field, 80);
				portToBeModified = true;
			}
			else
				portToBeModified = false;
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}",
				requestBody, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t encoderKey;
			auto encoderKeyIt = queryParameters.find("encoderKey");
			if (encoderKeyIt == queryParameters.end())
			{
				string errorMessage = "The 'encoderKey' parameter is not found";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			encoderKey = stoll(encoderKeyIt->second);

			_mmsEngineDBFacade->modifyEncoder(
				encoderKey, labelToBeModified, label, externalToBeModified, external, enabledToBeModified, enabled, protocolToBeModified, protocol,
				publicServerNameToBeModified, publicServerName, internalServerNameToBeModified, internalServerName, portToBeModified, port
			);

			sResponse = (string("{ ") + "\"encoderKey\": " + to_string(encoderKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->modifyEncoder failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::removeEncoder(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t encoderKey;
			auto encoderKeyIt = queryParameters.find("encoderKey");
			if (encoderKeyIt == queryParameters.end())
			{
				string errorMessage = "The 'encoderKey' parameter is not found";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			encoderKey = stoll(encoderKeyIt->second);

			_mmsEngineDBFacade->removeEncoder(encoderKey);

			sResponse = (string("{ ") + "\"encoderKey\": " + to_string(encoderKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeEncoder failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::encoderList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "encoderList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t encoderKey = -1;
		auto encoderKeyIt = queryParameters.find("encoderKey");
		if (encoderKeyIt != queryParameters.end() && !encoderKeyIt->second.empty())
		{
			encoderKey = stoll(encoderKeyIt->second);
			// 2020-01-31: it was sent 0, it should return no rows but, since we have the below check and
			//	it is changed to -1, the return is all the rows. Because of that it was commented
			// if (liveURLKey == 0)
			// 	liveURLKey = -1;
		}

		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && !startIt->second.empty())
		{
			start = stoll(startIt->second);
		}

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && !rowsIt->second.empty())
		{
			rows = stoll(rowsIt->second);
			if (rows > _maxPageSize)
			{
				// 2022-02-13: changed to return an error otherwise the user
				//	think to ask for a huge number of items while the return is much less

				// rows = _maxPageSize;

				string errorMessage =
					__FILEREF__ + "rows parameter too big" + ", rows: " + to_string(rows) + ", _maxPageSize: " + to_string(_maxPageSize);
				throw runtime_error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string label;
		auto labelIt = queryParameters.find("label");
		if (labelIt != queryParameters.end() && !labelIt->second.empty())
		{
			label = labelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(label, regex(plus), plusDecoded);

			label = CurlWrapper::unescape(firstDecoding);
		}

		string serverName;
		auto serverNameIt = queryParameters.find("serverName");
		if (serverNameIt != queryParameters.end() && !serverNameIt->second.empty())
		{
			serverName = serverNameIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(serverName, regex(plus), plusDecoded);

			serverName = CurlWrapper::unescape(firstDecoding);
		}

		int port = -1;
		auto portIt = queryParameters.find("port");
		if (portIt != queryParameters.end() && !portIt->second.empty())
		{
			port = stoi(portIt->second);
		}

		string labelOrder;
		auto labelOrderIt = queryParameters.find("labelOrder");
		if (labelOrderIt != queryParameters.end() && !labelOrderIt->second.empty())
		{
			if (labelOrderIt->second == "asc" || labelOrderIt->second == "desc")
				labelOrder = labelOrderIt->second;
			else
				SPDLOG_WARN(
					"encoderList: 'labelOrder' parameter is unknown"
					", labelOrder: {}",
					labelOrderIt->second
				);
		}

		bool runningInfo = false;
		auto runningInfoIt = queryParameters.find("runningInfo");
		if (runningInfoIt != queryParameters.end())
			runningInfo = (runningInfoIt->second == "true" ? true : false);

		bool allEncoders = false;
		int64_t workspaceKey = apiAuthorizationDetails->workspace->_workspaceKey;
		if (apiAuthorizationDetails->admin)
		{
			// in case of admin, from the GUI, it is needed to:
			// - get the list of all encoders
			// - encoders for a specific workspace

			auto allEncodersIt = queryParameters.find("allEncoders");
			if (allEncodersIt != queryParameters.end())
				allEncoders = (allEncodersIt->second == "true" ? true : false);

			auto workspaceKeyIt = queryParameters.find("workspaceKey");
			if (workspaceKeyIt != queryParameters.end() && !workspaceKeyIt->second.empty())
				workspaceKey = stoll(workspaceKeyIt->second);
		}

		{
			json encoderListRoot = _mmsEngineDBFacade->getEncoderList(
				apiAuthorizationDetails->admin, start, rows, allEncoders, workspaceKey, runningInfo, encoderKey, label, serverName, port, labelOrder
			);

			string responseBody = JSONUtils::toString(encoderListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::encodersPoolList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "encoderList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t encodersPoolKey = -1;
		auto encodersPoolKeyIt = queryParameters.find("encodersPoolKey");
		if (encodersPoolKeyIt != queryParameters.end() && !encodersPoolKeyIt->second.empty())
		{
			encodersPoolKey = stoll(encodersPoolKeyIt->second);
		}

		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && !startIt->second.empty())
		{
			start = stoll(startIt->second);
		}

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && !rowsIt->second.empty())
		{
			rows = stoll(rowsIt->second);
			if (rows > _maxPageSize)
			{
				// 2022-02-13: changed to return an error otherwise the user
				//	think to ask for a huge number of items while the return is much less

				// rows = _maxPageSize;

				string errorMessage = std::format(
					"rows parameter too big"
					", rows: {}"
					", _maxPageSize: {}",
					rows, _maxPageSize
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string label;
		auto labelIt = queryParameters.find("label");
		if (labelIt != queryParameters.end() && !labelIt->second.empty())
		{
			label = labelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(label, regex(plus), plusDecoded);

			label = CurlWrapper::unescape(firstDecoding);
		}

		string labelOrder;
		auto labelOrderIt = queryParameters.find("labelOrder");
		if (labelOrderIt != queryParameters.end() && !labelOrderIt->second.empty())
		{
			if (labelOrderIt->second == "asc" || labelOrderIt->second == "desc")
				labelOrder = labelOrderIt->second;
			else
				SPDLOG_WARN(
					"encodersPoolList: 'labelOrder' parameter is unknown"
					", labelOrder: {}",
					labelOrderIt->second
				);
		}

		{
			json encodersPoolListRoot =
				_mmsEngineDBFacade->getEncodersPoolList(start, rows, apiAuthorizationDetails->workspace->_workspaceKey, encodersPoolKey, label, labelOrder);

			string responseBody = JSONUtils::toString(encodersPoolListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::addEncodersPool(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addEncodersPool";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditEncodersPool)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditEncodersPool: {}",
			apiAuthorizationDetails->canEditEncodersPool
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		vector<int64_t> encoderKeys;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "label";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "encoderKeys";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				json encoderKeysRoot = requestBodyRoot[field];

				for (int encoderIndex = 0; encoderIndex < encoderKeysRoot.size(); ++encoderIndex)
				{
					encoderKeys.push_back(JSONUtils::asInt64(encoderKeysRoot[encoderIndex]));
				}
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format"
				", requestBody: {}",
				requestBody
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t encodersPoolKey = _mmsEngineDBFacade->addEncodersPool(apiAuthorizationDetails->workspace->_workspaceKey, label, encoderKeys);

			sResponse = (string("{ ") + "\"EncodersPoolKey\": " + to_string(encodersPoolKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addEncodersPool failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::modifyEncodersPool(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyEncodersPool";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditEncodersPool)
	{
		string errorMessage = string(
			"APIKey does not have the permission"
			", canEditEncodersPool: {}",
			apiAuthorizationDetails->canEditEncodersPool
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		vector<int64_t> encoderKeys;

		int64_t encodersPoolKey = -1;
		auto encodersPoolKeyIt = queryParameters.find("encodersPoolKey");
		if (encodersPoolKeyIt == queryParameters.end())
		{
			string errorMessage = "The 'encodersPoolKey' parameter is not found";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		encodersPoolKey = stoll(encodersPoolKeyIt->second);

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "label";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "encoderKeys";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				json encoderKeysRoot = requestBodyRoot[field];

				for (int encoderIndex = 0; encoderIndex < encoderKeysRoot.size(); ++encoderIndex)
				{
					encoderKeys.push_back(JSONUtils::asInt64(encoderKeysRoot[encoderIndex]));
				}
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}",
				requestBody, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			_mmsEngineDBFacade->modifyEncodersPool(encodersPoolKey, apiAuthorizationDetails->workspace->_workspaceKey, label, encoderKeys);

			sResponse = (string("{ ") + "\"EncodersPoolKey\": " + to_string(encodersPoolKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->modifyEncodersPool failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::removeEncodersPool(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeEncodersPool";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditEncodersPool)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditEncodersPool: {}",
			apiAuthorizationDetails->canEditEncodersPool
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t encodersPoolKey;
			auto encodersPoolKeyIt = queryParameters.find("encodersPoolKey");
			if (encodersPoolKeyIt == queryParameters.end())
			{
				string errorMessage = "The 'encodersPoolKey' parameter is not found";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodersPoolKey = stoll(encodersPoolKeyIt->second);

			_mmsEngineDBFacade->removeEncodersPool(encodersPoolKey);

			sResponse = (string("{ ") + "\"encodersPoolKey\": " + to_string(encodersPoolKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeEncodersPool failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::addAssociationWorkspaceEncoder(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addAssociationWorkspaceEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t workspaceKey;
			auto workspaceKeyIt = queryParameters.find("workspaceKey");
			if (workspaceKeyIt == queryParameters.end())
			{
				string errorMessage = "The 'workspaceKey' parameter is not found";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			workspaceKey = stoll(workspaceKeyIt->second);

			int64_t encoderKey;
			auto encoderKeyIt = queryParameters.find("encoderKey");
			if (encoderKeyIt == queryParameters.end())
			{
				string errorMessage = "The 'encoderKey' parameter is not found";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			encoderKey = stoll(encoderKeyIt->second);

			_mmsEngineDBFacade->addAssociationWorkspaceEncoder(workspaceKey, encoderKey);

			sResponse = (string("{ ") + "\"workspaceKey\": " + to_string(workspaceKey) + ", \"encoderKey\": " + to_string(encoderKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addAssociationWorkspaceEncoder failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::removeAssociationWorkspaceEncoder(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeAssociationWorkspaceEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t workspaceKey;
			auto workspaceKeyIt = queryParameters.find("workspaceKey");
			if (workspaceKeyIt == queryParameters.end())
			{
				string errorMessage = "The 'workspaceKey' parameter is not found";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			workspaceKey = stoll(workspaceKeyIt->second);

			int64_t encoderKey;
			auto encoderKeyIt = queryParameters.find("encoderKey");
			if (encoderKeyIt == queryParameters.end())
			{
				string errorMessage = "The 'encoderKey' parameter is not found";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			encoderKey = stoll(encoderKeyIt->second);

			_mmsEngineDBFacade->removeAssociationWorkspaceEncoder(workspaceKey, encoderKey);

			sResponse = (string("{ ") + "\"workspaceKey\": " + to_string(workspaceKey) + ", \"encoderKey\": " + to_string(encoderKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeAssociationWorkspaceEncoder failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}