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

void API::addDeliveryServer(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "addDeliveryServer";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
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
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		string label;
		string type;
		optional<int64_t> originDeliveryServerKey;
		bool external;
		bool enabled;
		string publicServerName;
		string internalServerName;

		try
		{
			auto requestBodyRoot = JSONUtils::toJson<json>(requestData.requestBody);

			label = JSONUtils::as<string>(requestBodyRoot, "label", "", {}, true);
			type = JSONUtils::as<string>(requestBodyRoot, "type", "origin",
				{"origin", "edge", "mid-origin"}, true);
			if (type == "edge" || type == "mid-origin")
				originDeliveryServerKey = JSONUtils::as<int64_t>(requestBodyRoot, "originDeliveryServerKey", -1,
					{}, true);
			external = JSONUtils::as<bool>(requestBodyRoot, "external", false);
			enabled = JSONUtils::as<bool>(requestBodyRoot, "enabled", true);
			publicServerName = JSONUtils::as<string>(requestBodyRoot, "publicServerName", "", {}, true);
			internalServerName = JSONUtils::as<string>(requestBodyRoot, "internalServerName", "", {}, true);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format"
				", requestData.requestBody: {}"
				", e.what(): {}",
				requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		json responseRoot;
		try
		{
			int64_t deliveryServerKey = _mmsEngineDBFacade->addDeliveryServer(label, type, originDeliveryServerKey,
				external, enabled, publicServerName, internalServerName);

			responseRoot["deliveryServerKey"] = deliveryServerKey;
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->addDeliveryServer failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201,
			JSONUtils::toString(responseRoot));
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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::modifyDeliveryServer(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "modifyDeliveryServer";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
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
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		optional<string> label;
		optional<string> type;
		optional<int64_t> originDeliveryServerKey;
		optional<bool> external;
		optional<bool> enabled;
		optional<string> publicServerName;
		optional<string> internalServerName;

		try
		{
			auto requestBodyRoot = JSONUtils::toJson<json>(requestData.requestBody);

			label = JSONUtils::asOpt<string>(requestBodyRoot, "label");
			type = JSONUtils::asOpt<string>(requestBodyRoot, "type", {"origin", "edge"});
			originDeliveryServerKey = JSONUtils::asOpt<int64_t>(requestBodyRoot, "originDeliveryServerKey");
			external = JSONUtils::asOpt<bool>(requestBodyRoot, "external");
			enabled = JSONUtils::asOpt<bool>(requestBodyRoot, "enabled");
			publicServerName = JSONUtils::asOpt<string>(requestBodyRoot, "publicServerName");
			internalServerName = JSONUtils::asOpt<string>(requestBodyRoot, "internalServerName");
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format"
				", requestData.requestBody: {}"
				", e.what(): {}",
				requestData.requestBody, e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		json responseRoot;
		try
		{
			int64_t deliveryServerKey = requestData.getQueryParameter("deliveryServerKey", -1, true);

			_mmsEngineDBFacade->modifyDeliveryServer(
				deliveryServerKey, label, type, originDeliveryServerKey, external, enabled,
				publicServerName, internalServerName
			);

			responseRoot["deliveryServerKey"] = deliveryServerKey;
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->modifyDeliveryServer failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
			JSONUtils::toString(responseRoot));
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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::updateDeliveryServerBandwidthStats(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "updateDeliveryServerBandwidthStats";

	const shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canUpdateEncoderAndDeliveryStats)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	int64_t deliveryServerKey;
	try
	{
		json responseRoot;
		try
		{
			deliveryServerKey = requestData.getQueryParameter<int64_t>("deliveryServerKey", -1, true);
			auto txAvgBandwidthUsage = requestData.getQueryParameter<uint64_t>("txAvgBandwidthUsage", 0, true);
			auto rxAvgBandwidthUsage = requestData.getQueryParameter<uint64_t>("rxAvgBandwidthUsage", 0, true);

			_mmsEngineDBFacade->updateDeliveryServerAvgBandwidthUsage(deliveryServerKey, txAvgBandwidthUsage, rxAvgBandwidthUsage);

			responseRoot["deliveryServerKey"] = deliveryServerKey;
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->updateDeliveryServerBandwidthStats failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
			JSONUtils::toString(responseRoot));
	}
	catch (exception &e)
	{
		const string errorMessage = std::format(
			"API failed"
			", API: {}"
			", deliveryServerKey: {}"
			", e.what(): {}",
			api, deliveryServerKey, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::updateDeliveryServerCPUUsageStats(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
	)
{
	string api = "updateDeliveryServerCPUUsageStats";

	const shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails =
		static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canUpdateEncoderAndDeliveryStats)
	{
		const string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	int64_t deliveryServerKey;
	try
	{
		json responseRoot;
		try
		{
			deliveryServerKey = requestData.getQueryParameter<int64_t>("deliveryServerKey", -1, true);
			auto cpuUsage = requestData.getQueryParameter<uint16_t>("cpuUsage", 0, true);

			_mmsEngineDBFacade->updateDeliveryServerCPUUsage(deliveryServerKey, cpuUsage);

			responseRoot["deliveryServerKey"] = deliveryServerKey;
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->updateDeliveryServerCPUUsageStats failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
			JSONUtils::toString(responseRoot));
	}
	catch (exception &e)
	{
		const string errorMessage = std::format(
			"API failed"
			", API: {}"
			", deliveryServerKey: {}"
			", e.what(): {}",
			api, deliveryServerKey, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::removeDeliveryServer(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "removeDeliveryServer";

	const shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
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
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		json responseRoot;
		try
		{
			int64_t deliveryServerKey = requestData.getQueryParameter("deliveryServerKey", static_cast<int64_t>(-1), true);

			_mmsEngineDBFacade->removeDeliveryServer(deliveryServerKey);

			responseRoot["deliveryServerKey"] = deliveryServerKey;
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->removeDeliveryServer failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
			JSONUtils::toString(responseRoot));
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::deliveryServerList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "deliveryServerList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		auto deliveryServerKey = requestData.getOptQueryParameter<int64_t>("deliveryServerKey");

		auto start = requestData.getQueryParameter<int32_t>("start", 0);
		auto rows = requestData.getQueryParameter<int32_t>("rows", 30);
		if (rows > _maxPageSize)
		{
			// 2022-02-13: changed to return an error otherwise the user
			//	think to ask for a huge number of items while the return is much less

			// rows = _maxPageSize;

			string errorMessage = std::format("rows parameter too big"
				", rows: {}"
				", _maxPageSize: {}", rows, _maxPageSize);
			LOG_ERROR(errorMessage);
			throw runtime_error(errorMessage);
		}

		optional<string> label = requestData.getOptQueryParameter<string>("label");
		optional<string> serverName = requestData.getOptQueryParameter<string>("serverName");
		optional<string> type = requestData.getOptQueryParameter<string>("type", {"origin", "mid-origin", "edge"});
		string labelOrder = requestData.getQueryParameter("labelOrder");
		if (!labelOrder.empty() && labelOrder != "asc" && labelOrder != "desc")
		{
			LOG_WARN(
				"encoderList: 'labelOrder' parameter is unknown"
				", labelOrder: {}",
				labelOrder
			);
			labelOrder = "";
		}

		int64_t workspaceKey = apiAuthorizationDetails->workspace->_workspaceKey;
		bool allDeliveryServers = false;
		if (apiAuthorizationDetails->admin)
		{
			// in case of admin, from the GUI, it is needed to:
			// - get the list of all encoders
			// - encoders for a specific workspace

			allDeliveryServers = requestData.getQueryParameter("allDeliveryServers", false);
			workspaceKey = requestData.getQueryParameter("workspaceKey", apiAuthorizationDetails->workspace->_workspaceKey);
		}

		{
			json deliveryServerListRoot = _mmsEngineDBFacade->getDeliveryServerList(
				apiAuthorizationDetails->admin, start, rows, allDeliveryServers, workspaceKey, deliveryServerKey, label, serverName,
				type, labelOrder
			);

			string responseBody = JSONUtils::toString(deliveryServerListRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
				responseBody);
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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::deliveryServersPoolList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "deliveryServersPoolList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t deliveryServersPoolKey = requestData.getQueryParameter("deliveryServersPoolKey", static_cast<int64_t>(-1));

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
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string label = requestData.getQueryParameter("label", "");
		string labelOrder = requestData.getQueryParameter("labelOrder", "");
		if (!labelOrder.empty() && labelOrder != "asc" && labelOrder != "desc")
		{
			LOG_WARN(
				"encodersPoolList: 'labelOrder' parameter is unknown"
				", labelOrder: {}",
				labelOrder
			);
			labelOrder = "";
		}

		{
			json deliveryServersPoolListRoot =
				_mmsEngineDBFacade->getDeliveryServersPoolList(start, rows, apiAuthorizationDetails->workspace->_workspaceKey,
					deliveryServersPoolKey, label, labelOrder);

			string responseBody = JSONUtils::toString(deliveryServersPoolListRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
				responseBody);
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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::addDeliveryServersPool(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "addDeliveryServersPool";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditEncodersPool)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditDeliveryServersPool: {}",
			apiAuthorizationDetails->canEditDeliveryServersPool
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		string label;
		vector<int64_t> deliveryServerKeys;

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
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::as<string>(requestBodyRoot, field, "");

			field = "deliveryServerKeys";
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				json deliveryServerKeysRoot = requestBodyRoot[field];

				for (int deliveryServerIndex = 0; deliveryServerIndex < deliveryServerKeysRoot.size(); ++deliveryServerIndex)
					deliveryServerKeys.push_back(JSONUtils::as<int64_t>(deliveryServerKeysRoot[deliveryServerIndex]));
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format"
				", requestData.requestBody: {}",
				requestData.requestBody
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t deliveryServersPoolKey = _mmsEngineDBFacade->addDeliveryServersPool(apiAuthorizationDetails->workspace->_workspaceKey,
				label, deliveryServerKeys);

			sResponse = (string("{ ") + "\"DeliveryServersPoolKey\": " + to_string(deliveryServersPoolKey) + "}");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->addDeliveryServersPool failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201,
			sResponse);
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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::modifyDeliveryServersPool(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "modifyDeliveryServersPool";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditEncodersPool)
	{
		string errorMessage = string(
			"APIKey does not have the permission"
			", canEditDeliveryServersPool: {}",
			apiAuthorizationDetails->canEditDeliveryServersPool
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		string label;
		vector<int64_t> deliveryServerKeys;

		int64_t deliveryServersPoolKey = requestData.getQueryParameter("deliveryServersPoolKey", static_cast<int64_t>(-1), true);

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
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::as<string>(requestBodyRoot, field, "");

			field = "deliveryServerKeys";
			if (JSONUtils::isPresent(requestBodyRoot, field))
			{
				json deliveryServerKeysRoot = requestBodyRoot[field];

				for (int deliveryServerIndex = 0; deliveryServerIndex < deliveryServerKeysRoot.size(); ++deliveryServerIndex)
					deliveryServerKeys.push_back(JSONUtils::as<int64_t>(deliveryServerKeysRoot[deliveryServerIndex]));
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
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			_mmsEngineDBFacade->modifyDeliveryServersPool(deliveryServersPoolKey, apiAuthorizationDetails->workspace->_workspaceKey,
				label, deliveryServerKeys);

			sResponse = (string("{ ") + "\"DeliveryServersPoolKey\": " + to_string(deliveryServersPoolKey) + "}");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->modifyDeliveryServersPool failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201,
			sResponse);
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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::removeDeliveryServersPool(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "removeDeliveryServersPool";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditDeliveryServersPool)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditEncodersPool: {}",
			apiAuthorizationDetails->canEditEncodersPool
		);
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t deliveryServersPoolKey = requestData.getQueryParameter("deliveryServersPoolKey", static_cast<int64_t>(-1), true);

			_mmsEngineDBFacade->removeDeliveryServersPool(deliveryServersPoolKey);

			sResponse = (string("{ ") + "\"deliveryServersPoolKey\": " + to_string(deliveryServersPoolKey) + "}");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->removeDeliveryServersPool failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
			sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::addAssociationWorkspaceDeliveryServer(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "addAssociationWorkspaceDeliveryServer";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
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
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t workspaceKey = requestData.getQueryParameter("workspaceKey", static_cast<int64_t>(-1), true);

			int64_t deliveryServerKey = requestData.getQueryParameter("deliveryServerKey", static_cast<int64_t>(-1), true);

			_mmsEngineDBFacade->addAssociationWorkspaceDeliveryServer(workspaceKey, deliveryServerKey);

			sResponse = (string("{ ") + "\"workspaceKey\": " + to_string(workspaceKey) + ", \"deliveryServerKey\": " + to_string(deliveryServerKey) + "}");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->addAssociationWorkspaceDeliveryServer failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
			sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::removeAssociationWorkspaceDeliveryServer(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "removeAssociationWorkspaceDeliveryServer";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
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
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t workspaceKey = requestData.getQueryParameter("workspaceKey", static_cast<int64_t>(-1), true);

			int64_t deliveryServerKey = requestData.getQueryParameter("deliveryServerKey", static_cast<int64_t>(-1), true);

			_mmsEngineDBFacade->removeAssociationWorkspaceDeliveryServer(workspaceKey, deliveryServerKey);

			sResponse = (string("{ ") + "\"workspaceKey\": " + to_string(workspaceKey) + ", \"deliveryServerKey\": " + to_string(deliveryServerKey) + "}");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->removeAssociationWorkspaceDeliveryServer failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200,
			sResponse);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		LOG_ERROR(errorMessage);
		throw;
	}
}
