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
#include "Validator.h"
#include <regex>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "API.h"
#include "catralibraries/StringUtils.h"


void API::addYouTubeConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addYouTubeConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string tokenType;
        string refreshToken;
        string accessToken;
        
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

            string field = "label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "").asString();            

            field = "tokenType";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            tokenType = requestBodyRoot.get(field, "").asString();            

			if (tokenType == "RefreshToken")
			{
				field = "refreshToken";
				if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				{
					string errorMessage = __FILEREF__
						+ "Field is not present or it is null"
                        + ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}    
				refreshToken = requestBodyRoot.get(field, "XXX").asString();            
			}
			else	// if (tokenType == "AccessToken")
			{
				field = "accessToken";
				if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}    
				accessToken = requestBodyRoot.get(field, "").asString();            
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
			Validator validator(_logger, _mmsEngineDBFacade, _configuration);
			if (!validator.isYouTubeTokenTypeValid(tokenType))
			{
				string errorMessage = string("The 'tokenType' is not valid")
					+ ", tokenType: " + tokenType
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
            }

			if (tokenType == "RefreshToken")
			{
				if (refreshToken == "")
				{
					string errorMessage = "The 'refreshToken' is not valid (empty)";
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else // if (tokenType == "AccessToken")
			{
				if (accessToken == "")
				{
					string errorMessage = "The 'accessToken' is not valid (empty)";
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			Json::Value youTubeRoot = _mmsEngineDBFacade->addYouTubeConf(
				workspace->_workspaceKey, label, tokenType, refreshToken, accessToken);

			Json::StreamWriterBuilder wbuilder;
			sResponse = Json::writeString(wbuilder, youTubeRoot);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addYouTubeConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addYouTubeConf failed"
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

void API::modifyYouTubeConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyYouTubeConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
		bool labelModified = false;
        string tokenType;
        bool tokenTypeModified = false;
        string refreshToken;
        bool refreshTokenModified = false;
        string accessToken;
        bool accessTokenModified = false;
        
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

            string field = "label";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				label = requestBodyRoot.get(field, "").asString();            
				labelModified = true;
            }

			field = "tokenType";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				tokenType = requestBodyRoot.get(field, "").asString();            
				tokenTypeModified = true;
			}

			field = "refreshToken";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				refreshToken = requestBodyRoot.get(field, "").asString();            
				refreshTokenModified = true;
			}

			field = "accessToken";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				accessToken = requestBodyRoot.get(field, "").asString();            
				accessTokenModified = true;
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
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

			if (tokenTypeModified)
			{
				Validator validator(_logger, _mmsEngineDBFacade, _configuration);
				if (!validator.isYouTubeTokenTypeValid(tokenType))
				{
					string errorMessage = string("The 'tokenType' is not valid");
						_logger->error(__FILEREF__ + errorMessage);
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}

				if (tokenType == "RefreshToken")
				{
					if (!refreshTokenModified || refreshToken == "")
					{
						string errorMessage = string("The 'refreshToken' is not valid");
							_logger->error(__FILEREF__ + errorMessage);
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				else // if (tokenType == "AccessToken")
				{
					if (!accessTokenModified || accessToken == "")
					{
						string errorMessage = string("The 'accessToken' is not valid");
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}

			Json::Value youTubeRoot = _mmsEngineDBFacade->modifyYouTubeConf(
                confKey, workspace->_workspaceKey,
				label, labelModified,
				tokenType, tokenTypeModified,
				refreshToken, refreshTokenModified,
				accessToken, accessTokenModified
			);

			Json::StreamWriterBuilder wbuilder;
			sResponse = Json::writeString(wbuilder, youTubeRoot);
		}
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyYouTubeConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyYouTubeConf failed"
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

void API::removeYouTubeConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeYouTubeConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeYouTubeConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeYouTubeConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeYouTubeConf failed"
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

void API::youTubeConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "youTubeConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
			Json::Value youTubeConfListRoot = _mmsEngineDBFacade->getYouTubeConfList(
				workspace->_workspaceKey);

			Json::StreamWriterBuilder wbuilder;
			string responseBody = Json::writeString(wbuilder, youTubeConfListRoot);

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

void API::addFacebookConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addFacebookConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string pageToken;
        
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

            field = "PageToken";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            pageToken = requestBodyRoot.get(field, "XXX").asString();            
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
            int64_t confKey = _mmsEngineDBFacade->addFacebookConf(
                workspace->_workspaceKey, label, pageToken);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFacebookConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFacebookConf failed"
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

void API::modifyFacebookConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyFacebookConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string pageToken;
        
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

            field = "PageToken";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            pageToken = requestBodyRoot.get(field, "XXX").asString();            
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
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifyFacebookConf(
                confKey, workspace->_workspaceKey, label, pageToken);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFacebookConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFacebookConf failed"
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

void API::removeFacebookConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeFacebookConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeFacebookConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFacebookConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFacebookConf failed"
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

void API::facebookConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "facebookConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
            
            Json::Value facebookConfListRoot = _mmsEngineDBFacade->getFacebookConfList(
                    workspace->_workspaceKey);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, facebookConfListRoot);
            
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

void API::addStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addStream";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
		string sourceType;

        string encodersPoolLabel;
        string url;
        string pushProtocol;
        int64_t pushEncoderKey;
        string pushServerName;
        int pushServerPort;
        string pushUri;
        int pushListenTimeout;
        int captureLiveVideoDeviceNumber;
        string captureLiveVideoInputFormat;
        int captureLiveFrameRate;
        int captureLiveWidth;
        int captureLiveHeight;
        int captureLiveAudioDeviceNumber;
        int captureLiveChannelsNumber;
		int64_t tvSourceTVConfKey;

        string type;
        string description;
        string name;
        string region;
        string country;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		int position = -1;
        Json::Value userData = Json::nullValue;

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

            string field = "label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "").asString();            

            field = "sourceType";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            sourceType = requestBodyRoot.get(field, "").asString();            

            field = "encodersPoolLabel";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				encodersPoolLabel = requestBodyRoot.get(field, "").asString();            

            field = "url";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				url = requestBodyRoot.get(field, "").asString();            

            field = "pushProtocol";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				pushProtocol = requestBodyRoot.get(field, "").asString();            

			field = "pushEncoderKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				pushEncoderKey = JSONUtils::asInt64(requestBodyRoot, field, -1);            

            field = "pushServerName";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				pushServerName = requestBodyRoot.get(field, "").asString();            

			field = "pushServerPort";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				pushServerPort = JSONUtils::asInt(requestBodyRoot, field, -1);            

            field = "pushURI";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				pushUri = requestBodyRoot.get(field, "").asString();            

			field = "pushListenTimeout";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				pushListenTimeout = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "captureLiveVideoDeviceNumber";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				captureLiveVideoDeviceNumber = JSONUtils::asInt(requestBodyRoot, field, -1);

            field = "captureLiveVideoInputFormat";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				captureLiveVideoInputFormat = requestBodyRoot.get(field, "").asString();

			field = "captureLiveFrameRate";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				captureLiveFrameRate = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveWidth";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				captureLiveWidth = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveHeight";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				captureLiveHeight = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveAudioDeviceNumber";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				captureLiveAudioDeviceNumber = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveChannelsNumber";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				captureLiveChannelsNumber = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "sourceTVConfKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				tvSourceTVConfKey = JSONUtils::asInt(requestBodyRoot, field, -1);

            field = "type";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				type = requestBodyRoot.get(field, "").asString();            

            field = "description";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				description = requestBodyRoot.get(field, "").asString();            

            field = "name";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				name = requestBodyRoot.get(field, "").asString();            

            field = "region";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				region = requestBodyRoot.get(field, "").asString();            

            field = "country";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				country = requestBodyRoot.get(field, "").asString();            

			field = "imageMediaItemKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				imageMediaItemKey = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "imageUniqueName";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				imageUniqueName = requestBodyRoot.get(field, "").asString();            

			field = "position";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				position = JSONUtils::asInt(requestBodyRoot, field, -1);            

            field = "userData";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				userData = requestBodyRoot[field];
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
			Json::Value streamRoot = _mmsEngineDBFacade->addStream(
                workspace->_workspaceKey, label,
				sourceType,
				encodersPoolLabel,
				url,
				pushProtocol,
				pushEncoderKey,
				pushServerName,
				pushServerPort,
				pushUri,
				pushListenTimeout,
				captureLiveVideoDeviceNumber,
				captureLiveVideoInputFormat,
				captureLiveFrameRate,
				captureLiveWidth,
				captureLiveHeight,
				captureLiveAudioDeviceNumber,
				captureLiveChannelsNumber,
				tvSourceTVConfKey,
				type, description,
				name, region, country, imageMediaItemKey, imageUniqueName, position,
				userData);

            Json::StreamWriterBuilder wbuilder;
            sResponse = Json::writeString(wbuilder, streamRoot);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addStream failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addStream failed"
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

void API::modifyStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyStream";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
		string sourceType;

        string encodersPoolLabel;
        string url;
        string pushProtocol;
        int64_t pushEncoderKey;
        string pushServerName;
        int pushServerPort;
        string pushUri;
        int pushListenTimeout;
        int captureLiveVideoDeviceNumber;
        string captureLiveVideoInputFormat;
        int captureLiveFrameRate;
        int captureLiveWidth;
        int captureLiveHeight;
        int captureLiveAudioDeviceNumber;
        int captureLiveChannelsNumber;
		int64_t tvSourceTVConfKey;

        string type;
        string description;
        string name;
        string region;
        string country;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		int position = -1;
        Json::Value userData;
        
		bool labelToBeModified;
		bool sourceTypeToBeModified;
		bool encodersPoolLabelToBeModified;
		bool urlToBeModified;
		bool pushProtocolToBeModified;
		bool pushEncoderKeyToBeModified;
		bool pushServerNameToBeModified;
		bool pushServerPortToBeModified;
		bool pushUriToBeModified;
		bool pushListenTimeoutToBeModified;
		bool captureLiveVideoDeviceNumberToBeModified;
		bool captureLiveVideoInputFormatToBeModified;
		bool captureLiveFrameRateToBeModified;
		bool captureLiveWidthToBeModified;
		bool captureLiveHeightToBeModified;
		bool captureLiveAudioDeviceNumberToBeModified;
		bool captureLiveChannelsNumberToBeModified;
		bool tvSourceTVConfKeyToBeModified;
		bool typeToBeModified;
		bool descriptionToBeModified;
		bool nameToBeModified;
		bool regionToBeModified;
		bool countryToBeModified;
		bool imageToBeModified;
		bool positionToBeModified;
		bool userDataToBeModified;

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

			labelToBeModified = false;
            string field = "label";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				label = requestBodyRoot.get(field, "").asString();            
				labelToBeModified = true;
            }    

			sourceTypeToBeModified = false;
            field = "sourceType";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				sourceType = requestBodyRoot.get(field, "").asString();
				sourceTypeToBeModified = true;
            }

			encodersPoolLabelToBeModified = false;
            field = "encodersPoolLabel";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				encodersPoolLabel = requestBodyRoot.get(field, "").asString();            
				encodersPoolLabelToBeModified = true;
			}

			urlToBeModified = false;
            field = "url";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				url = requestBodyRoot.get(field, "").asString();            
				urlToBeModified = true;
			}

			pushProtocolToBeModified = false;
            field = "pushProtocol";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				pushProtocol = requestBodyRoot.get(field, "").asString();            
				pushProtocolToBeModified = true;
			}

			pushEncoderKeyToBeModified = false;
			field = "pushEncoderKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				pushEncoderKey = JSONUtils::asInt64(requestBodyRoot, field, -1);            
				pushEncoderKeyToBeModified = true;
			}

			pushServerNameToBeModified = false;
            field = "pushServerName";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				pushServerName = requestBodyRoot.get(field, "").asString();            
				pushServerNameToBeModified = true;
			}

			pushServerPortToBeModified = false;
			field = "pushServerPort";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				pushServerPort = JSONUtils::asInt(requestBodyRoot, field, -1);            
				pushServerPortToBeModified = true;
			}

			pushUriToBeModified = false;
            field = "pushURI";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				pushUri = requestBodyRoot.get(field, "").asString();            
				pushUriToBeModified = true;
			}

			pushListenTimeoutToBeModified = false;
			field = "pushListenTimeout";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				pushListenTimeout = JSONUtils::asInt(requestBodyRoot, field, -1);            
				pushListenTimeoutToBeModified = true;
			}

			captureLiveVideoDeviceNumberToBeModified = false;
			field = "captureLiveVideoDeviceNumber";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				captureLiveVideoDeviceNumber = JSONUtils::asInt(requestBodyRoot, field, -1);
				captureLiveVideoDeviceNumberToBeModified = true;
			}

			captureLiveVideoInputFormatToBeModified = false;
            field = "captureLiveVideoInputFormat";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				captureLiveVideoInputFormat = requestBodyRoot.get(field, "").asString();
				captureLiveVideoInputFormatToBeModified = true;
			}

			captureLiveFrameRateToBeModified = false;
			field = "captureLiveFrameRate";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				captureLiveFrameRate = JSONUtils::asInt(requestBodyRoot, field, -1);
				captureLiveFrameRateToBeModified = true;
			}

			captureLiveWidthToBeModified = false;
			field = "captureLiveWidth";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				captureLiveWidth = JSONUtils::asInt(requestBodyRoot, field, -1);
				captureLiveWidthToBeModified = true;
			}

			captureLiveHeightToBeModified = false;
			field = "captureLiveHeight";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				captureLiveHeight = JSONUtils::asInt(requestBodyRoot, field, -1);
				captureLiveHeightToBeModified = true;
			}

			captureLiveAudioDeviceNumberToBeModified = false;
			field = "captureLiveAudioDeviceNumber";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				captureLiveAudioDeviceNumber = JSONUtils::asInt(requestBodyRoot, field, -1);
				captureLiveAudioDeviceNumberToBeModified = true;
			}

			captureLiveChannelsNumberToBeModified = false;
			field = "captureLiveChannelsNumber";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				captureLiveChannelsNumber = JSONUtils::asInt(requestBodyRoot, field, -1);
				captureLiveChannelsNumberToBeModified = true;
			}

			tvSourceTVConfKeyToBeModified = false;
			field = "sourceTVConfKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				tvSourceTVConfKey = JSONUtils::asInt64(requestBodyRoot, field, -1);
				tvSourceTVConfKeyToBeModified = true;
			}

			typeToBeModified = false;
            field = "type";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				type = requestBodyRoot.get(field, "XXX").asString();            
				typeToBeModified = true;
			}

			descriptionToBeModified = false;
            field = "description";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				description = requestBodyRoot.get(field, "XXX").asString();            
				descriptionToBeModified = true;
			}

			nameToBeModified = false;
            field = "name";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				name = requestBodyRoot.get(field, "XXX").asString();            
				nameToBeModified = true;
			}

			regionToBeModified = false;
            field = "region";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				region = requestBodyRoot.get(field, "XXX").asString();            
				regionToBeModified = true;
			}

			countryToBeModified = false;
            field = "country";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				country = requestBodyRoot.get(field, "XXX").asString();            
				countryToBeModified = true;
			}

			imageToBeModified = false;
			field = "imageMediaItemKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				imageMediaItemKey = JSONUtils::asInt64(requestBodyRoot, field, -1);            
				imageToBeModified = true;
			}

			imageToBeModified = false;
			field = "imageUniqueName";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				imageUniqueName = requestBodyRoot.get(field, "").asString();            
				imageToBeModified = true;
			}

			positionToBeModified = false;
			field = "position";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				position = JSONUtils::asInt(requestBodyRoot, field, -1);            
				positionToBeModified = true;
			}
			else if (JSONUtils::isNull(requestBodyRoot, field))
			{
				// in order to set the field as null into the DB
				positionToBeModified = true;
			}

			userDataToBeModified = false;
            field = "userData";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				userData = requestBodyRoot[field];
				userDataToBeModified = true;
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
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

			Json::Value streamRoot = _mmsEngineDBFacade->modifyStream(
                confKey, workspace->_workspaceKey,
				labelToBeModified, label,
				sourceTypeToBeModified, sourceType,
				encodersPoolLabelToBeModified, encodersPoolLabel,
				urlToBeModified, url,
				pushProtocolToBeModified, pushProtocol,
				pushEncoderKeyToBeModified, pushEncoderKey,
				pushServerNameToBeModified, pushServerName,
				pushServerPortToBeModified, pushServerPort,
				pushUriToBeModified, pushUri,
				pushListenTimeoutToBeModified, pushListenTimeout,
				captureLiveVideoDeviceNumberToBeModified, captureLiveVideoDeviceNumber,
				captureLiveVideoInputFormatToBeModified, captureLiveVideoInputFormat,
				captureLiveFrameRateToBeModified, captureLiveFrameRate,
				captureLiveWidthToBeModified, captureLiveWidth,
				captureLiveHeightToBeModified, captureLiveHeight,
				captureLiveAudioDeviceNumberToBeModified, captureLiveAudioDeviceNumber,
				captureLiveChannelsNumberToBeModified, captureLiveChannelsNumber,
				tvSourceTVConfKeyToBeModified, tvSourceTVConfKey,
				typeToBeModified, type,
				descriptionToBeModified, description,
				nameToBeModified, name,
				regionToBeModified, region,
				countryToBeModified, country,
				imageToBeModified, imageMediaItemKey, imageUniqueName,
				positionToBeModified, position,
				userDataToBeModified, userData);

            Json::StreamWriterBuilder wbuilder;
            sResponse = Json::writeString(wbuilder, streamRoot);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyStream failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyStream failed"
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

