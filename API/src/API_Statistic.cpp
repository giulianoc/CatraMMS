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
#include <regex>

using namespace std;
using json = nlohmann::json;

void API::addRequestStatistic(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "addRequestStatistic";

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
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		string userId;
		string ipAddress;
		int64_t physicalPathKey = -1;
		int64_t confStreamKey = -1;
		string title;

		try
		{
			json requestBodyRoot = JSONUtils::toJson<json>(requestData.requestBody);

			string field = "userId";
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
			userId = JSONUtils::asString(requestBodyRoot, field, "");

			field = "ipAddress";
			if (JSONUtils::isPresent(requestBodyRoot, field))
				ipAddress = JSONUtils::asString(requestBodyRoot, field, "");

			field = "physicalPathKey";
			physicalPathKey = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "confStreamKey";
			confStreamKey = JSONUtils::asInt64(requestBodyRoot, field, -1);

			if (physicalPathKey == -1 && confStreamKey == -1)
			{
				string errorMessage = std::format(
					"physicalPathKey or confStreamKey has to be present"
					", Field: {}",
					field
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "title";
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
			title = JSONUtils::asString(requestBodyRoot, field, "");
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
			json statisticRoot =
				_mmsEngineDBFacade->addRequestStatistic(apiAuthorizationDetails->workspace->_workspaceKey, ipAddress, userId, physicalPathKey, confStreamKey, title);

			sResponse = JSONUtils::toString(statisticRoot);
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->addRequestStatistic failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::requestStatisticList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "requestStatisticList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
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

		string userId = requestData.getQueryParameter("userId", "");

		string title = requestData.getQueryParameter("title", "");

		string startStatisticDate = requestData.getQueryParameter("startStatisticDate", "", true);
		string endStatisticDate = requestData.getQueryParameter("endStatisticDate", "", true);

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticList(
				apiAuthorizationDetails->workspace->_workspaceKey, userId, title, startStatisticDate, endStatisticDate, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::requestStatisticPerContentList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "requestStatisticPerContentList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
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

		string userId = requestData.getQueryParameter("userId", "");

		string title = requestData.getQueryParameter("title", "");

		string startStatisticDate = requestData.getQueryParameter("startStatisticDate", "", true);
		string endStatisticDate = requestData.getQueryParameter("endStatisticDate", "", true);

		int32_t minimalNextRequestDistanceInSeconds = requestData.getQueryParameter("minimalNextRequestDistanceInSeconds",
			static_cast<int32_t>(-1));

		bool totalNumFoundToBeCalculated = requestData.getQueryParameter("totalNumFoundToBeCalculated", false);

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerContentList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::requestStatisticPerUserList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "requestStatisticPerUserList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
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

		string userId = requestData.getQueryParameter("userId", "");

		string title = requestData.getQueryParameter("title", "");

		string startStatisticDate = requestData.getQueryParameter("startStatisticDate", "", true);
		string endStatisticDate = requestData.getQueryParameter("endStatisticDate", "", true);

		int32_t minimalNextRequestDistanceInSeconds = requestData.getQueryParameter("minimalNextRequestDistanceInSeconds",
			static_cast<int32_t>(-1));

		bool totalNumFoundToBeCalculated = requestData.getQueryParameter("totalNumFoundToBeCalculated", false);

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerUserList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::requestStatisticPerMonthList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "requestStatisticPerMonthList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
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

		string userId = requestData.getQueryParameter("userId", "");

		string title = requestData.getQueryParameter("title", "");

		string startStatisticDate = requestData.getQueryParameter("startStatisticDate", "", true);
		string endStatisticDate = requestData.getQueryParameter("endStatisticDate", "", true);

		int32_t minimalNextRequestDistanceInSeconds = requestData.getQueryParameter("minimalNextRequestDistanceInSeconds",
			static_cast<int32_t>(-1));

		bool totalNumFoundToBeCalculated = requestData.getQueryParameter("totalNumFoundToBeCalculated", false);

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerMonthList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::requestStatisticPerDayList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "requestStatisticPerDayList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
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

		string userId = requestData.getQueryParameter("userId", "");

		string title = requestData.getQueryParameter("title", "");

		string startStatisticDate = requestData.getQueryParameter("startStatisticDate", "", true);
		string endStatisticDate = requestData.getQueryParameter("endStatisticDate", "", true);

		int32_t minimalNextRequestDistanceInSeconds = requestData.getQueryParameter("minimalNextRequestDistanceInSeconds",
			static_cast<int32_t>(-1));

		bool totalNumFoundToBeCalculated = requestData.getQueryParameter("totalNumFoundToBeCalculated", false);

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerDayList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::requestStatisticPerHourList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "requestStatisticPerHourList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
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

		string userId = requestData.getQueryParameter("userId", "");

		string title = requestData.getQueryParameter("title", "");

		string startStatisticDate = requestData.getQueryParameter("startStatisticDate", "", true);
		string endStatisticDate = requestData.getQueryParameter("endStatisticDate", "", true);

		int32_t minimalNextRequestDistanceInSeconds = requestData.getQueryParameter("minimalNextRequestDistanceInSeconds",
			static_cast<int32_t>(-1));

		bool totalNumFoundToBeCalculated = requestData.getQueryParameter("totalNumFoundToBeCalculated", false);

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerHourList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::requestStatisticPerCountryList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "requestStatisticPerCountryList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
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

		string userId = requestData.getQueryParameter("userId", "");

		string title = requestData.getQueryParameter("title", "");

		string startStatisticDate = requestData.getQueryParameter("startStatisticDate", "", true);
		string endStatisticDate = requestData.getQueryParameter("endStatisticDate", "", true);

		int32_t minimalNextRequestDistanceInSeconds = requestData.getQueryParameter("minimalNextRequestDistanceInSeconds",
			static_cast<int32_t>(-1));

		bool totalNumFoundToBeCalculated = requestData.getQueryParameter("totalNumFoundToBeCalculated", false);

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerCountryList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::loginStatisticList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "loginStatisticList";

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
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
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

		string startStatisticDate = requestData.getQueryParameter("startStatisticDate", "", true);
		string endStatisticDate = requestData.getQueryParameter("endStatisticDate", "", true);

		{
			json statisticsListRoot = _mmsEngineDBFacade->getLoginStatisticList(startStatisticDate, endStatisticDate, start, rows);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}
