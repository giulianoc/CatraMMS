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
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include "Validator.h"
#include "spdlog/spdlog.h"
#include <format>
#include <regex>

void API::addYouTubeConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addYouTubeConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);

		throw HTTPError(403);
	}

	try
	{
		string label;
		string tokenType;
		string refreshToken;
		string accessToken;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "tokenType";
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
			tokenType = JSONUtils::asString(requestBodyRoot, field, "");

			if (tokenType == "RefreshToken")
			{
				field = "refreshToken";
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
				refreshToken = JSONUtils::asString(requestBodyRoot, field, "");
			}
			else // if (tokenType == "AccessToken")
			{
				field = "accessToken";
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
				accessToken = JSONUtils::asString(requestBodyRoot, field, "");
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}",
				requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			Validator validator(_mmsEngineDBFacade, _configurationRoot);
			if (!validator.isYouTubeTokenTypeValid(tokenType))
			{
				string errorMessage = string("The 'tokenType' is not valid") + ", tokenType: " + tokenType;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (tokenType == "RefreshToken")
			{
				if (refreshToken.empty())
				{
					string errorMessage = "The 'refreshToken' is not valid (empty)";
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else // if (tokenType == "AccessToken")
			{
				if (accessToken.empty())
				{
					string errorMessage = "The 'accessToken' is not valid (empty)";
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			json youTubeRoot = _mmsEngineDBFacade->addYouTubeConf(apiAuthorizationDetails->workspace->_workspaceKey, label, tokenType, refreshToken, accessToken);

			sResponse = JSONUtils::toString(youTubeRoot);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addYouTubeConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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

		throw;
	}
}

void API::modifyYouTubeConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyYouTubeConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);

		throw HTTPError(403);
	}

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
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "label";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				label = JSONUtils::asString(requestBodyRoot, field, "");
				labelModified = true;
			}

			field = "tokenType";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				tokenType = JSONUtils::asString(requestBodyRoot, field, "");
				tokenTypeModified = true;
			}

			field = "refreshToken";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				refreshToken = JSONUtils::asString(requestBodyRoot, field, "");
				refreshTokenModified = true;
			}

			field = "accessToken";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				accessToken = JSONUtils::asString(requestBodyRoot, field, "");
				accessTokenModified = true;
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = getQueryParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

			if (tokenTypeModified)
			{
				Validator validator(_mmsEngineDBFacade, _configurationRoot);
				if (!validator.isYouTubeTokenTypeValid(tokenType))
				{
					string errorMessage = string("The 'tokenType' is not valid");
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				if (tokenType == "RefreshToken")
				{
					if (!refreshTokenModified || refreshToken.empty())
					{
						string errorMessage = string("The 'refreshToken' is not valid");
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				else // if (tokenType == "AccessToken")
				{
					if (!accessTokenModified || accessToken == "")
					{
						string errorMessage = string("The 'accessToken' is not valid");
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
			}

			json youTubeRoot = _mmsEngineDBFacade->modifyYouTubeConf(
				confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, labelModified, tokenType, tokenTypeModified, refreshToken, refreshTokenModified,
				accessToken, accessTokenModified
			);

			sResponse = JSONUtils::toString(youTubeRoot);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->modifyYouTubeConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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

		throw;
	}
}

void API::removeYouTubeConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeYouTubeConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);

		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeYouTubeConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);

		throw;
	}
}

