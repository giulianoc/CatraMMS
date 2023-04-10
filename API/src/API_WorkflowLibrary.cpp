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
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "Validator.h"
#include "catralibraries/Convert.h"
#include "API.h"


void API::workflowsAsLibraryList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
    string api = "workflowsAsLibraryList";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
		Json::Value workflowListRoot = _mmsEngineDBFacade->getWorkflowsAsLibraryList(
			workspace->_workspaceKey);

		string responseBody = JSONUtils::toString(workflowListRoot);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 200, responseBody);
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


void API::workflowAsLibraryContent(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters)
{
	string api = "workflowAsLibraryContent";

	_logger->info(__FILEREF__ + "Received " + api
	);

    try
    {
		int64_t workflowLibraryKey = -1;
		auto workflowLibraryKeyIt = queryParameters.find("workflowLibraryKey");
		if (workflowLibraryKeyIt == queryParameters.end())
		{
			string errorMessage = string("'workflowLibraryKey' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		workflowLibraryKey = stoll(workflowLibraryKeyIt->second);

		string workflowLibraryContent = _mmsEngineDBFacade->getWorkflowAsLibraryContent(
			workspace->_workspaceKey, workflowLibraryKey);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 200, workflowLibraryContent);
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


void API::saveWorkflowAsLibrary(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
		int64_t userKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody, bool admin)
{
    string api = "saveWorkflowAsLibrary";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
        string responseBody;    

        {
			// validation and retrieving of the Label
			string workflowLabel;
			string workflowAsLibraryScope;
			{
				Json::Value requestBodyRoot = manageWorkflowVariables(requestBody, Json::nullValue);

				Validator validator(_logger, _mmsEngineDBFacade, _configuration);
				// it starts from the root and validate recursively the entire body
				validator.validateIngestedRootMetadata(workspace->_workspaceKey,
                    requestBodyRoot);

				string field = "label";
				if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				workflowLabel = JSONUtils::asString(requestBodyRoot, field, "");

				auto workflowAsLibraryScopeIt = queryParameters.find("scope");
				if (workflowAsLibraryScopeIt == queryParameters.end())
				{
					string errorMessage = string("'scope' URI parameter is missing");
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
				workflowAsLibraryScope = workflowAsLibraryScopeIt->second;

				if (workflowAsLibraryScope == "MMS" && !admin)
				{
					string errorMessage = string("APIKey does not have the permission to add/update MMS WorkflowAsLibrary")
						+ ", admin: " + to_string(admin)
					;
					_logger->error(__FILEREF__ + errorMessage);

					// sendError(request, 403, errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			int64_t thumbnailMediaItemKey = -1;
			auto thumbnailMediaItemKeyIt = queryParameters.find("thumbnailMediaItemKey");
			if (thumbnailMediaItemKeyIt != queryParameters.end() && thumbnailMediaItemKeyIt->second != "")
				thumbnailMediaItemKey = stoll(thumbnailMediaItemKeyIt->second);

			int64_t workflowLibraryKey = _mmsEngineDBFacade->addUpdateWorkflowAsLibrary(
				workflowAsLibraryScope == "MMS" ? -1 : userKey,
				workflowAsLibraryScope == "MMS" ? -1 : workspace->_workspaceKey,
				workflowLabel, thumbnailMediaItemKey, requestBody, admin);

            responseBody = (
                    string("{ ") 
                    + "\"workflowLibraryKey\": " + to_string(workflowLibraryKey)
                    + ", \"label\": \"" + workflowLabel + "\""
                    + ", \"scope\": \"" + workflowAsLibraryScope + "\" "
                    + "}"
                    );            
        }

        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 201, responseBody);
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

void API::removeWorkflowAsLibrary(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
        FCGX_Request& request,
		int64_t userKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters, bool admin)
{
    string api = "removeWorkflowAsLibrary";

    _logger->info(__FILEREF__ + "Received " + api
    );

    try
    {
        auto workflowLibraryKeyIt = queryParameters.find("workflowLibraryKey");
        if (workflowLibraryKeyIt == queryParameters.end())
        {
            string errorMessage = string("'workflowLibraryKey' URI parameter is missing");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);            
        }
        int64_t workflowLibraryKey = stoll(workflowLibraryKeyIt->second);

		string workflowAsLibraryScope;
		auto workflowAsLibraryScopeIt = queryParameters.find("scope");
		if (workflowAsLibraryScopeIt == queryParameters.end())
		{
			string errorMessage = string("'scope' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		workflowAsLibraryScope = workflowAsLibraryScopeIt->second;

		if (workflowAsLibraryScope == "MMS" && !admin)
		{
			string errorMessage = string("APIKey does not have the permission to add/update MMS WorkflowAsLibrary"
				", admin: " + to_string(admin)
			);
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}

        try
        {
			_mmsEngineDBFacade->removeWorkflowAsLibrary(
				workflowAsLibraryScope == "MMS" ? -1 : userKey,
				workflowAsLibraryScope == "MMS" ? -1 : workspace->_workspaceKey,
				workflowLibraryKey, admin);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__
				+ "_mmsEngineDBFacade->removeWorkflowAsLibrary failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__
				+ "_mmsEngineDBFacade->removeWorkflowAsLibrary failed"
                + ", e.what(): " + e.what()
            );

            throw e;
        }

        string responseBody;
        
        sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed,
			request, "", api, 200, responseBody);
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

