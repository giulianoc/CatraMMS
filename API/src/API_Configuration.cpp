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

/*
#include <fstream>
#include <sstream>
#include "catralibraries/Convert.h"
#include "catralibraries/LdapWrapper.h"
#include "Validator.h"
#include "EMailSender.h"
*/
#include "JSONUtils.h"
#include <regex>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "API.h"


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

void API::addChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "addChannelConf";

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
            int64_t confKey = _mmsEngineDBFacade->addChannelConf(
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

void API::modifyChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "modifyChannelConf";

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

            _mmsEngineDBFacade->modifyChannelConf(
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

void API::removeChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "removeChannelConf";

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
            
            _mmsEngineDBFacade->removeChannelConf(
                workspace->_workspaceKey, confKey);

            sResponse = (
                    string("{ ") 
                    + "\"confKey\": " + to_string(confKey)
                    + "}"
                    );            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeChannelConf failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeChannelConf failed"
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

void API::channelConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters)
{
    string api = "channelConfList";

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

			string labelDecoded = curlpp::unescape(label);
			// still there is the '+' char
			string plus = "\\+";
			string plusDecoded = " ";
			label = regex_replace(labelDecoded, regex(plus), plusDecoded);

			/*
			curl = curl_easy_init();                                                                    
			if(curl)                                                                                          
			{                                                                                                 
				int outLength;                                                                                
				char *decoded = curl_easy_unescape(curl,                                                      
				title.c_str(), title.length(), &outLength);                                           
				if(decoded)                                                                                   
				{                                                                                             
					string sDecoded = decoded;                                                                
					curl_free(decoded);                                                                       
                                                                                                              
					// still there is the '+' char                                                            
					string plus = "\\+";                                                                      
					string plusDecoded = " ";                                                                 
					title = regex_replace(sDecoded, regex(plus), plusDecoded);                                
				}                                                                                             
			}                                                                                                 
			*/                     
		}

		string url;
		auto urlIt = queryParameters.find("url");
		if (urlIt != queryParameters.end() && urlIt->second != "")
		{
			url = urlIt->second;

			string urlDecoded = curlpp::unescape(url);
			// still there is the '+' char
			string plus = "\\+";
			string plusDecoded = " ";
			url = regex_replace(urlDecoded, regex(plus), plusDecoded);
		}

		string type;
		auto typeIt = queryParameters.find("type");
		if (typeIt != queryParameters.end() && typeIt->second != "")
		{
			type = typeIt->second;

			string typeDecoded = curlpp::unescape(type);
			// still there is the '+' char
			string plus = "\\+";
			string plusDecoded = " ";
			type = regex_replace(typeDecoded, regex(plus), plusDecoded);
		}

		string name;
		auto nameIt = queryParameters.find("name");
		if (nameIt != queryParameters.end() && nameIt->second != "")
		{
			name = nameIt->second;

			string nameDecoded = curlpp::unescape(name);
			// still there is the '+' char
			string plus = "\\+";
			string plusDecoded = " ";
			name = regex_replace(nameDecoded, regex(plus), plusDecoded);
		}

		string region;
		auto regionIt = queryParameters.find("region");
		if (regionIt != queryParameters.end() && regionIt->second != "")
		{
			region = regionIt->second;

			string regionDecoded = curlpp::unescape(region);
			// still there is the '+' char
			string plus = "\\+";
			string plusDecoded = " ";
			region = regex_replace(regionDecoded, regex(plus), plusDecoded);
		}

		string country;
		auto countryIt = queryParameters.find("country");
		if (countryIt != queryParameters.end() && countryIt->second != "")
		{
			country = countryIt->second;

			string countryDecoded = curlpp::unescape(country);
			// still there is the '+' char
			string plus = "\\+";
			string plusDecoded = " ";
			country = regex_replace(countryDecoded, regex(plus), plusDecoded);
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
            
            Json::Value channelConfListRoot = _mmsEngineDBFacade->getChannelConfList(
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

