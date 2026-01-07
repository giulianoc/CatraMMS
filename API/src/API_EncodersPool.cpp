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

using namespace std;
using json = nlohmann::json;

void API::addEncoder(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "addEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
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
			json requestBodyRoot = JSONUtils::toJson<json>(requestData.requestBody);

			string field = "label";
			if (!JSONUtils::isPresent(requestBodyRoot, field))
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
			if (!JSONUtils::isPresent(requestBodyRoot, field))
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
			if (!JSONUtils::isPresent(requestBodyRoot, field))
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
			if (!JSONUtils::isPresent(requestBodyRoot, field))
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
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				port = JSONUtils::asInt32(requestBodyRoot, field, 80);
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
				", requestData.requestBody: {}"
				", e.what(): {}",
				requestData.requestBody, e.what()
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

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::modifyEncoder(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "modifyEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
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
			json requestBodyRoot = JSONUtils::toJson<json>(requestData.requestBody);

			string field = "label";
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				label = JSONUtils::asString(requestBodyRoot, field, "");
				labelToBeModified = true;
			}
			else
				labelToBeModified = false;

			field = "External";
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				external = JSONUtils::asBool(requestBodyRoot, field, false);
				externalToBeModified = true;
			}
			else
				externalToBeModified = false;

			field = "Enabled";
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				enabled = JSONUtils::asBool(requestBodyRoot, field, true);
				enabledToBeModified = true;
			}
			else
				enabledToBeModified = false;

			field = "Protocol";
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				protocol = JSONUtils::asString(requestBodyRoot, field, "");
				protocolToBeModified = true;
			}
			else
				protocolToBeModified = false;

			field = "PublicServerName";
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				publicServerName = JSONUtils::asString(requestBodyRoot, field, "");
				publicServerNameToBeModified = true;
			}
			else
				publicServerNameToBeModified = false;

			field = "InternalServerName";
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				internalServerName = JSONUtils::asString(requestBodyRoot, field, "");
				internalServerNameToBeModified = true;
			}
			else
				internalServerNameToBeModified = false;

			field = "Port";
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				port = JSONUtils::asInt32(requestBodyRoot, field, 80);
				portToBeModified = true;
			}
			else
				portToBeModified = false;
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format"
				", requestData.requestBody: {}"
				", e.what(): {}",
				requestData.requestBody, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t encoderKey = requestData.getQueryParameter("encoderKey", static_cast<int64_t>(-1), true);

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

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::updateEncoderBandwidthStats(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "updateEncoderBandwidthStats";

	const shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canUpdateEncoderStats)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	int64_t encoderKey;
	try
	{
		json response;
		try
		{
			encoderKey = requestData.getQueryParameter<int64_t>("encoderKey", -1, true);
			auto txAvgBandwidthUsage = requestData.getQueryParameter<uint64_t>("txAvgBandwidthUsage", 0, true);
			auto rxAvgBandwidthUsage = requestData.getQueryParameter<uint64_t>("rxAvgBandwidthUsage", 0, true);

			_mmsEngineDBFacade->updateEncoderAvgBandwidthUsage(encoderKey, txAvgBandwidthUsage, rxAvgBandwidthUsage);

			response["encoderKey"] = encoderKey;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->updateEncoderBandwidthStats failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
			JSONUtils::toString(response));
	}
	catch (exception &e)
	{
		const string errorMessage = std::format(
			"API failed"
			", API: {}"
			", encoderKey: {}"
			", e.what(): {}",
			api, encoderKey, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::updateEncoderCPUUsageStats(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
	)
{
	string api = "updateEncoderCPUUsageStats";

	const shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails =
		static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canUpdateEncoderStats)
	{
		const string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	int64_t encoderKey;
	try
	{
		json response;
		try
		{
			encoderKey = requestData.getQueryParameter<int64_t>("encoderKey", -1, true);
			auto cpuUsage = requestData.getQueryParameter<uint32_t>("cpuUsage", 0, true);

			_mmsEngineDBFacade->updateEncoderCPUUsage(encoderKey, cpuUsage);

			response["encoderKey"] = encoderKey;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->updateEncoderCPUUsageStats failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
			JSONUtils::toString(response));
	}
	catch (exception &e)
	{
		const string errorMessage = std::format(
			"API failed"
			", API: {}"
			", encoderKey: {}"
			", e.what(): {}",
			api, encoderKey, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::removeEncoder(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "removeEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

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
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t encoderKey = requestData.getQueryParameter("encoderKey", static_cast<int64_t>(-1), true);

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

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, sResponse);
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
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "encoderList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t encoderKey = requestData.getQueryParameter("encoderKey", static_cast<int64_t>(-1));

		int32_t start = requestData.getQueryParameter("start", static_cast<int32_t>(0));
		int32_t rows = requestData.getQueryParameter("rows", static_cast<int32_t>(30));
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

		string label = requestData.getQueryParameter("label", "");

		string serverName = requestData.getQueryParameter("serverName", "");

		int32_t port = requestData.getQueryParameter("port", static_cast<int64_t>(-1));

		string labelOrder = requestData.getQueryParameter("labelOrder", "");
		if (!labelOrder.empty() && labelOrder != "asc" && labelOrder != "desc")
		{
			SPDLOG_WARN(
				"encoderList: 'labelOrder' parameter is unknown"
				", labelOrder: {}",
				labelOrder
			);
			labelOrder = "";
		}

		bool runningInfo = requestData.getQueryParameter("runningInfo", false);

		int64_t workspaceKey = apiAuthorizationDetails->workspace->_workspaceKey;
		bool allEncoders = false;
		if (apiAuthorizationDetails->admin)
		{
			// in case of admin, from the GUI, it is needed to:
			// - get the list of all encoders
			// - encoders for a specific workspace

			allEncoders = requestData.getQueryParameter("allEncoders", false);
			workspaceKey = requestData.getQueryParameter("workspaceKey", apiAuthorizationDetails->workspace->_workspaceKey);
		}

		{
			json encoderListRoot = _mmsEngineDBFacade->getEncoderList(
				apiAuthorizationDetails->admin, start, rows, allEncoders, workspaceKey, runningInfo, encoderKey, label, serverName, port, labelOrder
			);

			string responseBody = JSONUtils::toString(encoderListRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
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
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "encoderList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t encodersPoolKey = requestData.getQueryParameter("encodersPoolKey", static_cast<int64_t>(-1));

		int32_t start = requestData.getQueryParameter("start", static_cast<int32_t>(0));
		int32_t rows = requestData.getQueryParameter("rows", static_cast<int32_t>(30));
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

		string label = requestData.getQueryParameter("label", "");
		string labelOrder = requestData.getQueryParameter("labelOrder", "");
		if (!labelOrder.empty() && labelOrder != "asc" && labelOrder != "desc")
		{
			SPDLOG_WARN(
				"encodersPoolList: 'labelOrder' parameter is unknown"
				", labelOrder: {}",
				labelOrder
			);
			labelOrder = "";
		}

		{
			SPDLOG_ERROR("allora 1ui 1");
			json encodersPoolListRoot =
				_mmsEngineDBFacade->getEncodersPoolList(start, rows, apiAuthorizationDetails->workspace->_workspaceKey, encodersPoolKey, label, labelOrder);
			SPDLOG_ERROR("allora 1ui 2");

			string responseBody = JSONUtils::toString(encodersPoolListRoot);
			SPDLOG_ERROR("allora 1ui 3");

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
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
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "addEncodersPool";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditEncodersPool)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditEncodersPool: {}",
			apiAuthorizationDetails->canEditEncodersPool
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		string label;
		vector<int64_t> encoderKeys;

		try
		{
			json requestBodyRoot = JSONUtils::toJson<json>(requestData.requestBody);

			string field = "label";
			if (!JSONUtils::isPresent(requestBodyRoot, field))
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
			if (JSONUtils::isPresent(requestBodyRoot, field))
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
				", requestData.requestBody: {}",
				requestData.requestBody
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

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::modifyEncodersPool(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "modifyEncodersPool";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditEncodersPool)
	{
		string errorMessage = string(
			"APIKey does not have the permission"
			", canEditEncodersPool: {}",
			apiAuthorizationDetails->canEditEncodersPool
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		string label;
		vector<int64_t> encoderKeys;

		int64_t encodersPoolKey = requestData.getQueryParameter("encodersPoolKey", static_cast<int64_t>(-1), true);

		try
		{
			json requestBodyRoot = JSONUtils::toJson<json>(requestData.requestBody);

			string field = "label";
			if (!JSONUtils::isPresent(requestBodyRoot, field))
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
			if (JSONUtils::isPresent(requestBodyRoot, field))
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
				", requestData.requestBody: {}"
				", e.what(): {}",
				requestData.requestBody, e.what()
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

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::removeEncodersPool(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "removeEncodersPool";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

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
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t encodersPoolKey = requestData.getQueryParameter("encodersPoolKey", static_cast<int64_t>(-1), true);

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

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, sResponse);
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
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "addAssociationWorkspaceEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

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
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t workspaceKey = requestData.getQueryParameter("workspaceKey", static_cast<int64_t>(-1), true);

			int64_t encoderKey = requestData.getQueryParameter("encoderKey", static_cast<int64_t>(-1), true);

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

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, sResponse);
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
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "removeAssociationWorkspaceEncoder";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

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
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t workspaceKey = requestData.getQueryParameter("workspaceKey", static_cast<int64_t>(-1), true);

			int64_t encoderKey = requestData.getQueryParameter("encoderKey", static_cast<int64_t>(-1), true);

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

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, sResponse);
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