void API::removeStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeStream";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeStream(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeStream failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeStream failed"
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

void API::streamList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters)
{
    string api = "streamList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
		int64_t liveURLKey = -1;
		auto liveURLKeyIt = queryParameters.find("liveURLKey");
		if (liveURLKeyIt != queryParameters.end() && liveURLKeyIt->second != "")
		{
			liveURLKey = stoll(liveURLKeyIt->second);
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

		string url;
		auto urlIt = queryParameters.find("url");
		if (urlIt != queryParameters.end() && urlIt->second != "")
		{
			url = urlIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(url, regex(plus), plusDecoded);

			url = curlpp::unescape(firstDecoding);
		}

		string sourceType;
		auto sourceTypeIt = queryParameters.find("sourceType");
		if (sourceTypeIt != queryParameters.end() && sourceTypeIt->second != "")
		{
			if (sourceTypeIt->second == "IP_PULL"
				|| sourceTypeIt->second == "IP_PUSH"
				|| sourceTypeIt->second == "CaptureLive"
				|| sourceTypeIt->second == "TV"
			)
				sourceType = sourceTypeIt->second;
			else
				_logger->warn(__FILEREF__ + "streamList: 'sourceType' parameter is unknown"
					+ ", sourceType: " + sourceTypeIt->second);
		}

		string type;
		auto typeIt = queryParameters.find("type");
		if (typeIt != queryParameters.end() && typeIt->second != "")
		{
			type = typeIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(type, regex(plus), plusDecoded);

			type = curlpp::unescape(firstDecoding);
		}

		string name;
		auto nameIt = queryParameters.find("name");
		if (nameIt != queryParameters.end() && nameIt->second != "")
		{
			name = nameIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(name, regex(plus), plusDecoded);

			name = curlpp::unescape(firstDecoding);
		}

		string region;
		auto regionIt = queryParameters.find("region");
		if (regionIt != queryParameters.end() && regionIt->second != "")
		{
			region = regionIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(region, regex(plus), plusDecoded);

			region = curlpp::unescape(firstDecoding);
		}

		string country;
		auto countryIt = queryParameters.find("country");
		if (countryIt != queryParameters.end() && countryIt->second != "")
		{
			country = countryIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(country, regex(plus), plusDecoded);

			country = curlpp::unescape(firstDecoding);
		}

		string labelOrder;
		auto labelOrderIt = queryParameters.find("labelOrder");
		if (labelOrderIt != queryParameters.end() && labelOrderIt->second != "")
		{
			if (labelOrderIt->second == "asc" || labelOrderIt->second == "desc")
				labelOrder = labelOrderIt->second;
			else
				_logger->warn(__FILEREF__ + "liveURLList: 'labelOrder' parameter is unknown"
					+ ", labelOrder: " + labelOrderIt->second);
		}

        {
            
            Json::Value streamListRoot = _mmsEngineDBFacade->getStreamList(
                    workspace->_workspaceKey, liveURLKey, start, rows, label, 
					url, sourceType, type, name, region, country, labelOrder);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, streamListRoot);
            
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


void API::addSourceTVStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addSourceTVStream";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		int64_t serviceId;
		int64_t networkId;
		int64_t transportStreamId;
		string name;
		string satellite;
		int64_t frequency;
		string lnb;
		int videoPid;
		string audioPids;
		int audioItalianPid;
		int audioEnglishPid;
		int teletextPid;
		string modulation;
		string polarization;
		int64_t symbolRate;
		string country;
		string deliverySystem;

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

			string field = "serviceId";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				serviceId = -1;
			else
				serviceId = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "networkId";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				networkId = -1;
			else
				networkId = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "transportStreamId";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				transportStreamId = -1;
			else
				transportStreamId = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "name";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			name = requestBodyRoot.get(field, "").asString();            

			field = "satellite";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				satellite = requestBodyRoot.get(field, "").asString();            

			field = "frequency";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			frequency = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "lnb";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				lnb = requestBodyRoot.get(field, "").asString();            

			field = "videoPid";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				videoPid = -1;
			else
				videoPid = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "audioPids";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				audioPids = requestBodyRoot.get(field, "").asString();            

			field = "audioItalianPid";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				audioItalianPid = -1;
			else
				audioItalianPid = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "audioEnglishPid";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				audioEnglishPid = -1;
			else
				audioEnglishPid = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "teletextPid";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				teletextPid = -1;
			else
				teletextPid = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "modulation";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				modulation = requestBodyRoot.get(field, "").asString();            

			field = "polarization";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				polarization = requestBodyRoot.get(field, "").asString();            

			field = "symbolRate";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				symbolRate = -1;
			else
				symbolRate = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "country";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				country = requestBodyRoot.get(field, "").asString();            

			field = "deliverySystem";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				deliverySystem = requestBodyRoot.get(field, "").asString();            
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
			Json::Value sourceTVStreamRoot =
				_mmsEngineDBFacade->addSourceTVStream(
				serviceId, networkId, transportStreamId,
				name, satellite, frequency, lnb,
				videoPid, audioPids, audioItalianPid, audioEnglishPid, teletextPid,
				modulation, polarization, symbolRate, country, deliverySystem
			);

            Json::StreamWriterBuilder wbuilder;
            sResponse = Json::writeString(wbuilder, sourceTVStreamRoot);
		}
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addSourceTVStream failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addSourceTVStream failed"
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

void API::modifySourceTVStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifySourceTVStream";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		bool serviceIdToBeModified;
		int64_t serviceId = -1;
		bool networkIdToBeModified;
		int64_t networkId = -1;
		bool transportStreamIdToBeModified;
		int64_t transportStreamId = -1;
		bool nameToBeModified;
		string name;
		bool satelliteToBeModified;
		string satellite;
		bool frequencyToBeModified;
		int64_t frequency = -1;
		bool lnbToBeModified;
		string lnb;
		bool videoPidToBeModified;
		int videoPid = -1;
		bool audioPidsToBeModified;
		string audioPids;
		bool audioItalianPidToBeModified;
		int audioItalianPid = -1;
		bool audioEnglishPidToBeModified;
		int audioEnglishPid = -1;
		bool teletextPidToBeModified;
		int teletextPid = -1;
		bool modulationToBeModified;
		string modulation;
		bool polarizationToBeModified;
		string polarization;
		bool symbolRateToBeModified;
		int64_t symbolRate = -1;
		bool countryToBeModified;
		string country;
		bool deliverySystemToBeModified;
		string deliverySystem;


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

            string field = "serviceId";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				serviceId = JSONUtils::asInt64(requestBodyRoot, field, -1);
				serviceIdToBeModified = true;
            }

            field = "networkId";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				networkId = JSONUtils::asInt64(requestBodyRoot, field, -1);
				networkIdToBeModified = true;
            }

            field = "transportStreamId";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				transportStreamId = JSONUtils::asInt64(requestBodyRoot, field, -1);
				transportStreamIdToBeModified = true;
            }

            field = "name";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				name = requestBodyRoot.get(field, "").asString();            
				nameToBeModified = true;
            }

            field = "satellite";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				satellite = requestBodyRoot.get(field, "").asString();            
				satelliteToBeModified = true;
            }

            field = "frequency";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				frequency = JSONUtils::asInt64(requestBodyRoot, field, -1);
				frequencyToBeModified = true;
            }

            field = "lnb";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				lnb = requestBodyRoot.get(field, "").asString();            
				lnbToBeModified = true;
            }

            field = "videoPid";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				videoPid = JSONUtils::asInt(requestBodyRoot, field, -1);
				videoPidToBeModified = true;
            }

            field = "audioPids";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				audioPids = requestBodyRoot.get(field, "").asString();            
				audioPidsToBeModified = true;
            }

            field = "audioItalianPid";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				audioItalianPid = JSONUtils::asInt(requestBodyRoot, field, -1);
				audioItalianPidToBeModified = true;
            }

            field = "audioEnglishPid";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				audioEnglishPid = JSONUtils::asInt(requestBodyRoot, field, -1);
				audioEnglishPidToBeModified = true;
            }

            field = "teletextPid";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				teletextPid = JSONUtils::asInt(requestBodyRoot, field, -1);
				teletextPidToBeModified = true;
            }

            field = "modulation";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				modulation = requestBodyRoot.get(field, "").asString();            
				modulationToBeModified = true;
            }

            field = "polarization";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				polarization = requestBodyRoot.get(field, "").asString();            
				polarizationToBeModified = true;
            }

            field = "symbolRate";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				symbolRate = JSONUtils::asInt64(requestBodyRoot, field, -1);
				symbolRateToBeModified = true;
            }

            field = "country";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				country = requestBodyRoot.get(field, "").asString();            
				countryToBeModified = true;
            }

            field = "deliverySystem";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				deliverySystem = requestBodyRoot.get(field, "").asString();            
				deliverySystemToBeModified = true;
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
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

			Json::Value sourceTVStreamRoot =
				_mmsEngineDBFacade->modifySourceTVStream(
				confKey,
                serviceIdToBeModified, serviceId,
				networkIdToBeModified, networkId,
				transportStreamIdToBeModified, transportStreamId,
				nameToBeModified, name,
				satelliteToBeModified, satellite,
				frequencyToBeModified, frequency,
				lnbToBeModified, lnb,
				videoPidToBeModified, videoPid,
				audioPidsToBeModified, audioPids,
				audioItalianPidToBeModified, audioItalianPid,
				audioEnglishPidToBeModified, audioEnglishPid,
				teletextPidToBeModified, teletextPid,
				modulationToBeModified, modulation,
				polarizationToBeModified, polarization,
				symbolRateToBeModified, symbolRate,
				countryToBeModified, country,
				deliverySystemToBeModified, deliverySystem
			);

            Json::StreamWriterBuilder wbuilder;
            sResponse = Json::writeString(wbuilder, sourceTVStreamRoot);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifySourceTVStream failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifySourceTVStream failed"
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

