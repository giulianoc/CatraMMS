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

#include "JSONUtils.h"
#include <regex>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "API.h"


void API::addInvoice(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request, unordered_map<string, string> queryParameters,
	string requestBody)
{
    string api = "addInvoice";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
		int64_t userKey;
		string description;
		int64_t amount;
		string expirationDate;

        try
        {
            Json::Value requestBodyRoot = JSONUtils::toJson(-1, -1, requestBody);

            string field = "userKey";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            userKey = JSONUtils::asInt64(requestBodyRoot, field, -1);            

            field = "description";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            description = JSONUtils::asString(requestBodyRoot, field, "");            

            field = "amount";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            amount = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "expirationDate";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				expirationDate = JSONUtils::asString(requestBodyRoot, field, "");            
        }
        catch(runtime_error& e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception& e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string sResponse;
        try
        {
			int64_t invoiceKey = _mmsEngineDBFacade->addInvoice(
				userKey, description, amount, expirationDate);

			Json::Value response;
			response["invoiceKey"] = invoiceKey;

			sResponse = JSONUtils::toString(response);
		}
		catch(runtime_error& e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addInvoice failed"
				+ ", e.what(): " + e.what()
			);

            throw e;
        }
        catch(exception& e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addInvoice failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 201, sResponse);
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

void API::invoiceList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request, int64_t userKey,
	unordered_map<string, string> queryParameters, bool admin)
{
    string api = "invoiceList";

    _logger->info(__FILEREF__ + "Received " + api
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

				string errorMessage = __FILEREF__ + "rows parameter too big"
					+ ", rows: " + to_string(rows)
					+ ", _maxPageSize: " + to_string(_maxPageSize)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        {
			Json::Value invoiceListRoot = _mmsEngineDBFacade->getInvoicesList(
				userKey, admin, start, rows); 

            string responseBody = JSONUtils::toString(invoiceListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, "", api, 200, responseBody);
		}
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error: ") + e.what();
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", e.what(): " + e.what()
        );

        string errorMessage = string("Internal server error");
        _logger->error(__FILEREF__ + errorMessage);

        sendError(request, 500, errorMessage);

        throw runtime_error(errorMessage);
    }
}

