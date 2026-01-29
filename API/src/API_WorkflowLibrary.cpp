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
#include "Convert.h"
#include "JSONUtils.h"
#include "Validator.h"
#include "spdlog/spdlog.h"
#include <format>
#include <sstream>

using namespace std;
using json = nlohmann::json;

void API::workflowsAsLibraryList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "workflowsAsLibraryList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO("Received {}", api);

	try
	{
		json workflowListRoot = _mmsEngineDBFacade->getWorkflowsAsLibraryList(apiAuthorizationDetails->workspace->_workspaceKey);

		string responseBody = JSONUtils::toString(workflowListRoot);

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::workflowAsLibraryContent(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "workflowAsLibraryContent";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO("Received {}", api);

	try
	{
		int64_t workflowLibraryKey = requestData.getQueryParameter("workflowLibraryKey", static_cast<int64_t>(-1), true);

		string workflowLibraryContent = _mmsEngineDBFacade->getWorkflowAsLibraryContent(apiAuthorizationDetails->workspace->_workspaceKey, workflowLibraryKey);

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, workflowLibraryContent);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::saveWorkflowAsLibrary(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "saveWorkflowAsLibrary";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", requestData.requestBody: {}",
		api, requestData.requestBody
	);

	try
	{
		string responseBody;

		{
			// validation and retrieving of the Label
			string workflowLabel;
			string workflowAsLibraryScope;
			{
				json requestBodyRoot = manageWorkflowVariables(requestData.requestBody, nullptr);

				Validator validator(_mmsEngineDBFacade, _configurationRoot);
				// it starts from the root and validate recursively the entire body
				validator.validateIngestedRootMetadata(apiAuthorizationDetails->workspace->_workspaceKey, requestBodyRoot);

				string field = "label";
				if (!JSONUtils::isPresent(requestBodyRoot, field))
				{
					string errorMessage = std::format(
						"Field is not present or it is null"
						", Field: {}",
						field
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				workflowLabel = JSONUtils::as<string>(requestBodyRoot, field, "");

				workflowAsLibraryScope = requestData.getQueryParameter("scope", "", true);

				if (workflowAsLibraryScope == "MMS" && !apiAuthorizationDetails->admin)
				{
					string errorMessage = std::format(
						"APIKey does not have the permission to add/update MMS WorkflowAsLibrary"
						", admin: {}",
						apiAuthorizationDetails->admin
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			int64_t thumbnailMediaItemKey = requestData.getQueryParameter("thumbnailMediaItemKey", static_cast<int64_t>(-1), false);

			int64_t workflowLibraryKey = _mmsEngineDBFacade->addUpdateWorkflowAsLibrary(
				workflowAsLibraryScope == "MMS" ? -1 : apiAuthorizationDetails->userKey, workflowAsLibraryScope == "MMS" ? -1 : apiAuthorizationDetails->workspace->_workspaceKey, workflowLabel,
				thumbnailMediaItemKey, requestData.requestBody, apiAuthorizationDetails->admin
			);

			responseBody =
				(string("{ ") + "\"workflowLibraryKey\": " + to_string(workflowLibraryKey) + ", \"label\": \"" + workflowLabel + "\"" +
				 ", \"scope\": \"" + workflowAsLibraryScope + "\" " + "}");
		}

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 201, responseBody);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		throw;
	}
}

void API::removeWorkflowAsLibrary(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "removeWorkflowAsLibrary";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO("Received {}", api);

	try
	{
		int64_t workflowLibraryKey = requestData.getQueryParameter("workflowLibraryKey", static_cast<int64_t>(-1), true);

		string workflowAsLibraryScope = requestData.getQueryParameter("scope", "", true);

		if (workflowAsLibraryScope == "MMS" && !apiAuthorizationDetails->admin)
		{
			string errorMessage = std::format(
				"APIKey does not have the permission to add/update MMS WorkflowAsLibrary"
				", admin: {}",
				apiAuthorizationDetails->admin
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			_mmsEngineDBFacade->removeWorkflowAsLibrary(
				workflowAsLibraryScope == "MMS" ? -1 : apiAuthorizationDetails->userKey, workflowAsLibraryScope == "MMS" ? -1 : apiAuthorizationDetails->workspace->_workspaceKey, workflowLibraryKey,
				apiAuthorizationDetails->admin
			);
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"_mmsEngineDBFacade->removeWorkflowAsLibrary failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		string responseBody;

		sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}