void API::youTubeConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "youTubeConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		string label = getQueryParameter(queryParameters, "label", string(), false);

		{
			json youTubeConfListRoot = _mmsEngineDBFacade->getYouTubeConfList(apiAuthorizationDetails->workspace->_workspaceKey, label);

			string responseBody = JSONUtils::toString(youTubeConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addFacebookConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addFacebookConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string userAccessToken;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "UserAccessToken";
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
			userAccessToken = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addFacebookConf(apiAuthorizationDetails->workspace->_workspaceKey, label, userAccessToken);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addFacebookConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyFacebookConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyFacebookConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string userAccessToken;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "UserAccessToken";
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
			userAccessToken = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifyFacebookConf(confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, userAccessToken);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeFacebookConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeFacebookConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeFacebookConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::facebookConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "facebookConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), false);
		if (confKey == 0)
			confKey = -1;

		string label;
		if (confKey == -1)
			label = getMapParameter(queryParameters, "label", string(), false);

		{
			json facebookConfListRoot = _mmsEngineDBFacade->getFacebookConfList(apiAuthorizationDetails->workspace->_workspaceKey, confKey, label);

			string responseBody = JSONUtils::toString(facebookConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addTwitchConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addTwitchConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string refreshToken;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "RefreshToken";
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
			refreshToken = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addTwitchConf(apiAuthorizationDetails->workspace->_workspaceKey, label, refreshToken);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addTwitchConf failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyTwitchConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyTwitchConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string refreshToken;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "RefreshToken";
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
			refreshToken = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifyTwitchConf(confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, refreshToken);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeTwitchConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeTwitchConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeTwitchConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::twitchConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "twitchConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), false);
		if (confKey == 0)
			confKey = -1;

		string label;
		if (confKey == -1)
			label = getMapParameter(queryParameters, "label", string(), false);

		{
			json twitchConfListRoot = _mmsEngineDBFacade->getTwitchConfList(apiAuthorizationDetails->workspace->_workspaceKey, confKey, label);

			string responseBody = JSONUtils::toString(twitchConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addTiktokConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addTiktokConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	try
	{
		string label;
		string token;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Token";
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
			token = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}",
				requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addTiktokConf(apiAuthorizationDetails->workspace->_workspaceKey, label, token);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addTiktokConf failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyTiktokConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyTiktokConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	try
	{
		string label;
		string token;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Token";
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
			token = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}",
				requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifyTiktokConf(confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, token);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeTiktokConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeTiktokConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeTiktokConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::tiktokConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "tiktokConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), false);
		if (confKey == 0)
			confKey = -1;

		string label;
		if (confKey == -1)
			label = getMapParameter(queryParameters, "label", string(), false);

		{
			json tiktokConfListRoot = _mmsEngineDBFacade->getTiktokConfList(apiAuthorizationDetails->workspace->_workspaceKey, confKey, label);

			string responseBody = JSONUtils::toString(tiktokConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addStream(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addStream";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string sourceType;

		int64_t encodersPoolKey;
		string url;
		string pushProtocol;
		int64_t pushEncoderKey;
		bool pushPublicEncoderName;
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
		json userData = nullptr;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "sourceType";
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
			sourceType = JSONUtils::asString(requestBodyRoot, field, "");

			field = "encodersPoolKey";
			encodersPoolKey = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "url";
			url = JSONUtils::asString(requestBodyRoot, field, "");

			field = "pushProtocol";
			pushProtocol = JSONUtils::asString(requestBodyRoot, field, "");

			field = "pushEncoderKey";
			pushEncoderKey = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "pushPublicEncoderName";
			pushPublicEncoderName = JSONUtils::asBool(requestBodyRoot, field, false);

			field = "pushServerPort";
			pushServerPort = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "pushURI";
			pushUri = JSONUtils::asString(requestBodyRoot, field, "");

			field = "pushListenTimeout";
			pushListenTimeout = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveVideoDeviceNumber";
			captureLiveVideoDeviceNumber = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveVideoInputFormat";
			captureLiveVideoInputFormat = JSONUtils::asString(requestBodyRoot, field, "");

			field = "captureLiveFrameRate";
			captureLiveFrameRate = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveWidth";
			captureLiveWidth = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveHeight";
			captureLiveHeight = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveAudioDeviceNumber";
			captureLiveAudioDeviceNumber = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "captureLiveChannelsNumber";
			captureLiveChannelsNumber = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "sourceTVConfKey";
			tvSourceTVConfKey = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "type";
			type = JSONUtils::asString(requestBodyRoot, field, "");

			field = "description";
			description = JSONUtils::asString(requestBodyRoot, field, "");

			field = "name";
			name = JSONUtils::asString(requestBodyRoot, field, "");

			field = "region";
			region = JSONUtils::asString(requestBodyRoot, field, "");

			field = "country";
			country = JSONUtils::asString(requestBodyRoot, field, "");

			field = "imageMediaItemKey";
			imageMediaItemKey = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "imageUniqueName";
			imageUniqueName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "position";
			position = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "userData";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
				userData = requestBodyRoot[field];
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			json streamRoot = _mmsEngineDBFacade->addStream(
				apiAuthorizationDetails->workspace->_workspaceKey, label, sourceType, encodersPoolKey, url, pushProtocol, pushEncoderKey, pushPublicEncoderName,
				pushServerPort, pushUri, pushListenTimeout, captureLiveVideoDeviceNumber, captureLiveVideoInputFormat, captureLiveFrameRate,
				captureLiveWidth, captureLiveHeight, captureLiveAudioDeviceNumber, captureLiveChannelsNumber, tvSourceTVConfKey, type, description,
				name, region, country, imageMediaItemKey, imageUniqueName, position, userData
			);

			sResponse = JSONUtils::toString(streamRoot);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addStream failed"
				", e.what(): {}",
				e.what()
			);

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyStream(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyStream";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string sourceType;

		int64_t encodersPoolKey;
		string url;
		string pushProtocol;
		int64_t pushEncoderKey;
		bool pushPublicEncoderName;
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
		json userData;

		bool labelToBeModified;
		bool sourceTypeToBeModified;
		bool encodersPoolKeyToBeModified;
		bool urlToBeModified;
		bool pushProtocolToBeModified;
		bool pushEncoderKeyToBeModified;
		bool pushPublicEncoderNameToBeModified;
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
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			labelToBeModified = false;
			string field = "label";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				label = JSONUtils::asString(requestBodyRoot, field, "");
				labelToBeModified = true;
			}

			sourceTypeToBeModified = false;
			field = "sourceType";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				sourceType = JSONUtils::asString(requestBodyRoot, field, "");
				sourceTypeToBeModified = true;
			}

			encodersPoolKeyToBeModified = false;
			field = "encodersPoolKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				encodersPoolKey = JSONUtils::asInt64(requestBodyRoot, field, -1);
				encodersPoolKeyToBeModified = true;
			}

			urlToBeModified = false;
			field = "url";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				url = JSONUtils::asString(requestBodyRoot, field, "");
				urlToBeModified = true;
			}

			pushProtocolToBeModified = false;
			field = "pushProtocol";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				pushProtocol = JSONUtils::asString(requestBodyRoot, field, "");
				pushProtocolToBeModified = true;
			}

			pushEncoderKeyToBeModified = false;
			field = "pushEncoderKey";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				pushEncoderKey = JSONUtils::asInt64(requestBodyRoot, field, -1);
				pushEncoderKeyToBeModified = true;
			}

			pushPublicEncoderNameToBeModified = false;
			field = "pushPublicEncoderName";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				pushPublicEncoderName = JSONUtils::asBool(requestBodyRoot, field, false);
				pushPublicEncoderNameToBeModified = true;
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
				pushUri = JSONUtils::asString(requestBodyRoot, field, "");
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
				captureLiveVideoInputFormat = JSONUtils::asString(requestBodyRoot, field, "");
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
				type = JSONUtils::asString(requestBodyRoot, field, "");
				typeToBeModified = true;
			}

			descriptionToBeModified = false;
			field = "description";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				description = JSONUtils::asString(requestBodyRoot, field, "");
				descriptionToBeModified = true;
			}

			nameToBeModified = false;
			field = "name";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				name = JSONUtils::asString(requestBodyRoot, field, "");
				nameToBeModified = true;
			}

			regionToBeModified = false;
			field = "region";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				region = JSONUtils::asString(requestBodyRoot, field, "");
				regionToBeModified = true;
			}

			countryToBeModified = false;
			field = "country";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				country = JSONUtils::asString(requestBodyRoot, field, "");
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
				imageUniqueName = JSONUtils::asString(requestBodyRoot, field, "");
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
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), false);
			string labelKey = getMapParameter(queryParameters, "label", string(), false);

			if (confKey == -1 && labelKey.empty())
			{
				string errorMessage = string("The 'confKey/label' parameter is not found");
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			json streamRoot = _mmsEngineDBFacade->modifyStream(
				confKey, labelKey, apiAuthorizationDetails->workspace->_workspaceKey, labelToBeModified, label, sourceTypeToBeModified, sourceType,
				encodersPoolKeyToBeModified, encodersPoolKey, urlToBeModified, url, pushProtocolToBeModified, pushProtocol,
				pushEncoderKeyToBeModified, pushEncoderKey, pushPublicEncoderNameToBeModified, pushPublicEncoderName, pushServerPortToBeModified,
				pushServerPort, pushUriToBeModified, pushUri, pushListenTimeoutToBeModified, pushListenTimeout,
				captureLiveVideoDeviceNumberToBeModified, captureLiveVideoDeviceNumber, captureLiveVideoInputFormatToBeModified,
				captureLiveVideoInputFormat, captureLiveFrameRateToBeModified, captureLiveFrameRate, captureLiveWidthToBeModified, captureLiveWidth,
				captureLiveHeightToBeModified, captureLiveHeight, captureLiveAudioDeviceNumberToBeModified, captureLiveAudioDeviceNumber,
				captureLiveChannelsNumberToBeModified, captureLiveChannelsNumber, tvSourceTVConfKeyToBeModified, tvSourceTVConfKey, typeToBeModified,
				type, descriptionToBeModified, description, nameToBeModified, name, regionToBeModified, region, countryToBeModified, country,
				imageToBeModified, imageMediaItemKey, imageUniqueName, positionToBeModified, position, userDataToBeModified, userData
			);

			sResponse = JSONUtils::toString(streamRoot);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->modifyStream failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeStream(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeStream";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string sResponse;
		try
		{
			int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), false);
			string label = getMapParameter(queryParameters, "label", string(), false);
			if (confKey == -1 && label.empty())
			{
				string errorMessage = string("The 'confKey/label' parameter is not found");
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			_mmsEngineDBFacade->removeStream(apiAuthorizationDetails->workspace->_workspaceKey, confKey, label);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->removeStream failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::streamList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "streamList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), false);
		int32_t start = getMapParameter(queryParameters, "start", static_cast<int32_t>(0), false);
		int32_t rows = getMapParameter(queryParameters, "rows", static_cast<int32_t>(30), false);
		if (rows > _maxPageSize)
		{
			// 2022-02-13: changed to return an error otherwise the user
			//	think to ask for a huge number of items while the return is much less

			// rows = _maxPageSize;

			string errorMessage =
				__FILEREF__ + "rows parameter too big" + ", rows: " + to_string(rows) + ", _maxPageSize: " + to_string(_maxPageSize);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string label = getMapParameter(queryParameters, "label", string(""), false);
		bool labelLike = getMapParameter(queryParameters, "labelLike", true, false);
		string url = getMapParameter(queryParameters, "url", string(""), false);
		string sourceType = getMapParameter(queryParameters, "sourceType", string(""), false);
		if (sourceType != "" && sourceType != "IP_PULL" && sourceType != "IP_PUSH"
			&& sourceType != "CaptureLive" && sourceType != "TV")
		{
			SPDLOG_WARN(
				"streamList: 'sourceType' parameter is unknown"
				", sourceType: {}",
				sourceType
			);
			sourceType = "";
		}
		string type = getMapParameter(queryParameters, "type", string(""), false);
		string name = getMapParameter(queryParameters, "name", string(""), false);
		string region = getMapParameter(queryParameters, "region", string(""), false);
		string country = getMapParameter(queryParameters, "country", string(""), false);
		string labelOrder = getMapParameter(queryParameters, "labelOrder", string(""), false);
		if (labelOrder != "asc" && labelOrder != "desc")
		{
			SPDLOG_WARN(
				"liveURLList: 'labelOrder' parameter is unknown"
				", labelOrder: {}",
				labelOrder
			);
			labelOrder = "";
		}

		json streamListRoot = _mmsEngineDBFacade->getStreamList(
			apiAuthorizationDetails->workspace->_workspaceKey, confKey, start, rows, label, labelLike,
			url, sourceType, type, name, region, country, labelOrder
		);

		string responseBody = JSONUtils::toString(streamListRoot);

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
		throw;
	}
}

void API::streamFreePushEncoderPort(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "streamFreePushEncoderPort";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		const int64_t encoderKey = getMapParameter(queryParameters, "encoderKey", -1, true);

		const json streamFreePushEncoderPortRoot = _mmsEngineDBFacade->getStreamFreePushEncoderPort(encoderKey);

		const string responseBody = JSONUtils::toString(streamFreePushEncoderPortRoot);

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
		throw;
	}
}

