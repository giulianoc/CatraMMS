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
        string serverName;
        int port;
        // int maxTranscodingCapability;
        // int maxLiveProxiesCapabilities;
        // int maxLiveRecordingCapabilities;

        try
        {
            Json::Value requestBodyRoot;

            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &requestBodyRoot, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                            + ", errors: " + errors
                            + ", requestBody: " + requestBody
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

            field = "External";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				external = requestBodyRoot.get(field, false).asBool();            
			else
				external = false;

            field = "true";
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
            protocol = requestBodyRoot.get(field, "").asString();            

            field = "ServerName";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            serverName = requestBodyRoot.get(field, "").asString();            

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

			/*
            field = "MaxTranscodingCapability";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				maxTranscodingCapability = requestBodyRoot.get(field, 5).asInt();            
			else
				maxTranscodingCapability = 5;            

            field = "MaxLiveProxiesCapabilities";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				maxLiveProxiesCapabilities = requestBodyRoot.get(field, 5).asInt();            
			else
				maxLiveProxiesCapabilities = 5;

            field = "MaxLiveRecordingCapabilities";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				maxLiveRecordingCapabilities = requestBodyRoot.get(field, 5).asInt();            
			else
				maxLiveRecordingCapabilities = 5;            
			*/
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
                label, external, enabled, protocol, serverName, port);
				// maxTranscodingCapability, maxLiveProxiesCapabilities, maxLiveRecordingCapabilities);

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

        sendSuccess(request, 201, sResponse);
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

        string serverName;
		bool serverNameToBeModified;

        int port;
		bool portToBeModified;

		/*
        int maxTranscodingCapability;
		bool maxTranscodingCapabilityToBeModified;

        int maxLiveProxiesCapabilities;
		bool maxLiveProxiesCapabilitiesToBeModified;

        int maxLiveRecordingCapabilities;
		bool maxLiveRecordingCapabilitiesToBeModified;
		*/

        try
        {
            Json::Value requestBodyRoot;
            
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &requestBodyRoot, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                            + ", errors: " + errors
                            + ", requestBody: " + requestBody
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				label = requestBodyRoot.get(field, "").asString();            
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
				protocol = requestBodyRoot.get(field, "").asString();            
				protocolToBeModified = true;
            }
			else
				protocolToBeModified = false;

            field = "ServerName";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				serverName = requestBodyRoot.get(field, "").asString();            
				serverNameToBeModified = true;
            }
			else
				serverNameToBeModified = false;

            field = "Port";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				port = requestBodyRoot.get(field, 80).asInt();            
				portToBeModified = true;
            }
			else
				portToBeModified = false;

			/*
            field = "MaxTranscodingCapability";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				maxTranscodingCapability = requestBodyRoot.get(field, 5).asInt();            
				maxTranscodingCapabilityToBeModified = true;
            }
			else
				maxTranscodingCapabilityToBeModified = false;

            field = "MaxLiveProxiesCapabilities";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				maxLiveProxiesCapabilities = requestBodyRoot.get(field, 5).asInt();            
				maxLiveProxiesCapabilitiesToBeModified = true;
            }
			else
				maxLiveProxiesCapabilitiesToBeModified = false;

            field = "MaxLiveRecordingCapabilities";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				maxLiveRecordingCapabilities = requestBodyRoot.get(field, 5).asInt();            
				maxLiveRecordingCapabilitiesToBeModified = true;
            }
			else
				maxLiveRecordingCapabilitiesToBeModified = false;
			*/
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
				serverNameToBeModified, serverName,
				portToBeModified, port
				// maxTranscodingCapabilityToBeModified, maxTranscodingCapability,
				// maxLiveProxiesCapabilitiesToBeModified, maxLiveProxiesCapabilities,
				// maxLiveRecordingCapabilitiesToBeModified, maxLiveRecordingCapabilities
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

        sendSuccess(request, 200, sResponse);
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

        sendSuccess(request, 200, sResponse);
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
				rows = _maxPageSize;
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
                    start, rows,
					allEncoders, workspaceKey,
					encoderKey, label, serverName, port, labelOrder);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, encoderListRoot);
            
            sendSuccess(request, 200, responseBody);
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
				rows = _maxPageSize;
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

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, encodersPoolListRoot);
            
            sendSuccess(request, 200, responseBody);
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
            Json::Value requestBodyRoot;
            
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &requestBodyRoot, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                            + ", errors: " + errors
                            + ", requestBody: " + requestBody
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

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

        sendSuccess(request, 201, sResponse);
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
            Json::Value requestBodyRoot;
            
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(requestBody.c_str(),
                        requestBody.c_str() + requestBody.size(), 
                        &requestBodyRoot, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the requestBody"
                            + ", errors: " + errors
                            + ", requestBody: " + requestBody
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errors);
                }
            }

            string field = "Label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "XXX").asString();            

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

        sendSuccess(request, 201, sResponse);
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

        sendSuccess(request, 200, sResponse);
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

        sendSuccess(request, 200, sResponse);
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

        sendSuccess(request, 200, sResponse);
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