void API::removeSourceTVStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeSourceTVStream";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeSourceTVStream(confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeSourceTVStream failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeSourceTVStream failed"
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

void API::sourceTVStreamList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters)
{
    string api = "sourceTVStreamList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
		int64_t confKey = -1;
		auto confKeyIt = queryParameters.find("confKey");
		if (confKeyIt != queryParameters.end() && confKeyIt->second != "")
		{
			confKey = stoll(confKeyIt->second);
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
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		int64_t serviceId = -1;
		auto serviceIdIt = queryParameters.find("serviceId");
		if (serviceIdIt != queryParameters.end() && serviceIdIt->second != "")
			serviceId = stoll(serviceIdIt->second);

		string name;
		auto nameIt = queryParameters.find("name");
		if (nameIt != queryParameters.end() && nameIt->second != "")
		{
			name = nameIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(name, regex(plus), plusDecoded);

			name = curlpp::unescape(firstDecoding);
		}

		string lnb;
		auto lnbIt = queryParameters.find("lnb");
		if (lnbIt != queryParameters.end() && lnbIt->second != "")
		{
			lnb = lnbIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(lnb, regex(plus), plusDecoded);

			lnb = curlpp::unescape(firstDecoding);
		}

		int64_t frequency = -1;
		auto frequencyIt = queryParameters.find("frequency");
		if (frequencyIt != queryParameters.end() && frequencyIt->second != "")
			frequency = stoll(frequencyIt->second);

		int videoPid = -1;
		auto videoPidIt = queryParameters.find("videoPid");
		if (videoPidIt != queryParameters.end() && videoPidIt->second != "")
			videoPid = stoi(videoPidIt->second);

		string audioPids;
		auto audioPidsIt = queryParameters.find("audioPids");
		if (audioPidsIt != queryParameters.end() && audioPidsIt->second != "")
		{
			audioPids = audioPidsIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(audioPids, regex(plus), plusDecoded);

			audioPids = curlpp::unescape(firstDecoding);
		}

		string nameOrder;
		auto nameOrderIt = queryParameters.find("nameOrder");
		if (nameOrderIt != queryParameters.end() && nameOrderIt->second != "")
		{
			if (nameOrderIt->second == "asc" || nameOrderIt->second == "desc")
				nameOrder = nameOrderIt->second;
			else
				_logger->warn(__FILEREF__ + "tvChannelList: 'nameOrder' parameter is unknown"
					+ ", nameOrder: " + nameOrderIt->second);
		}

        {
            Json::Value sourceTVStreamRoot =
				_mmsEngineDBFacade->getSourceTVStreamList(
				confKey, start, rows,
				serviceId, name, frequency, lnb, videoPid, audioPids, nameOrder);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, sourceTVStreamRoot);

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

void API::addAWSChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addAWSChannelConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string channelId;
        string rtmpURL;
        string playURL;
        string type;

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

            string field = "label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "").asString();            

            field = "channelId";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            channelId = requestBodyRoot.get(field, "").asString();            

            field = "rtmpURL";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            rtmpURL = requestBodyRoot.get(field, "").asString();            

            field = "playURL";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            playURL = requestBodyRoot.get(field, "").asString();            

            field = "type";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            type = requestBodyRoot.get(field, "").asString();            
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
            int64_t confKey = _mmsEngineDBFacade->addAWSChannelConf(
                workspace->_workspaceKey, label, channelId, rtmpURL,
				playURL, type);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addAWSChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addAWSChannelConf failed"
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

void API::modifyAWSChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyAWSChannelConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string channelId;
        string rtmpURL;
        string playURL;
        string type;

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

            string field = "label";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            label = requestBodyRoot.get(field, "").asString();            

            field = "channelId";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            channelId = requestBodyRoot.get(field, "").asString();            

            field = "rtmpURL";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            rtmpURL = requestBodyRoot.get(field, "").asString();            

            field = "playURL";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            playURL = requestBodyRoot.get(field, "").asString();            

            field = "type";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            type = requestBodyRoot.get(field, "").asString();            
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
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifyAWSChannelConf(
                confKey, workspace->_workspaceKey, label, 
				channelId, rtmpURL, playURL, type);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyAWSChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyAWSChannelConf failed"
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

void API::removeAWSChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeAWSChannelConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeAWSChannelConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeAWSChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeAWSChannelConf failed"
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

void API::awsChannelConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "awsChannelConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
            
            Json::Value awsChannelConfListRoot = _mmsEngineDBFacade->getAWSChannelConfList(
                    workspace->_workspaceKey);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, awsChannelConfListRoot);
            
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


void API::addFTPConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addFTPConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string server;
        int port;
        string userName;
        string password;
        string remoteDirectory;
        
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

            field = "Server";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            server = requestBodyRoot.get(field, "XXX").asString();            

            field = "Port";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            port = JSONUtils::asInt(requestBodyRoot, field, 0);            

            field = "UserName";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            userName = requestBodyRoot.get(field, "XXX").asString();            

            field = "Password";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            password = requestBodyRoot.get(field, "XXX").asString();            

            field = "RemoteDirectory";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            remoteDirectory = requestBodyRoot.get(field, "XXX").asString();            
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
            int64_t confKey = _mmsEngineDBFacade->addFTPConf(
                workspace->_workspaceKey, label, server, port,
				userName, password, remoteDirectory);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFTPConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFTPConf failed"
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

void API::modifyFTPConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyFTPConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string server;
        int port;
        string userName;
        string password;
        string remoteDirectory;
        
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

            field = "Server";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            server = requestBodyRoot.get(field, "XXX").asString();            

            field = "Port";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            port = JSONUtils::asInt(requestBodyRoot, field, 0);            

            field = "UserName";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            userName = requestBodyRoot.get(field, "XXX").asString();            

            field = "Password";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            password = requestBodyRoot.get(field, "XXX").asString();            

            field = "RemoteDirectory";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            remoteDirectory = requestBodyRoot.get(field, "XXX").asString();            
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
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifyFTPConf(
                confKey, workspace->_workspaceKey, label, 
				server, port, userName, password, remoteDirectory);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFTPConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFTPConf failed"
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

void API::removeFTPConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeFTPConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeFTPConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFTPConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFTPConf failed"
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

void API::ftpConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "ftpConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
            
            Json::Value ftpConfListRoot = _mmsEngineDBFacade->getFTPConfList(
                    workspace->_workspaceKey);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, ftpConfListRoot);
            
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

