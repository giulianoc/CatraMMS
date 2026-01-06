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

using namespace std;
using json = nlohmann::json;

void API::addInvoice(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData)
{
	string api = "addInvoice";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
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
		int64_t userKey;
		string description;
		int64_t amount;
		string expirationDate;

		try
		{
			json requestBodyRoot = JSONUtils::toJson<json>(requestData.requestBody);

			string field = "userKey";
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
			userKey = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "description";
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
			description = JSONUtils::asString(requestBodyRoot, field, "");

			field = "amount";
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
			amount = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "expirationDate";
			if (JSONUtils::isPresent(requestBodyRoot, field))
				expirationDate = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format",
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

void API::invoiceList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "invoiceList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO("Received {}", api);

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
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
#ifdef __POSTGRES__
			json invoiceListRoot = _mmsEngineDBFacade->getInvoicesList(apiAuthorizationDetails->userKey, apiAuthorizationDetails->admin, start, rows);

			string responseBody = JSONUtils::toString(invoiceListRoot);
#else
			string responseBody;
#endif

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
