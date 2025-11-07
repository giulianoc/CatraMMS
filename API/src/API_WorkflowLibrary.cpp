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

void API::workflowsAsLibraryList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "workflowsAsLibraryList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO("Received {}", api);

	try
	{
		json workflowListRoot = _mmsEngineDBFacade->getWorkflowsAsLibraryList(apiAuthorizationDetails->workspace->_workspaceKey);

		string responseBody = JSONUtils::toString(workflowListRoot);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw HTTPError(500);
	}
}

void API::workflowAsLibraryContent(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "workflowAsLibraryContent";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO("Received {}", api);

	try
	{
		int64_t workflowLibraryKey = -1;
		auto workflowLibraryKeyIt = queryParameters.find("workflowLibraryKey");
		if (workflowLibraryKeyIt == queryParameters.end())
		{
			string errorMessage = "'workflowLibraryKey' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		workflowLibraryKey = stoll(workflowLibraryKeyIt->second);

		string workflowLibraryContent = _mmsEngineDBFacade->getWorkflowAsLibraryContent(apiAuthorizationDetails->workspace->_workspaceKey, workflowLibraryKey);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, workflowLibraryContent);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw HTTPError(500);
	}
}

void API::saveWorkflowAsLibrary(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "saveWorkflowAsLibrary";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		string responseBody;

		{
			// validation and retrieving of the Label
			string workflowLabel;
			string workflowAsLibraryScope;
			{
				json requestBodyRoot = manageWorkflowVariables(requestBody, nullptr);

				Validator validator(_mmsEngineDBFacade, _configurationRoot);
				// it starts from the root and validate recursively the entire body
				validator.validateIngestedRootMetadata(apiAuthorizationDetails->workspace->_workspaceKey, requestBodyRoot);

				string field = "label";
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
				workflowLabel = JSONUtils::asString(requestBodyRoot, field, "");

				auto workflowAsLibraryScopeIt = queryParameters.find("scope");
				if (workflowAsLibraryScopeIt == queryParameters.end())
				{
					string errorMessage = "'scope' URI parameter is missing";
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				workflowAsLibraryScope = workflowAsLibraryScopeIt->second;

				if (workflowAsLibraryScope == "MMS" && !apiAuthorizationDetails->admin)
				{
					string errorMessage = std::format(
						"APIKey does not have the permission to add/update MMS WorkflowAsLibrary"
						", admin: {}",
						apiAuthorizationDetails->admin
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			int64_t thumbnailMediaItemKey = -1;
			auto thumbnailMediaItemKeyIt = queryParameters.find("thumbnailMediaItemKey");
			if (thumbnailMediaItemKeyIt != queryParameters.end() && thumbnailMediaItemKeyIt->second != "")
				thumbnailMediaItemKey = stoll(thumbnailMediaItemKeyIt->second);

			int64_t workflowLibraryKey = _mmsEngineDBFacade->addUpdateWorkflowAsLibrary(
				workflowAsLibraryScope == "MMS" ? -1 : apiAuthorizationDetails->userKey, workflowAsLibraryScope == "MMS" ? -1 : apiAuthorizationDetails->workspace->_workspaceKey, workflowLabel,
				thumbnailMediaItemKey, requestBody, apiAuthorizationDetails->admin
			);

			responseBody =
				(string("{ ") + "\"workflowLibraryKey\": " + to_string(workflowLibraryKey) + ", \"label\": \"" + workflowLabel + "\"" +
				 ", \"scope\": \"" + workflowAsLibraryScope + "\" " + "}");
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, responseBody);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);
		throw HTTPError(500);
	}
}

void API::removeWorkflowAsLibrary(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeWorkflowAsLibrary";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO("Received {}", api);

	try
	{
		auto workflowLibraryKeyIt = queryParameters.find("workflowLibraryKey");
		if (workflowLibraryKeyIt == queryParameters.end())
		{
			string errorMessage = "'workflowLibraryKey' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t workflowLibraryKey = stoll(workflowLibraryKeyIt->second);

		string workflowAsLibraryScope;
		auto workflowAsLibraryScopeIt = queryParameters.find("scope");
		if (workflowAsLibraryScopeIt == queryParameters.end())
		{
			string errorMessage = "'scope' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		workflowAsLibraryScope = workflowAsLibraryScopeIt->second;

		if (workflowAsLibraryScope == "MMS" && !apiAuthorizationDetails->admin)
		{
			string errorMessage = std::format(
				"APIKey does not have the permission to add/update MMS WorkflowAsLibrary"
				", admin: {}",
				apiAuthorizationDetails->admin
			);
			SPDLOG_ERROR(errorMessage);

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
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeWorkflowAsLibrary failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		string responseBody;

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw HTTPError(500);
	}
}