void API::addEMailConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addEMailConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string addresses;
        string subject;
        string message;
        
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

            field = "Addresses";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            addresses = requestBodyRoot.get(field, "XXX").asString();            

            field = "Subject";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            subject = requestBodyRoot.get(field, "XXX").asString();            

            field = "Message";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            message = requestBodyRoot.get(field, "XXX").asString();            
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
            int64_t confKey = _mmsEngineDBFacade->addEMailConf(
                workspace->_workspaceKey, label, addresses, subject, message);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEMailConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEMailConf failed"
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

void API::modifyEMailConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyEMailConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string addresses;
        string subject;
        string message;
        
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

            field = "Addresses";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            addresses = requestBodyRoot.get(field, "XXX").asString();            

            field = "Subject";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            subject = requestBodyRoot.get(field, "XXX").asString();            

            field = "Message";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            message = requestBodyRoot.get(field, "XXX").asString();            
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
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifyEMailConf(
                confKey, workspace->_workspaceKey, label, 
				addresses, subject, message);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEMailConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEMailConf failed"
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

void API::removeEMailConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeEMailConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
    );

    try
    {
        string sResponse;
        try
        {
            int64_t confKey;
            auto confKeyIt = queryParameters.find("confKey");
            if (confKeyIt == queryParameters.end())
            {
                string errorMessage = string("The 'confKey' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);
            
            _mmsEngineDBFacade->removeEMailConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEMailConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEMailConf failed"
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

void API::emailConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace)
{
    string api = "emailConfList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        {
            
            Json::Value emailConfListRoot = _mmsEngineDBFacade->getEMailConfList(
                    workspace->_workspaceKey);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, emailConfListRoot);
            
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

