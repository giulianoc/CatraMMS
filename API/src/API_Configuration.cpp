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
#include "catralibraries/StringUtils.h"


void API::addYouTubeConf(
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
        string refreshToken;
        
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

            field = "RefreshToken";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            refreshToken = requestBodyRoot.get(field, "XXX").asString();            
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
            int64_t confKey = _mmsEngineDBFacade->addYouTubeConf(
                workspace->_workspaceKey, label, refreshToken);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
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

void API::modifyYouTubeConf(
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
        string refreshToken;
        
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

            field = "RefreshToken";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            refreshToken = requestBodyRoot.get(field, "XXX").asString();            
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

            _mmsEngineDBFacade->modifyYouTubeConf(
                confKey, workspace->_workspaceKey, label, refreshToken);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
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

void API::removeYouTubeConf(
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

void API::youTubeConfList(
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

void API::addFacebookConf(
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

void API::modifyFacebookConf(
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

void API::removeFacebookConf(
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

void API::facebookConfList(
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

void API::addIPChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addIPChannelConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string url;
        string type;
        string description;
        string name;
        string region;
        string country;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		int position = -1;
        Json::Value channelData = Json::nullValue;

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

            field = "Url";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            url = requestBodyRoot.get(field, "").asString();            

            field = "Type";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				type = requestBodyRoot.get(field, "").asString();            

            field = "Description";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				description = requestBodyRoot.get(field, "").asString();            

            field = "Name";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				name = requestBodyRoot.get(field, "").asString();            

            field = "Region";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				region = requestBodyRoot.get(field, "").asString();            

            field = "Country";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				country = requestBodyRoot.get(field, "").asString();            

			field = "ImageMediaItemKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				imageMediaItemKey = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "ImageUniqueName";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				imageUniqueName = requestBodyRoot.get(field, "").asString();            

			field = "Position";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				position = JSONUtils::asInt(requestBodyRoot, field, -1);            

            field = "ChannelData";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				channelData = requestBodyRoot[field];
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
            int64_t confKey = _mmsEngineDBFacade->addIPChannelConf(
                workspace->_workspaceKey, label, url, type, description,
				name, region, country, imageMediaItemKey, imageUniqueName, position,
				channelData);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addLiveURLConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addLiveURLConf failed"
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

void API::modifyIPChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyIPChannelConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
        string label;
        string url;
        string type;
        string description;
        string name;
        string region;
        string country;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		int position = -1;
        Json::Value channelData;
        
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

            field = "Url";
            if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }    
            url = requestBodyRoot.get(field, "").asString();            

            field = "Type";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				type = requestBodyRoot.get(field, "XXX").asString();            

            field = "Description";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				description = requestBodyRoot.get(field, "XXX").asString();            

            field = "Name";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				name = requestBodyRoot.get(field, "XXX").asString();            

            field = "Region";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				region = requestBodyRoot.get(field, "XXX").asString();            

            field = "Country";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				country = requestBodyRoot.get(field, "XXX").asString();            

			field = "ImageMediaItemKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				imageMediaItemKey = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "ImageUniqueName";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				imageUniqueName = requestBodyRoot.get(field, "").asString();            

			field = "Position";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				position = JSONUtils::asInt(requestBodyRoot, field, -1);            

            field = "ChannelData";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				channelData = requestBodyRoot[field];
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

			bool labelToBeModified = true;
			bool urlToBeModified = true;
			bool typeToBeModified = true;
			bool descriptionToBeModified = true;
			bool nameToBeModified = true;
			bool regionToBeModified = true;
			bool countryToBeModified = true;
			bool imageToBeModified = true;
			bool positionToBeModified = true;
			bool channelDataToBeModified = true;

            _mmsEngineDBFacade->modifyIPChannelConf(
                confKey, workspace->_workspaceKey,
				labelToBeModified, label,
				urlToBeModified, url,
				typeToBeModified, type,
				descriptionToBeModified, description,
				nameToBeModified, name,
				regionToBeModified, region,
				countryToBeModified, country,
				imageToBeModified, imageMediaItemKey, imageUniqueName,
				positionToBeModified, position,
				channelDataToBeModified, channelData);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyLiveURLConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyLiveURLConf failed"
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

void API::removeIPChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeIPChannelConf";

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
            
            _mmsEngineDBFacade->removeIPChannelConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeIPChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeIPChannelConf failed"
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

void API::ipChannelConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters)
{
    string api = "ipChannelConfList";

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
            
            Json::Value channelConfListRoot = _mmsEngineDBFacade->getIPChannelConfList(
                    workspace->_workspaceKey, liveURLKey, start, rows, label, 
					url, type, name, region, country, labelOrder);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, channelConfListRoot);
            
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

void API::addSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addSATChannelConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		int64_t sourceSATConfKey;

        string region;
        string country;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		int position = -1;
        Json::Value channelData = Json::nullValue;


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

			string field = "SourceSATConfKey";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
			sourceSATConfKey = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "Region";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				region = requestBodyRoot.get(field, "").asString();            

			field = "Country";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				country = requestBodyRoot.get(field, "").asString();            

			field = "ImageMediaItemKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				imageMediaItemKey = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "ImageUniqueName";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				imageUniqueName = requestBodyRoot.get(field, "").asString();            

			field = "Position";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				position = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "ChannelData";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				channelData = requestBodyRoot[field];
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
			int64_t confKey = _mmsEngineDBFacade->addSATChannelConf(
				workspace->_workspaceKey, sourceSATConfKey, region, country, 
				imageMediaItemKey, imageUniqueName, position, channelData);

			sResponse = (
				string("{ ") 
					+ "\"confKey\": " + to_string(confKey)
					+ "}"
				);            
		}
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addSATChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addSATChannelConf failed"
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

void API::modifySATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifySATChannelConf";

    _logger->info(__FILEREF__ + "Received " + api
        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		bool sourceSATConfKeyToBeModified;
		int64_t sourceSATConfKey = -1;
		bool regionToBeModified;
        string region;
		bool countryToBeModified;
        string country;
		bool imageToBeModified;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		bool positionToBeModified;
		int position = -1;
		bool channelDataToBeModified;
        Json::Value channelData = Json::nullValue;

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

            string field = "SourceSATConfKey";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				sourceSATConfKey = JSONUtils::asInt64(requestBodyRoot, field, -1);
				sourceSATConfKeyToBeModified = true;
            }

            field = "Region";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				region = requestBodyRoot.get(field, "XXX").asString();            
				regionToBeModified = true;
            }

            field = "Country";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				country = requestBodyRoot.get(field, "XXX").asString();            
				countryToBeModified = true;
            }

			field = "ImageMediaItemKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				imageMediaItemKey = JSONUtils::asInt64(requestBodyRoot, field, -1);            
				imageToBeModified = true;
			}

			field = "ImageUniqueName";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				imageUniqueName = requestBodyRoot.get(field, "").asString();            
				imageToBeModified = true;
			}

			field = "Position";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				position = JSONUtils::asInt(requestBodyRoot, field, -1);            
				positionToBeModified = true;
			}

            field = "ChannelData";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				channelData = requestBodyRoot[field];
				channelDataToBeModified = true;
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
                string errorMessage = string("The 'serviceId' parameter is not found");
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
            confKey = stoll(confKeyIt->second);

            _mmsEngineDBFacade->modifySATChannelConf(
                confKey, workspace->_workspaceKey,
				sourceSATConfKeyToBeModified, sourceSATConfKey,
				regionToBeModified, region,
				countryToBeModified, country,
				imageToBeModified, imageMediaItemKey, imageUniqueName,
				positionToBeModified, position,
				channelDataToBeModified, channelData);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifySATChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifySATChannelConf failed"
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

