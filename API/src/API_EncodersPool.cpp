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
#include "API.h"


void API::addEncoder(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request,
	shared_ptr<Workspace> workspace,
	string requestBody)
{
    string api = "addEncoder";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
		bool external;
		bool enabled;
        string protocol;
        string publicServerName;
        string internalServerName;
        int port;

        try
        {
            Json::Value requestBodyRoot = JSONUtils::toJson(-1, -1, requestBody);

            string field = "Label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = JSONUtils::asString(requestBodyRoot, field, "");            

            field = "External";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				external = requestBodyRoot.get(field, false).asBool();            
			else
				external = false;

            field = "Enabled";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				enabled = requestBodyRoot.get(field, true).asBool();            
			else
				enabled = true;

            field = "Protocol";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            protocol = JSONUtils::asString(requestBodyRoot, field, "");            

            field = "PublicServerName";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            publicServerName = JSONUtils::asString(requestBodyRoot, field, "");            

            field = "InternalServerName";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            internalServerName = JSONUtils::asString(requestBodyRoot, field, "");            


            field = "Port";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				port = requestBodyRoot.get(field, 80).asInt();            
            }    
			else
			{
				if (protocol == "http")
					port = 80;
				else if (protocol == "https")
					port = 443;
			}
        }
        catch(runtime_error e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
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
			int64_t encoderKey = _mmsEngineDBFacade->addEncoder(
                label, external, enabled, protocol, publicServerName, internalServerName, port);

            sResponse = (
                    string("{ ") 
                    + "\"EncoderKey\": " + to_string(encoderKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 201, sResponse);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::modifyEncoder(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyEncoder";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

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
            Json::Value requestBodyRoot = JSONUtils::toJson(-1, -1, requestBody);
            
            string field = "Label";
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
				external = requestBodyRoot.get(field, false).asBool();            
				externalToBeModified = true;
			}
			else
				externalToBeModified = false;

            field = "Enabled";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				enabled = requestBodyRoot.get(field, true).asBool();            
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
				port = requestBodyRoot.get(field, 80).asInt();            
				portToBeModified = true;
            }
			else
				portToBeModified = false;
        }
        catch(runtime_error e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
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
            int64_t encoderKey;
            auto encoderKeyIt = queryParameters.find("encoderKey");
            if (encoderKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'encoderKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            encoderKey = stoll(encoderKeyIt->second);

            _mmsEngineDBFacade->modifyEncoder(
                encoderKey,
				labelToBeModified, label,
				externalToBeModified, external,
				enabledToBeModified, enabled,
				protocolToBeModified, protocol,
				publicServerNameToBeModified, publicServerName,
				internalServerNameToBeModified, internalServerName,
				portToBeModified, port
			);

            sResponse = (
                    string("{ ") 
                    + "\"encoderKey\": " + to_string(encoderKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 200, sResponse);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::removeEncoder(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeEncoder";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t encoderKey;
            auto encoderKeyIt = queryParameters.find("encoderKey");
            if (encoderKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'encoderKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            encoderKey = stoll(encoderKeyIt->second);
            
            _mmsEngineDBFacade->removeEncoder(
                encoderKey);

            sResponse = (
                    string("{ ") 
                    + "\"encoderKey\": " + to_string(encoderKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 200, sResponse);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::encoderList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace, bool admin,
		unordered_map<string, string> queryParameters)
{
    string api = "encoderList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
		int64_t encoderKey = -1;
		auto encoderKeyIt = queryParameters.find("encoderKey");
		if (encoderKeyIt != queryParameters.end() && encoderKeyIt->second != "")
		{
			encoderKey = stoll(encoderKeyIt->second);
			// 2020-01-31: it was sent 0, it should return no rows but, since we have the below check and
			//	it is changed to -1, the return is all the rows. Because of that it was commented
			// if (liveURLKey == 0)
			// 	liveURLKey = -1;
		}

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
				throw runtime_error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string label;
		auto labelIt = queryParameters.find("label");
		if (labelIt != queryParameters.end() && labelIt->second != "")
		{
			label = labelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(label, regex(plus), plusDecoded);

			label = curlpp::unescape(firstDecoding);
		}

		string serverName;
		auto serverNameIt = queryParameters.find("serverName");
		if (serverNameIt != queryParameters.end() && serverNameIt->second != "")
		{
			serverName = serverNameIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(serverName, regex(plus), plusDecoded);

			serverName = curlpp::unescape(firstDecoding);
		}

		int port = -1;
		auto portIt = queryParameters.find("port");
		if (portIt != queryParameters.end() && portIt->second != "")
		{
			port = stoi(portIt->second);
		}

		string labelOrder;
		auto labelOrderIt = queryParameters.find("labelOrder");
		if (labelOrderIt != queryParameters.end() && labelOrderIt->second != "")
		{
			if (labelOrderIt->second == "asc" || labelOrderIt->second == "desc")
				labelOrder = labelOrderIt->second;
			else
				_logger->warn(__FILEREF__ + "encoderList: 'labelOrder' parameter is unknown"
					+ ", labelOrder: " + labelOrderIt->second);
		}

		bool runningInfo = false;
		auto runningInfoIt = queryParameters.find("runningInfo");
		if (runningInfoIt != queryParameters.end())
			runningInfo = (runningInfoIt->second == "true" ? true : false);

        bool allEncoders = false;
		int64_t workspaceKey = workspace->_workspaceKey;
		if(admin)
		{
			// in case of admin, from the GUI, it is needed to:
			// - get the list of all encoders
			// - encoders for a specific workspace

			auto allEncodersIt = queryParameters.find("allEncoders");
			if (allEncodersIt != queryParameters.end())
				allEncoders = (allEncodersIt->second == "true" ? true : false);

			auto workspaceKeyIt = queryParameters.find("workspaceKey");
			if (workspaceKeyIt != queryParameters.end() && workspaceKeyIt->second != "")
				workspaceKey = stoll(workspaceKeyIt->second);
		}

        {
            Json::Value encoderListRoot = _mmsEngineDBFacade->getEncoderList(
				admin,
				start, rows,
				allEncoders, workspaceKey, runningInfo,
				encoderKey, label, serverName, port, labelOrder);

            string responseBody = JSONUtils::toString(encoderListRoot);
            
            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, "", api, 200, responseBody);
        }
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::encodersPoolList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace, bool admin,
		unordered_map<string, string> queryParameters)
{
    string api = "encoderList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
		int64_t encodersPoolKey = -1;
		auto encodersPoolKeyIt = queryParameters.find("encodersPoolKey");
		if (encodersPoolKeyIt != queryParameters.end() && encodersPoolKeyIt->second != "")
		{
			encodersPoolKey = stoll(encodersPoolKeyIt->second);
		}

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

		string label;
		auto labelIt = queryParameters.find("label");
		if (labelIt != queryParameters.end() && labelIt->second != "")
		{
			label = labelIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(label, regex(plus), plusDecoded);

			label = curlpp::unescape(firstDecoding);
		}

		string labelOrder;
		auto labelOrderIt = queryParameters.find("labelOrder");
		if (labelOrderIt != queryParameters.end() && labelOrderIt->second != "")
		{
			if (labelOrderIt->second == "asc" || labelOrderIt->second == "desc")
				labelOrder = labelOrderIt->second;
			else
				_logger->warn(__FILEREF__ + "encodersPoolList: 'labelOrder' parameter is unknown"
					+ ", labelOrder: " + labelOrderIt->second);
		}

        {
            Json::Value encodersPoolListRoot = _mmsEngineDBFacade->getEncodersPoolList(
                    start, rows,
					workspace->_workspaceKey,
					encodersPoolKey, label, labelOrder);

            string responseBody = JSONUtils::toString(encodersPoolListRoot);
            
            sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
				request, "", api, 200, responseBody);
        }
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::addEncodersPool(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request,
	shared_ptr<Workspace> workspace,
	string requestBody)
{
    string api = "addEncodersPool";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        vector<int64_t> encoderKeys;

        try
        {
            Json::Value requestBodyRoot = JSONUtils::toJson(-1, -1, requestBody);

            string field = "Label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = JSONUtils::asString(requestBodyRoot, field, "");            

            field = "encoderKeys";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				Json::Value encoderKeysRoot = requestBodyRoot[field];

				for (int encoderIndex = 0; encoderIndex < encoderKeysRoot.size();
					++encoderIndex)
				{
					encoderKeys.push_back((encoderKeysRoot[encoderIndex]).asInt64());
				}
            }
        }
        catch(runtime_error e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
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
			int64_t encodersPoolKey = _mmsEngineDBFacade->addEncodersPool(
				workspace->_workspaceKey, label, encoderKeys);

            sResponse = (
                    string("{ ") 
                    + "\"EncodersPoolKey\": " + to_string(encodersPoolKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEncodersPool failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEncodersPool failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 201, sResponse);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::modifyEncodersPool(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
	FCGX_Request& request,
	shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters,
	string requestBody)
{
    string api = "modifyEncodersPool";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        vector<int64_t> encoderKeys;

		int64_t encodersPoolKey = -1;
		auto encodersPoolKeyIt = queryParameters.find("encodersPoolKey");
		if (encodersPoolKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodersPoolKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		encodersPoolKey = stoll(encodersPoolKeyIt->second);

        try
        {
            Json::Value requestBodyRoot = JSONUtils::toJson(-1, -1, requestBody);

            string field = "Label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = JSONUtils::asString(requestBodyRoot, field, "");            

            field = "encoderKeys";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				Json::Value encoderKeysRoot = requestBodyRoot[field];

				for (int encoderIndex = 0; encoderIndex < encoderKeysRoot.size(); ++encoderIndex)
				{
					encoderKeys.push_back((encoderKeysRoot[encoderIndex]).asInt64());
				}
            }
        }
        catch(runtime_error e)
        {
            string errorMessage = string("requestBody json is not well format")
                    + ", requestBody: " + requestBody
                    + ", e.what(): " + e.what()
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
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
			_mmsEngineDBFacade->modifyEncodersPool(
				encodersPoolKey, workspace->_workspaceKey, label, encoderKeys);

            sResponse = (
                    string("{ ") 
                    + "\"EncodersPoolKey\": " + to_string(encodersPoolKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEncodersPool failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEncodersPool failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 201, sResponse);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::removeEncodersPool(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeEncodersPool";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t encodersPoolKey;
            auto encodersPoolKeyIt = queryParameters.find("encodersPoolKey");
            if (encodersPoolKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'encodersPoolKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            encodersPoolKey = stoll(encodersPoolKeyIt->second);
            
            _mmsEngineDBFacade->removeEncodersPool(
                encodersPoolKey);

            sResponse = (
                    string("{ ") 
                    + "\"encodersPoolKey\": " + to_string(encodersPoolKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodersPool failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEncodersPool failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 200, sResponse);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::addAssociationWorkspaceEncoder(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters)
{
    string api = "addAssociationWorkspaceEncoder";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
			int64_t workspaceKey;
			auto workspaceKeyIt = queryParameters.find("workspaceKey");
			if (workspaceKeyIt == queryParameters.end())
			{
				string errorMessage = string("The 'workspaceKey' parameter is not found");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				throw runtime_error(errorMessage);
			}
			workspaceKey = stoll(workspaceKeyIt->second);

			int64_t encoderKey;
			auto encoderKeyIt = queryParameters.find("encoderKey");
			if (encoderKeyIt == queryParameters.end())
			{
				string errorMessage = string("The 'encoderKey' parameter is not found");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				throw runtime_error(errorMessage);
			}
			encoderKey = stoll(encoderKeyIt->second);

			_mmsEngineDBFacade->addAssociationWorkspaceEncoder(workspaceKey, encoderKey);

			sResponse = (
				string("{ ") 
					+ "\"workspaceKey\": " + to_string(workspaceKey)
					+ ", \"encoderKey\": " + to_string(encoderKey)
					+ "}"
				);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addAssociationWorkspaceEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addAssociationWorkspaceEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 200, sResponse);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

void API::removeAssociationWorkspaceEncoder(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters)
{
    string api = "removeAssociationWorkspaceEncoder";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
			int64_t workspaceKey;
			auto workspaceKeyIt = queryParameters.find("workspaceKey");
			if (workspaceKeyIt == queryParameters.end())
			{
				string errorMessage = string("The 'workspaceKey' parameter is not found");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				throw runtime_error(errorMessage);
			}
			workspaceKey = stoll(workspaceKeyIt->second);

			int64_t encoderKey;
			auto encoderKeyIt = queryParameters.find("encoderKey");
			if (encoderKeyIt == queryParameters.end())
			{
				string errorMessage = string("The 'encoderKey' parameter is not found");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				throw runtime_error(errorMessage);
			}
			encoderKey = stoll(encoderKeyIt->second);

			_mmsEngineDBFacade->removeAssociationWorkspaceEncoder(workspaceKey, encoderKey);

			sResponse = (
				string("{ ") 
					+ "\"workspaceKey\": " + to_string(workspaceKey)
					+ ", \"encoderKey\": " + to_string(encoderKey)
					+ "}"
				);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeAssociationWorkspaceEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeAssociationWorkspaceEncoder failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 200, sResponse);
    }
    catch(runtime_error e)
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
    catch(exception e)
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

