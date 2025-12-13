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
#include "JSONUtils.h"
#include "spdlog/spdlog.h"
#include <regex>

void API::addInvoice(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters)
{
	string api = "addInvoice";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
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
		int64_t userKey;
		string description;
		int64_t amount;
		string expirationDate;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "userKey";
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
			userKey = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "description";
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
			description = JSONUtils::asString(requestBodyRoot, field, "");

			field = "amount";
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
			amount = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "expirationDate";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				expirationDate = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format",
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
#ifdef __POSTGRES__
			int64_t invoiceKey = _mmsEngineDBFacade->addInvoice(userKey, description, amount, expirationDate);
#else
#endif

			json response;
#ifdef __POSTGRES__
			response["invoiceKey"] = invoiceKey;
#else
#endif

			sResponse = JSONUtils::toString(response);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addInvoice failed"
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

void API::invoiceList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "invoiceList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO("Received {}", api);

	try
	{
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

		{
#ifdef __POSTGRES__
			json invoiceListRoot = _mmsEngineDBFacade->getInvoicesList(apiAuthorizationDetails->userKey, apiAuthorizationDetails->admin, start, rows);

			string responseBody = JSONUtils::toString(invoiceListRoot);
#else
			string responseBody;
#endif

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