void API::removeSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeSATChannelConf";

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
            
            _mmsEngineDBFacade->removeSATChannelConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeSATChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeSATChannelConf failed"
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

void API::satChannelConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters)
{
    string api = "satChannelConfList";

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
				rows = _maxPageSize;
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

		string nameOrder;
		auto nameOrderIt = queryParameters.find("nameOrder");
		if (nameOrderIt != queryParameters.end() && nameOrderIt->second != "")
		{
			if (nameOrderIt->second == "asc" || nameOrderIt->second == "desc")
				nameOrder = nameOrderIt->second;
			else
				_logger->warn(__FILEREF__ + "satChannelList: 'nameOrder' parameter is unknown"
					+ ", nameOrder: " + nameOrderIt->second);
		}

        {
            Json::Value channelConfListRoot = _mmsEngineDBFacade->getSATChannelConfList(
                    workspace->_workspaceKey, confKey, region, country, start, rows,
					name, nameOrder);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, channelConfListRoot);
            
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

void API::addSourceSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addSourceSATChannelConf";

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

			string field = "ServiceId";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				serviceId = -1;
			else
				serviceId = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "NetworkId";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				networkId = -1;
			else
				networkId = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "TransportStreamId";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				transportStreamId = -1;
			else
				transportStreamId = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "Name";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			name = requestBodyRoot.get(field, "").asString();            

			field = "Satellite";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			satellite = requestBodyRoot.get(field, "").asString();            

			field = "Frequency";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			frequency = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "Lnb";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				lnb = requestBodyRoot.get(field, "").asString();            

			field = "VideoPid";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				videoPid = -1;
			else
				videoPid = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "AudioPids";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				audioPids = requestBodyRoot.get(field, "").asString();            

			field = "AudioItalianPid";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				audioItalianPid = -1;
			else
				audioItalianPid = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "AudioEnglishPid";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				audioEnglishPid = -1;
			else
				audioEnglishPid = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "TeletextPid";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				teletextPid = -1;
			else
				teletextPid = JSONUtils::asInt(requestBodyRoot, field, -1);            

			field = "Modulation";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				modulation = requestBodyRoot.get(field, "").asString();            

			field = "Polarization";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				polarization = requestBodyRoot.get(field, "").asString();            

			field = "SymbolRate";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				symbolRate = -1;
			else
				symbolRate = JSONUtils::asInt64(requestBodyRoot, field, -1);            

			field = "Country";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				country = requestBodyRoot.get(field, "").asString();            

			field = "DeliverySystem";
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
			int64_t confKey = _mmsEngineDBFacade->addSourceSATChannelConf(
				serviceId, networkId, transportStreamId,
				name, satellite, frequency, lnb,
				videoPid, audioPids, audioItalianPid, audioEnglishPid, teletextPid,
				modulation, polarization, symbolRate, country, deliverySystem
			);

			sResponse = (
				string("{ ") 
					+ "\"confKey\": " + to_string(confKey)
					+ "}"
				);
		}
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addSourceSATChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->addSourceSATChannelConf failed"
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

