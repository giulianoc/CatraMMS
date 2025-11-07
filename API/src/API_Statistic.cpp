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

void API::addRequestStatistic(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addRequestStatistic";

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
		string userId;
		string ipAddress;
		int64_t physicalPathKey = -1;
		int64_t confStreamKey = -1;
		string title;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "userId";
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
			userId = JSONUtils::asString(requestBodyRoot, field, "");

			field = "ipAddress";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
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
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "title";
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
			title = JSONUtils::asString(requestBodyRoot, field, "");
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
			json statisticRoot =
				_mmsEngineDBFacade->addRequestStatistic(apiAuthorizationDetails->workspace->_workspaceKey, ipAddress, userId, physicalPathKey, confStreamKey, title);

			sResponse = JSONUtils::toString(statisticRoot);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addRequestStatistic failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
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
		throw HTTPError(500);
	}
}

void API::requestStatisticList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "requestStatisticList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
			start = stoll(startIt->second);

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
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

		string userId;
		auto userIdIt = queryParameters.find("userId");
		if (userIdIt != queryParameters.end() && userIdIt->second != "")
		{
			userId = userIdIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(userId, regex(plus), plusDecoded);

			userId = CurlWrapper::unescape(firstDecoding);
		}

		string title;
		auto titleIt = queryParameters.find("title");
		if (titleIt != queryParameters.end() && titleIt->second != "")
		{
			title = titleIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(title, regex(plus), plusDecoded);

			title = CurlWrapper::unescape(firstDecoding);
		}

		string startStatisticDate;
		auto startStatisticDateIt = queryParameters.find("startStatisticDate");
		if (startStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'startStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		startStatisticDate = startStatisticDateIt->second;

		string endStatisticDate;
		auto endStatisticDateIt = queryParameters.find("endStatisticDate");
		if (endStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'endStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		endStatisticDate = endStatisticDateIt->second;

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticList(
				apiAuthorizationDetails->workspace->_workspaceKey, userId, title, startStatisticDate, endStatisticDate, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		throw HTTPError(500);
	}
}

void API::requestStatisticPerContentList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "requestStatisticPerContentList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
			start = stoll(startIt->second);

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
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

		string title;
		auto titleIt = queryParameters.find("title");
		if (titleIt != queryParameters.end() && titleIt->second != "")
		{
			title = titleIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(title, regex(plus), plusDecoded);

			title = CurlWrapper::unescape(firstDecoding);
		}

		string userId;
		auto userIdIt = queryParameters.find("userId");
		if (userIdIt != queryParameters.end() && userIdIt->second != "")
		{
			userId = userIdIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(userId, regex(plus), plusDecoded);

			userId = CurlWrapper::unescape(firstDecoding);
		}

		// dates are essential in order to make the indexes working
		string startStatisticDate;
		auto startStatisticDateIt = queryParameters.find("startStatisticDate");
		if (startStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'startStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		startStatisticDate = startStatisticDateIt->second;

		string endStatisticDate;
		auto endStatisticDateIt = queryParameters.find("endStatisticDate");
		if (endStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'endStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		endStatisticDate = endStatisticDateIt->second;

		int minimalNextRequestDistanceInSeconds = -1;
		auto minimalIt = queryParameters.find("minimalNextRequestDistanceInSeconds");
		if (minimalIt != queryParameters.end() && minimalIt->second != "")
			minimalNextRequestDistanceInSeconds = stoll(minimalIt->second);

		bool totalNumFoundToBeCalculated = false;
		auto totalNumFoundIt = queryParameters.find("totalNumFoundToBeCalculated");
		if (totalNumFoundIt != queryParameters.end() && totalNumFoundIt->second != "")
			totalNumFoundToBeCalculated = totalNumFoundIt->second == "true" ? true : false;

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerContentList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		throw HTTPError(500);
	}
}

void API::requestStatisticPerUserList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "requestStatisticPerUserList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
		{
			start = stoll(startIt->second);
		}

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
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

		string title;
		auto titleIt = queryParameters.find("title");
		if (titleIt != queryParameters.end() && titleIt->second != "")
		{
			title = titleIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(title, regex(plus), plusDecoded);

			title = CurlWrapper::unescape(firstDecoding);
		}

		string userId;
		auto userIdIt = queryParameters.find("userId");
		if (userIdIt != queryParameters.end() && userIdIt->second != "")
		{
			userId = userIdIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(userId, regex(plus), plusDecoded);

			userId = CurlWrapper::unescape(firstDecoding);
		}

		string startStatisticDate;
		auto startStatisticDateIt = queryParameters.find("startStatisticDate");
		if (startStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'startStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		startStatisticDate = startStatisticDateIt->second;

		string endStatisticDate;
		auto endStatisticDateIt = queryParameters.find("endStatisticDate");
		if (endStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'endStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		endStatisticDate = endStatisticDateIt->second;

		int minimalNextRequestDistanceInSeconds = -1;
		auto minimalIt = queryParameters.find("minimalNextRequestDistanceInSeconds");
		if (minimalIt != queryParameters.end() && minimalIt->second != "")
			minimalNextRequestDistanceInSeconds = stoll(minimalIt->second);

		bool totalNumFoundToBeCalculated = false;
		auto totalNumFoundIt = queryParameters.find("totalNumFoundToBeCalculated");
		if (totalNumFoundIt != queryParameters.end() && totalNumFoundIt->second != "")
			totalNumFoundToBeCalculated = totalNumFoundIt->second == "true" ? true : false;

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerUserList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		throw HTTPError(500);
	}
}

void API::requestStatisticPerMonthList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "requestStatisticPerMonthList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
		{
			start = stoll(startIt->second);
		}

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
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

		string title;
		auto titleIt = queryParameters.find("title");
		if (titleIt != queryParameters.end() && titleIt->second != "")
		{
			title = titleIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(title, regex(plus), plusDecoded);

			title = CurlWrapper::unescape(firstDecoding);
		}

		string userId;
		auto userIdIt = queryParameters.find("userId");
		if (userIdIt != queryParameters.end() && userIdIt->second != "")
		{
			userId = userIdIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(userId, regex(plus), plusDecoded);

			userId = CurlWrapper::unescape(firstDecoding);
		}

		string startStatisticDate;
		auto startStatisticDateIt = queryParameters.find("startStatisticDate");
		if (startStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'startStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		startStatisticDate = startStatisticDateIt->second;

		string endStatisticDate;
		auto endStatisticDateIt = queryParameters.find("endStatisticDate");
		if (endStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'endStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		endStatisticDate = endStatisticDateIt->second;

		int minimalNextRequestDistanceInSeconds = -1;
		auto minimalIt = queryParameters.find("minimalNextRequestDistanceInSeconds");
		if (minimalIt != queryParameters.end() && minimalIt->second != "")
			minimalNextRequestDistanceInSeconds = stoll(minimalIt->second);

		bool totalNumFoundToBeCalculated = false;
		auto totalNumFoundIt = queryParameters.find("totalNumFoundToBeCalculated");
		if (totalNumFoundIt != queryParameters.end() && totalNumFoundIt->second != "")
			totalNumFoundToBeCalculated = totalNumFoundIt->second == "true" ? true : false;

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerMonthList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		throw HTTPError(500);
	}
}

void API::requestStatisticPerDayList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "requestStatisticPerDayList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
		{
			start = stoll(startIt->second);
		}

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
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

		string title;
		auto titleIt = queryParameters.find("title");
		if (titleIt != queryParameters.end() && titleIt->second != "")
		{
			title = titleIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(title, regex(plus), plusDecoded);

			title = CurlWrapper::unescape(firstDecoding);
		}

		string userId;
		auto userIdIt = queryParameters.find("userId");
		if (userIdIt != queryParameters.end() && userIdIt->second != "")
		{
			userId = userIdIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(userId, regex(plus), plusDecoded);

			userId = CurlWrapper::unescape(firstDecoding);
		}

		string startStatisticDate;
		auto startStatisticDateIt = queryParameters.find("startStatisticDate");
		if (startStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'startStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		startStatisticDate = startStatisticDateIt->second;

		string endStatisticDate;
		auto endStatisticDateIt = queryParameters.find("endStatisticDate");
		if (endStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'endStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		endStatisticDate = endStatisticDateIt->second;

		int minimalNextRequestDistanceInSeconds = -1;
		auto minimalIt = queryParameters.find("minimalNextRequestDistanceInSeconds");
		if (minimalIt != queryParameters.end() && minimalIt->second != "")
			minimalNextRequestDistanceInSeconds = stoll(minimalIt->second);

		bool totalNumFoundToBeCalculated = false;
		auto totalNumFoundIt = queryParameters.find("totalNumFoundToBeCalculated");
		if (totalNumFoundIt != queryParameters.end() && totalNumFoundIt->second != "")
			totalNumFoundToBeCalculated = totalNumFoundIt->second == "true" ? true : false;

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerDayList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		throw HTTPError(500);
	}
}

void API::requestStatisticPerHourList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "requestStatisticPerHourList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
		{
			start = stoll(startIt->second);
		}

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
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

		string title;
		auto titleIt = queryParameters.find("title");
		if (titleIt != queryParameters.end() && titleIt->second != "")
		{
			title = titleIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(title, regex(plus), plusDecoded);

			title = CurlWrapper::unescape(firstDecoding);
		}

		string userId;
		auto userIdIt = queryParameters.find("userId");
		if (userIdIt != queryParameters.end() && userIdIt->second != "")
		{
			userId = userIdIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(userId, regex(plus), plusDecoded);

			userId = CurlWrapper::unescape(firstDecoding);
		}

		string startStatisticDate;
		auto startStatisticDateIt = queryParameters.find("startStatisticDate");
		if (startStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'startStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		startStatisticDate = startStatisticDateIt->second;

		string endStatisticDate;
		auto endStatisticDateIt = queryParameters.find("endStatisticDate");
		if (endStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'endStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		endStatisticDate = endStatisticDateIt->second;

		int minimalNextRequestDistanceInSeconds = -1;
		auto minimalIt = queryParameters.find("minimalNextRequestDistanceInSeconds");
		if (minimalIt != queryParameters.end() && minimalIt->second != "")
			minimalNextRequestDistanceInSeconds = stoll(minimalIt->second);

		bool totalNumFoundToBeCalculated = false;
		auto totalNumFoundIt = queryParameters.find("totalNumFoundToBeCalculated");
		if (totalNumFoundIt != queryParameters.end() && totalNumFoundIt->second != "")
			totalNumFoundToBeCalculated = totalNumFoundIt->second == "true" ? true : false;

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerHourList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		throw HTTPError(500);
	}
}

void API::requestStatisticPerCountryList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "requestStatisticPerCountryList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
		{
			start = stoll(startIt->second);
		}

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
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

		string title;
		auto titleIt = queryParameters.find("title");
		if (titleIt != queryParameters.end() && titleIt->second != "")
		{
			title = titleIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(title, regex(plus), plusDecoded);

			title = CurlWrapper::unescape(firstDecoding);
		}

		string userId;
		auto userIdIt = queryParameters.find("userId");
		if (userIdIt != queryParameters.end() && userIdIt->second != "")
		{
			userId = userIdIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(userId, regex(plus), plusDecoded);

			userId = CurlWrapper::unescape(firstDecoding);
		}

		string startStatisticDate;
		auto startStatisticDateIt = queryParameters.find("startStatisticDate");
		if (startStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'startStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		startStatisticDate = startStatisticDateIt->second;

		string endStatisticDate;
		auto endStatisticDateIt = queryParameters.find("endStatisticDate");
		if (endStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'endStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		endStatisticDate = endStatisticDateIt->second;

		int minimalNextRequestDistanceInSeconds = -1;
		auto minimalIt = queryParameters.find("minimalNextRequestDistanceInSeconds");
		if (minimalIt != queryParameters.end() && minimalIt->second != "")
			minimalNextRequestDistanceInSeconds = stoll(minimalIt->second);

		bool totalNumFoundToBeCalculated = false;
		auto totalNumFoundIt = queryParameters.find("totalNumFoundToBeCalculated");
		if (totalNumFoundIt != queryParameters.end() && totalNumFoundIt->second != "")
			totalNumFoundToBeCalculated = totalNumFoundIt->second == "true" ? true : false;

		{
			json statisticsListRoot = _mmsEngineDBFacade->getRequestStatisticPerCountryList(
				apiAuthorizationDetails->workspace->_workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds,
				totalNumFoundToBeCalculated, start, rows
			);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		throw HTTPError(500);
	}
}

void API::loginStatisticList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "loginStatisticList";

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
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
		{
			start = stoll(startIt->second);
		}

		int rows = 30;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && rowsIt->second != "")
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

		string startStatisticDate;
		auto startStatisticDateIt = queryParameters.find("startStatisticDate");
		if (startStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'startStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		startStatisticDate = startStatisticDateIt->second;

		string endStatisticDate;
		auto endStatisticDateIt = queryParameters.find("endStatisticDate");
		if (endStatisticDateIt == queryParameters.end())
		{
			string errorMessage = "'endStatisticDate' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		endStatisticDate = endStatisticDateIt->second;

		{
			json statisticsListRoot = _mmsEngineDBFacade->getLoginStatisticList(startStatisticDate, endStatisticDate, start, rows);

			string responseBody = JSONUtils::toString(statisticsListRoot);

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
		throw HTTPError(500);
	}
}