void API::addSourceTVStream(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addSourceTVStream";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string type;
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
		int64_t bandwidthInHz;
		string country;
		string deliverySystem;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");

			field = "serviceId";
			serviceId = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "networkId";
			networkId = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "transportStreamId";
			transportStreamId = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "name";
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
			name = JSONUtils::asString(requestBodyRoot, field, "");

			field = "satellite";
			satellite = JSONUtils::asString(requestBodyRoot, field, "");

			field = "frequency";
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
			frequency = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "lnb";
			lnb = JSONUtils::asString(requestBodyRoot, field, "");

			field = "videoPid";
			videoPid = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "audioPids";
			audioPids = JSONUtils::asString(requestBodyRoot, field, "");

			field = "audioItalianPid";
			audioItalianPid = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "audioEnglishPid";
			audioEnglishPid = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "teletextPid";
			teletextPid = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "modulation";
			modulation = JSONUtils::asString(requestBodyRoot, field, "");

			field = "polarization";
			polarization = JSONUtils::asString(requestBodyRoot, field, "");

			field = "symbolRate";
			symbolRate = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "bandwidthInHz";
			bandwidthInHz = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "country";
			country = JSONUtils::asString(requestBodyRoot, field, "");

			field = "deliverySystem";
			deliverySystem = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			json sourceTVStreamRoot = _mmsEngineDBFacade->addSourceTVStream(
				type, serviceId, networkId, transportStreamId, name, satellite, frequency, lnb, videoPid, audioPids, audioItalianPid, audioEnglishPid,
				teletextPid, modulation, polarization, symbolRate, bandwidthInHz, country, deliverySystem
			);

			sResponse = JSONUtils::toString(sourceTVStreamRoot);
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addSourceTVStream failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifySourceTVStream(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifySourceTVStream";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		bool typeToBeModified;
		string type;
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
		bool bandwidthInHzToBeModified;
		int64_t bandwidthInHz = -1;
		bool countryToBeModified;
		string country;
		bool deliverySystemToBeModified;
		string deliverySystem;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "type";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				type = JSONUtils::asString(requestBodyRoot, field, "");
				typeToBeModified = true;
			}

			field = "serviceId";
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
				name = JSONUtils::asString(requestBodyRoot, field, "");
				nameToBeModified = true;
			}

			field = "satellite";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				satellite = JSONUtils::asString(requestBodyRoot, field, "");
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
				lnb = JSONUtils::asString(requestBodyRoot, field, "");
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
				audioPids = JSONUtils::asString(requestBodyRoot, field, "");
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
				modulation = JSONUtils::asString(requestBodyRoot, field, "");
				modulationToBeModified = true;
			}

			field = "polarization";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				polarization = JSONUtils::asString(requestBodyRoot, field, "");
				polarizationToBeModified = true;
			}

			field = "symbolRate";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				symbolRate = JSONUtils::asInt64(requestBodyRoot, field, -1);
				symbolRateToBeModified = true;
			}

			field = "bandwidthInHz";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				bandwidthInHz = JSONUtils::asInt64(requestBodyRoot, field, -1);
				bandwidthInHzToBeModified = true;
			}

			field = "country";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				country = JSONUtils::asString(requestBodyRoot, field, "");
				countryToBeModified = true;
			}

			field = "deliverySystem";
			if (JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				deliverySystem = JSONUtils::asString(requestBodyRoot, field, "");
				deliverySystemToBeModified = true;
			}
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		json sourceTVStreamRoot = _mmsEngineDBFacade->modifySourceTVStream(
			confKey, typeToBeModified, type, serviceIdToBeModified, serviceId, networkIdToBeModified, networkId, transportStreamIdToBeModified,
			transportStreamId, nameToBeModified, name, satelliteToBeModified, satellite, frequencyToBeModified, frequency, lnbToBeModified, lnb,
			videoPidToBeModified, videoPid, audioPidsToBeModified, audioPids, audioItalianPidToBeModified, audioItalianPid,
			audioEnglishPidToBeModified, audioEnglishPid, teletextPidToBeModified, teletextPid, modulationToBeModified, modulation,
			polarizationToBeModified, polarization, symbolRateToBeModified, symbolRate, bandwidthInHzToBeModified, bandwidthInHz,
			countryToBeModified, country, deliverySystemToBeModified, deliverySystem
		);

		string sResponse = JSONUtils::toString(sourceTVStreamRoot);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeSourceTVStream(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeSourceTVStream";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", admin: {}",
			apiAuthorizationDetails->admin
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeSourceTVStream(confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::sourceTVStreamList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters)
{
	string api = "sourceTVStreamList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), false);
		int32_t start = getMapParameter(queryParameters, "start", static_cast<int32_t>(0), false);
		int32_t rows = getMapParameter(queryParameters, "rows", static_cast<int32_t>(30), false);
		if (rows > _maxPageSize)
		{
			// 2022-02-13: changed to return an error otherwise the user
			//	think to ask for a huge number of items while the return is much less

			// rows = _maxPageSize;

			string errorMessage =
				__FILEREF__ + "rows parameter too big" + ", rows: " + to_string(rows) + ", _maxPageSize: " + to_string(_maxPageSize);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string type = getMapParameter(queryParameters, "type", string(""), false);
		int64_t serviceId = getMapParameter(queryParameters, "serviceId", static_cast<int64_t>(-1), false);
		string name = getMapParameter(queryParameters, "name", string(""), false);
		string lnb = getMapParameter(queryParameters, "lnb", string(""), false);
		int64_t frequency = getMapParameter(queryParameters, "frequency", static_cast<int64_t>(-1), false);
		int32_t videoPid = getMapParameter(queryParameters, "videoPid", static_cast<int32_t>(-1), false);
		string audioPids = getMapParameter(queryParameters, "audioPids", string(""), false);
		string nameOrder = getMapParameter(queryParameters, "nameOrder", string(""), false);
		if (nameOrder != "asc" && nameOrder != "desc")
		{
			SPDLOG_WARN(
				"tvChannelList: 'nameOrder' parameter is unknown"
				", nameOrder: {}",
				nameOrder
			);
			nameOrder = "";
		}

		{
			json sourceTVStreamRoot = _mmsEngineDBFacade->getSourceTVStreamList(
				confKey, start, rows, type, serviceId, name, frequency, lnb, videoPid, audioPids, nameOrder
			);

			string responseBody = JSONUtils::toString(sourceTVStreamRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addAWSChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addAWSChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string channelId;
		string rtmpURL;
		string playURL;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "channelId";
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
			channelId = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
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
			rtmpURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "playURL";
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
			playURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addAWSChannelConf(apiAuthorizationDetails->workspace->_workspaceKey, label, channelId, rtmpURL, playURL, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addAWSChannelConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyAWSChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyAWSChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string channelId;
		string rtmpURL;
		string playURL;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "channelId";
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
			channelId = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
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
			rtmpURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "playURL";
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
			playURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifyAWSChannelConf(confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, channelId, rtmpURL, playURL, type);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeAWSChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeAWSChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);
		_mmsEngineDBFacade->removeAWSChannelConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::awsChannelConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "awsChannelConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		string label = getQueryParameter(queryParameters, "label", string(), false);
		const bool labelLike = getQueryParameter(queryParameters, "labelLike", false, false);

		int type = 0; // ALL
		string sType = getQueryParameter(queryParameters, "type", string(), false);
		if (sType != "")
		{
			if (sType == "SHARED")
				type = 1;
			else if (sType == "DEDICATED")
				type = 2;
		}
		{
			json awsChannelConfListRoot = _mmsEngineDBFacade->getAWSChannelConfList(apiAuthorizationDetails->workspace->_workspaceKey, -1, label, labelLike, type);

			string responseBody = JSONUtils::toString(awsChannelConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addCDN77ChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addCDN77ChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		bool srtFeed;
		string srtURL;
		string rtmpURL;
		string resourceURL;
		string filePath;
		string secureToken;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			srtFeed = JSONUtils::asBool(requestBodyRoot, "srtFeed", false);

			if (srtFeed)
			{
				field = "srtURL";
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
			}
			else
			{
				field = "rtmpURL";
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
			}
			// è possibile salvare entrambe le url
			rtmpURL = JSONUtils::asString(requestBodyRoot, "rtmpURL", "");
			srtURL = JSONUtils::asString(requestBodyRoot, "srtURL", "");

			field = "resourceURL";
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
			resourceURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "filePath";
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
			filePath = JSONUtils::asString(requestBodyRoot, field, "");

			field = "secureToken";
			secureToken = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addCDN77ChannelConf(
				apiAuthorizationDetails->workspace->_workspaceKey, label, srtFeed, srtURL, rtmpURL, resourceURL, filePath, secureToken, type
			);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addCDN77ChannelConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyCDN77ChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyCDN77ChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		bool srtFeed;
		string srtURL;
		string rtmpURL;
		string resourceURL;
		string filePath;
		string secureToken;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			srtFeed = JSONUtils::asBool(requestBodyRoot, "srtFeed", false);

			if (srtFeed)
			{
				field = "srtURL";
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
			}
			else
			{
				field = "rtmpURL";
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
			}
			rtmpURL = JSONUtils::asString(requestBodyRoot, "rtmpURL", "");
			srtURL = JSONUtils::asString(requestBodyRoot, "srtURL", "");

			field = "resourceURL";
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
			resourceURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "filePath";
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
			filePath = JSONUtils::asString(requestBodyRoot, field, "");

			field = "secureToken";
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
			secureToken = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifyCDN77ChannelConf(
			confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, srtFeed, srtURL, rtmpURL, resourceURL, filePath, secureToken, type
		);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeCDN77ChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeCDN77ChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeCDN77ChannelConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::cdn77ChannelConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "cdn77ChannelConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		string label = getQueryParameter(queryParameters, "label", string(), false);
		bool labelLike = getQueryParameter(queryParameters, "labelLike", false, false);

		int type = 0; // ALL
		string sType = getQueryParameter(queryParameters, "type", string(), false);
		if (sType != "")
		{
			if (sType == "SHARED")
				type = 1;
			else if (sType == "DEDICATED")
				type = 2;
		}
		{
			json cdn77ChannelConfListRoot = _mmsEngineDBFacade->getCDN77ChannelConfList(apiAuthorizationDetails->workspace->_workspaceKey, -1, label, labelLike, type);

			string responseBody = JSONUtils::toString(cdn77ChannelConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addRTMPChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addRTMPChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string rtmpURL;
		string streamName;
		string userName;
		string password;
		string playURL;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
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
			rtmpURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "streamName";
			streamName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "userName";
			userName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "password";
			password = JSONUtils::asString(requestBodyRoot, field, "");

			field = "playURL";
			playURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey =
				_mmsEngineDBFacade->addRTMPChannelConf(apiAuthorizationDetails->workspace->_workspaceKey, label, rtmpURL, streamName, userName, password, playURL, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addRTMPChannelConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyRTMPChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyRTMPChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string rtmpURL;
		string streamName;
		string userName;
		string password;
		string playURL;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
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
			rtmpURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "streamName";
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
			streamName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "userName";
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
			userName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "password";
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
			password = JSONUtils::asString(requestBodyRoot, field, "");

			field = "playURL";
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
			playURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifyRTMPChannelConf(
			confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, rtmpURL, streamName, userName, password, playURL, type
		);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeRTMPChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeRTMPChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeRTMPChannelConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::rtmpChannelConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "rtmpChannelConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		string label = getQueryParameter(queryParameters, "label", string(), false);
		bool labelLike = getQueryParameter(queryParameters, "labelLike", false, false);

		int type = 0; // ALL
		string sType = getQueryParameter(queryParameters, "type", string(), false);
		if (sType != "")
		{
			if (sType == "SHARED")
				type = 1;
			else if (sType == "DEDICATED")
				type = 2;
		}

		{
			json rtmpChannelConfListRoot = _mmsEngineDBFacade->getRTMPChannelConfList(apiAuthorizationDetails->workspace->_workspaceKey, -1, label, labelLike, type);

			string responseBody = JSONUtils::toString(rtmpChannelConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addSRTChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addSRTChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string srtURL;
		string mode;
		string streamId;
		string passphrase;
		string playURL;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "srtURL";
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
			srtURL = JSONUtils::asString(requestBodyRoot, field, "");

			mode = JSONUtils::asString(requestBodyRoot, "mode", "");

			streamId = JSONUtils::asString(requestBodyRoot, "streamId", "");

			passphrase = JSONUtils::asString(requestBodyRoot, "passphrase", "");

			field = "playURL";
			playURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey =
				_mmsEngineDBFacade->addSRTChannelConf(apiAuthorizationDetails->workspace->_workspaceKey, label, srtURL, mode, streamId, passphrase, playURL, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addSRTChannelConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifySRTChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifySRTChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string srtURL;
		string mode;
		string streamId;
		string passphrase;
		string playURL;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "srtURL";
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
			srtURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "mode";
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
			mode = JSONUtils::asString(requestBodyRoot, field, "");

			field = "streamId";
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
			streamId = JSONUtils::asString(requestBodyRoot, field, "");

			field = "passphrase";
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
			passphrase = JSONUtils::asString(requestBodyRoot, field, "");

			field = "playURL";
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
			playURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifySRTChannelConf(confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, srtURL, mode, streamId, passphrase, playURL, type);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeSRTChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeSRTChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeSRTChannelConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::srtChannelConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "srtChannelConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		string label = getQueryParameter(queryParameters, "label", string(), false);
		bool labelLike = getQueryParameter(queryParameters, "labelLike", false, false);

		int type = 0; // ALL
		string sType = getQueryParameter(queryParameters, "type", string(), false);
		if (sType != "")
		{
			if (sType == "SHARED")
				type = 1;
			else if (sType == "DEDICATED")
				type = 2;
		}

		{
			json srtChannelConfListRoot = _mmsEngineDBFacade->getSRTChannelConfList(apiAuthorizationDetails->workspace->_workspaceKey, -1, label, labelLike, type);

			string responseBody = JSONUtils::toString(srtChannelConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addHLSChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addHLSChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		int64_t deliveryCode;
		int segmentDuration;
		int playlistEntriesNumber;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "deliveryCode";
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
			deliveryCode = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "segmentDuration";
			segmentDuration = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "playlistEntriesNumber";
			playlistEntriesNumber = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey =
				_mmsEngineDBFacade->addHLSChannelConf(apiAuthorizationDetails->workspace->_workspaceKey, label, deliveryCode, segmentDuration, playlistEntriesNumber, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addHLSChannelConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyHLSChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyHLSChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		int64_t deliveryCode;
		int segmentDuration;
		int playlistEntriesNumber;
		string type;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "deliveryCode";
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
			deliveryCode = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "segmentDuration";
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
			segmentDuration = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "playlistEntriesNumber";
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
			playlistEntriesNumber = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "type";
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
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifyHLSChannelConf(
			confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, deliveryCode, segmentDuration, playlistEntriesNumber, type
		);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeHLSChannelConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeHLSChannelConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeHLSChannelConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::hlsChannelConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "hlsChannelConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		string label = getQueryParameter(queryParameters, "label", string(), false);
		bool labelLike = getQueryParameter(queryParameters, "labelLike", false, false);

		int type = 0; // ALL
		string sType = getQueryParameter(queryParameters, "type", string(), false);
		if (sType != "")
		{
			if (sType == "SHARED")
				type = 1;
			else if (sType == "DEDICATED")
				type = 2;
		}

		{
			json hlsChannelConfListRoot = _mmsEngineDBFacade->getHLSChannelConfList(apiAuthorizationDetails->workspace->_workspaceKey, -1, label, labelLike, type);

			string responseBody = JSONUtils::toString(hlsChannelConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addFTPConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addFTPConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

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
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Server";
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
			server = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Port";
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
			port = JSONUtils::asInt(requestBodyRoot, field, 0);

			field = "UserName";
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
			userName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Password";
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
			password = JSONUtils::asString(requestBodyRoot, field, "");

			field = "RemoteDirectory";
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
			remoteDirectory = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addFTPConf(apiAuthorizationDetails->workspace->_workspaceKey, label, server, port, userName, password, remoteDirectory);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addFTPConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyFTPConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyFTPConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

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
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Server";
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
			server = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Port";
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
			port = JSONUtils::asInt(requestBodyRoot, field, 0);

			field = "UserName";
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
			userName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Password";
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
			password = JSONUtils::asString(requestBodyRoot, field, "");

			field = "RemoteDirectory";
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
			remoteDirectory = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifyFTPConf(confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, server, port, userName, password, remoteDirectory);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeFTPConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeFTPConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeFTPConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::ftpConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters)
{
	string api = "ftpConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		{

			json ftpConfListRoot = _mmsEngineDBFacade->getFTPConfList(apiAuthorizationDetails->workspace->_workspaceKey);

			string responseBody = JSONUtils::toString(ftpConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::addEMailConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addEMailConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string addresses;
		string subject;
		string message;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Addresses";
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
			addresses = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Subject";
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
			subject = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Message";
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
			message = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addEMailConf(apiAuthorizationDetails->workspace->_workspaceKey, label, addresses, subject, message);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"_mmsEngineDBFacade->addEMailConf failed"
				", e.what(): {}",
				e.what()
			);

			throw;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
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
		throw;
	}
}

void API::modifyEMailConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "modifyEMailConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		string label;
		string addresses;
		string subject;
		string message;

		try
		{
			json requestBodyRoot = JSONUtils::toJson(requestBody);

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
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Addresses";
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
			addresses = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Subject";
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
			subject = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Message";
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
			message = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (exception &e)
		{
			string errorMessage = std::format("requestBody json is not well format"
				", requestBody: {}"
				", e.what(): {}", requestBody, e.what());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->modifyEMailConf(confKey, apiAuthorizationDetails->workspace->_workspaceKey, label, addresses, subject, message);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
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
		throw;
	}
}

void API::removeEMailConf(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "removeEMailConf";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditConfiguration)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditConfiguration: {}",
			apiAuthorizationDetails->canEditConfiguration
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		const int64_t confKey = getMapParameter(queryParameters, "confKey", static_cast<int64_t>(-1), true);

		_mmsEngineDBFacade->removeEMailConf(apiAuthorizationDetails->workspace->_workspaceKey, confKey);

		string sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}

void API::emailConfList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters)
{
	string api = "emailConfList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey
	);

	try
	{
		{

			json emailConfListRoot = _mmsEngineDBFacade->getEMailConfList(apiAuthorizationDetails->workspace->_workspaceKey);

			string responseBody = JSONUtils::toString(emailConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", e.what(): {}",
			api, e.what()
		);
		throw;
	}
}