void API::modifySourceSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifySourceSATChannelConf";

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

            string field = "ServiceId";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				serviceId = JSONUtils::asInt64(requestBodyRoot, field, -1);
				serviceIdToBeModified = true;
            }

            field = "NetworkId";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				networkId = JSONUtils::asInt64(requestBodyRoot, field, -1);
				networkIdToBeModified = true;
            }

            field = "TransportStreamId";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				transportStreamId = JSONUtils::asInt64(requestBodyRoot, field, -1);
				transportStreamIdToBeModified = true;
            }

            field = "Name";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				name = requestBodyRoot.get(field, "").asString();            
				nameToBeModified = true;
            }

            field = "Satellite";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				satellite = requestBodyRoot.get(field, "").asString();            
				satelliteToBeModified = true;
            }

            field = "Frequency";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				frequency = JSONUtils::asInt64(requestBodyRoot, field, -1);
				frequencyToBeModified = true;
            }

            field = "Lnb";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				lnb = requestBodyRoot.get(field, "").asString();            
				lnbToBeModified = true;
            }

            field = "VideoPid";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				videoPid = JSONUtils::asInt(requestBodyRoot, field, -1);
				videoPidToBeModified = true;
            }

            field = "AudioPids";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				audioPids = requestBodyRoot.get(field, "").asString();            
				audioPidsToBeModified = true;
            }

            field = "AudioItalianPid";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				audioItalianPid = JSONUtils::asInt(requestBodyRoot, field, -1);
				audioItalianPidToBeModified = true;
            }

            field = "AudioEnglishPid";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				audioEnglishPid = JSONUtils::asInt(requestBodyRoot, field, -1);
				audioEnglishPidToBeModified = true;
            }

            field = "TeletextPid";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				teletextPid = JSONUtils::asInt(requestBodyRoot, field, -1);
				teletextPidToBeModified = true;
            }

            field = "Modulation";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				modulation = requestBodyRoot.get(field, "").asString();            
				modulationToBeModified = true;
            }

            field = "Polarization";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				polarization = requestBodyRoot.get(field, "").asString();            
				polarizationToBeModified = true;
            }

            field = "SymbolRate";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				symbolRate = JSONUtils::asInt64(requestBodyRoot, field, -1);
				symbolRateToBeModified = true;
            }

            field = "Country";
            if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
            {
				country = requestBodyRoot.get(field, "").asString();            
				countryToBeModified = true;
            }

            field = "DeliverySystem";
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

            _mmsEngineDBFacade->modifySourceSATChannelConf(
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

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifySourceSATChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifySourceSATChannelConf failed"
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

void API::removeSourceSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeSourceSATChannelConf";

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
            
            _mmsEngineDBFacade->removeSourceSATChannelConf(confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeSourceSATChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeSourceSATChannelConf failed"
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

void API::sourceSatChannelConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters)
{
    string api = "sourceSatChannelConfList";

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
				rows = _maxPageSize;
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
				_logger->warn(__FILEREF__ + "satChannelList: 'nameOrder' parameter is unknown"
					+ ", nameOrder: " + nameOrderIt->second);
		}

        {
            Json::Value channelConfListRoot = _mmsEngineDBFacade->getSourceSATChannelConfList(
                    workspace->_workspaceKey, confKey, start, rows,
					serviceId, name, frequency, videoPid, audioPids, nameOrder);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, channelConfListRoot);
            
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

void API::addFTPConf(
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

void API::modifyFTPConf(
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

void API::removeFTPConf(
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

void API::ftpConfList(
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

void API::addEMailConf(
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

void API::modifyEMailConf(
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

void API::removeEMailConf(
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

void API::emailConfList(
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

