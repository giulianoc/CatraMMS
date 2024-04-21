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
#include "JSONUtils.h"
#include "Validator.h"
#include "catralibraries/StringUtils.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <regex>

void API::addYouTubeConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addYouTubeConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "tokenType";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			tokenType = JSONUtils::asString(requestBodyRoot, field, "");

			if (tokenType == "RefreshToken")
			{
				field = "refreshToken";
				if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				refreshToken = JSONUtils::asString(requestBodyRoot, field, "");
			}
			else // if (tokenType == "AccessToken")
			{
				field = "accessToken";
				if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				accessToken = JSONUtils::asString(requestBodyRoot, field, "");
			}
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			Validator validator(_logger, _mmsEngineDBFacade, _configurationRoot);
			if (!validator.isYouTubeTokenTypeValid(tokenType))
			{
				string errorMessage = string("The 'tokenType' is not valid") + ", tokenType: " + tokenType;
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

			json youTubeRoot = _mmsEngineDBFacade->addYouTubeConf(workspace->_workspaceKey, label, tokenType, refreshToken, accessToken);

			sResponse = JSONUtils::toString(youTubeRoot);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addYouTubeConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addYouTubeConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyYouTubeConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyYouTubeConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
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
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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
				Validator validator(_logger, _mmsEngineDBFacade, _configurationRoot);
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

			json youTubeRoot = _mmsEngineDBFacade->modifyYouTubeConf(
				confKey, workspace->_workspaceKey, label, labelModified, tokenType, tokenTypeModified, refreshToken, refreshTokenModified,
				accessToken, accessTokenModified
			);

			sResponse = JSONUtils::toString(youTubeRoot);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyYouTubeConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyYouTubeConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeYouTubeConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeYouTubeConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeYouTubeConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeYouTubeConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeYouTubeConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::youTubeConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "youTubeConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
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

		{
			json youTubeConfListRoot = _mmsEngineDBFacade->getYouTubeConfList(workspace->_workspaceKey, label);

			string responseBody = JSONUtils::toString(youTubeConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addFacebookConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addFacebookConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "UserAccessToken";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			userAccessToken = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addFacebookConf(workspace->_workspaceKey, label, userAccessToken);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFacebookConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFacebookConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyFacebookConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyFacebookConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "UserAccessToken";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			userAccessToken = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			_mmsEngineDBFacade->modifyFacebookConf(confKey, workspace->_workspaceKey, label, userAccessToken);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFacebookConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFacebookConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeFacebookConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeFacebookConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeFacebookConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFacebookConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFacebookConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::facebookConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "facebookConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
		int64_t confKey = -1;
		auto confKeyIt = queryParameters.find("confKey");
		if (confKeyIt != queryParameters.end() && confKeyIt->second != "")
		{
			confKey = stoll(confKeyIt->second);
			if (confKey == 0)
				confKey = -1;
		}

		string label;
		if (confKey == -1)
		{
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
		}

		{
			json facebookConfListRoot = _mmsEngineDBFacade->getFacebookConfList(workspace->_workspaceKey, confKey, label);

			string responseBody = JSONUtils::toString(facebookConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addTwitchConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addTwitchConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "RefreshToken";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			refreshToken = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addTwitchConf(workspace->_workspaceKey, label, refreshToken);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addTwitchConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addTwitchConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyTwitchConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyTwitchConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "RefreshToken";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			refreshToken = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			_mmsEngineDBFacade->modifyTwitchConf(confKey, workspace->_workspaceKey, label, refreshToken);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyTwitchConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyTwitchConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeTwitchConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeTwitchConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeTwitchConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeTwitchConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeTwitchConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::twitchConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "twitchConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
		int64_t confKey = -1;
		auto confKeyIt = queryParameters.find("confKey");
		if (confKeyIt != queryParameters.end() && confKeyIt->second != "")
		{
			confKey = stoll(confKeyIt->second);
			if (confKey == 0)
				confKey = -1;
		}

		string label;
		if (confKey == -1)
		{
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
		}

		{
			json twitchConfListRoot = _mmsEngineDBFacade->getTwitchConfList(workspace->_workspaceKey, confKey, label);

			string responseBody = JSONUtils::toString(twitchConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addTiktokConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addTiktokConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Token";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			token = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addTiktokConf(workspace->_workspaceKey, label, token);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addTiktokConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addTiktokConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyTiktokConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyTiktokConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Token";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			token = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			_mmsEngineDBFacade->modifyTiktokConf(confKey, workspace->_workspaceKey, label, token);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyTiktokConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyTiktokConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeTiktokConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeTiktokConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeTiktokConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeTiktokConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeTiktokConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::tiktokConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "tiktokConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
		int64_t confKey = -1;
		auto confKeyIt = queryParameters.find("confKey");
		if (confKeyIt != queryParameters.end() && confKeyIt->second != "")
		{
			confKey = stoll(confKeyIt->second);
			if (confKey == 0)
				confKey = -1;
		}

		string label;
		if (confKey == -1)
		{
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
		}

		{
			json tiktokConfListRoot = _mmsEngineDBFacade->getTiktokConfList(workspace->_workspaceKey, confKey, label);

			string responseBody = JSONUtils::toString(tiktokConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addStream";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "sourceType";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

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
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			json streamRoot = _mmsEngineDBFacade->addStream(
				workspace->_workspaceKey, label, sourceType, encodersPoolKey, url, pushProtocol, pushEncoderKey, pushPublicEncoderName,
				pushServerPort, pushUri, pushListenTimeout, captureLiveVideoDeviceNumber, captureLiveVideoInputFormat, captureLiveFrameRate,
				captureLiveWidth, captureLiveHeight, captureLiveAudioDeviceNumber, captureLiveChannelsNumber, tvSourceTVConfKey, type, description,
				name, region, country, imageMediaItemKey, imageUniqueName, position, userData
			);

			sResponse = JSONUtils::toString(streamRoot);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addStream failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addStream failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyStream";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = -1;
			auto confKeyIt = queryParameters.find("confKey");
			if (confKeyIt != queryParameters.end() && confKeyIt->second != "")
				confKey = stoll(confKeyIt->second);

			string labelKey;
			auto labelIt = queryParameters.find("label");
			if (labelIt != queryParameters.end() && labelIt->second != "")
			{
				labelKey = labelIt->second;

				// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
				//	That  because if we have really a + char (%2B into the string), and we do the replace
				//	after curlpp::unescape, this char will be changed to space and we do not want it
				string plus = "\\+";
				string plusDecoded = " ";
				string firstDecoding = regex_replace(labelKey, regex(plus), plusDecoded);

				labelKey = curlpp::unescape(firstDecoding);
			}

			if (confKey == -1 && labelKey == "")
			{
				string errorMessage = string("The 'confKey/label' parameter is not found");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				throw runtime_error(errorMessage);
			}

			json streamRoot = _mmsEngineDBFacade->modifyStream(
				confKey, labelKey, workspace->_workspaceKey, labelToBeModified, label, sourceTypeToBeModified, sourceType,
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
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyStream failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyStream failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeStream";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

	try
	{
		string sResponse;
		try
		{
			int64_t confKey = -1;
			auto confKeyIt = queryParameters.find("confKey");
			if (confKeyIt != queryParameters.end() && confKeyIt->second != "")
				confKey = stoll(confKeyIt->second);

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

			if (confKey == -1 && label == "")
			{
				string errorMessage = string("The 'confKey/label' parameter is not found");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				throw runtime_error(errorMessage);
			}

			_mmsEngineDBFacade->removeStream(workspace->_workspaceKey, confKey, label);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeStream failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeStream failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::streamList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "streamList";

	_logger->info(__FILEREF__ + "Received " + api);

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

				string errorMessage =
					__FILEREF__ + "rows parameter too big" + ", rows: " + to_string(rows) + ", _maxPageSize: " + to_string(_maxPageSize);
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

		bool labelLike = true;
		auto labelLikeIt = queryParameters.find("labelLike");
		if (labelLikeIt != queryParameters.end() && labelLikeIt->second != "")
		{
			labelLike = (labelLikeIt->second == "true" ? true : false);
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
			if (sourceTypeIt->second == "IP_PULL" || sourceTypeIt->second == "IP_PUSH" || sourceTypeIt->second == "CaptureLive" ||
				sourceTypeIt->second == "TV")
				sourceType = sourceTypeIt->second;
			else
				_logger->warn(__FILEREF__ + "streamList: 'sourceType' parameter is unknown" + ", sourceType: " + sourceTypeIt->second);
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
				_logger->warn(__FILEREF__ + "liveURLList: 'labelOrder' parameter is unknown" + ", labelOrder: " + labelOrderIt->second);
		}

		{

			json streamListRoot = _mmsEngineDBFacade->getStreamList(
				workspace->_workspaceKey, liveURLKey, start, rows, label, labelLike, url, sourceType, type, name, region, country, labelOrder
			);

			string responseBody = JSONUtils::toString(streamListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::streamFreePushEncoderPort(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "streamFreePushEncoderPort";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
		int64_t encoderKey = -1;
		auto encoderKeyIt = queryParameters.find("encoderKey");
		if (encoderKeyIt == queryParameters.end() || encoderKeyIt->second == "")
		{
			string errorMessage = string("The 'encoderKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		encoderKey = stoll(encoderKeyIt->second);

		{
			json streamFreePushEncoderPortRoot = _mmsEngineDBFacade->getStreamFreePushEncoderPort(encoderKey);

			string responseBody = JSONUtils::toString(streamFreePushEncoderPortRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addSourceTVStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addSourceTVStream";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			name = JSONUtils::asString(requestBodyRoot, field, "");

			field = "satellite";
			satellite = JSONUtils::asString(requestBodyRoot, field, "");

			field = "frequency";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

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
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

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
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addSourceTVStream failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addSourceTVStream failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifySourceTVStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifySourceTVStream";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			json sourceTVStreamRoot = _mmsEngineDBFacade->modifySourceTVStream(
				confKey, typeToBeModified, type, serviceIdToBeModified, serviceId, networkIdToBeModified, networkId, transportStreamIdToBeModified,
				transportStreamId, nameToBeModified, name, satelliteToBeModified, satellite, frequencyToBeModified, frequency, lnbToBeModified, lnb,
				videoPidToBeModified, videoPid, audioPidsToBeModified, audioPids, audioItalianPidToBeModified, audioItalianPid,
				audioEnglishPidToBeModified, audioEnglishPid, teletextPidToBeModified, teletextPid, modulationToBeModified, modulation,
				polarizationToBeModified, polarization, symbolRateToBeModified, symbolRate, bandwidthInHzToBeModified, bandwidthInHz,
				countryToBeModified, country, deliverySystemToBeModified, deliverySystem
			);

			sResponse = JSONUtils::toString(sourceTVStreamRoot);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifySourceTVStream failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifySourceTVStream failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeSourceTVStream(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeSourceTVStream";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeSourceTVStream failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeSourceTVStream failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::sourceTVStreamList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "sourceTVStreamList";

	_logger->info(__FILEREF__ + "Received " + api);

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

				string errorMessage =
					__FILEREF__ + "rows parameter too big" + ", rows: " + to_string(rows) + ", _maxPageSize: " + to_string(_maxPageSize);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string type;
		auto typeIt = queryParameters.find("type");
		if (typeIt != queryParameters.end() && typeIt->second != "")
			type = typeIt->second;

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
				_logger->warn(__FILEREF__ + "tvChannelList: 'nameOrder' parameter is unknown" + ", nameOrder: " + nameOrderIt->second);
		}

		{
			json sourceTVStreamRoot = _mmsEngineDBFacade->getSourceTVStreamList(
				confKey, start, rows, type, serviceId, name, frequency, lnb, videoPid, audioPids, nameOrder
			);

			string responseBody = JSONUtils::toString(sourceTVStreamRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addAWSChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addAWSChannelConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
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
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "label";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "channelId";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			channelId = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			rtmpURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "playURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			playURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addAWSChannelConf(workspace->_workspaceKey, label, channelId, rtmpURL, playURL, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addAWSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addAWSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyAWSChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyAWSChannelConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
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
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "label";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "channelId";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			channelId = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			rtmpURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "playURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			playURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			_mmsEngineDBFacade->modifyAWSChannelConf(confKey, workspace->_workspaceKey, label, channelId, rtmpURL, playURL, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyAWSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyAWSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeAWSChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeAWSChannelConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeAWSChannelConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeAWSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeAWSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::awsChannelConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "awsChannelConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
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

		int type = 0; // ALL
		string sType;
		auto typeIt = queryParameters.find("type");
		if (typeIt != queryParameters.end() && typeIt->second != "")
		{
			if (typeIt->second == "SHARED")
				type = 1;
			else if (typeIt->second == "DEDICATED")
				type = 2;
		}
		{
			json awsChannelConfListRoot = _mmsEngineDBFacade->getAWSChannelConfList(workspace->_workspaceKey, -1, label, type);

			string responseBody = JSONUtils::toString(awsChannelConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addCDN77ChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addCDN77ChannelConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

	try
	{
		string label;
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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			rtmpURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "resourceURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			resourceURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "filePath";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			filePath = JSONUtils::asString(requestBodyRoot, field, "");

			field = "secureToken";
			secureToken = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey =
				_mmsEngineDBFacade->addCDN77ChannelConf(workspace->_workspaceKey, label, rtmpURL, resourceURL, filePath, secureToken, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addCDN77ChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addCDN77ChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyCDN77ChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyCDN77ChannelConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

	try
	{
		string label;
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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			rtmpURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "resourceURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			resourceURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "filePath";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			filePath = JSONUtils::asString(requestBodyRoot, field, "");

			field = "secureToken";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			secureToken = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			_mmsEngineDBFacade->modifyCDN77ChannelConf(confKey, workspace->_workspaceKey, label, rtmpURL, resourceURL, filePath, secureToken, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyCDN77ChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyCDN77ChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeCDN77ChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeCDN77ChannelConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeCDN77ChannelConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeCDN77ChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeCDN77ChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::cdn77ChannelConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "cdn77ChannelConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
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

		int type = 0; // ALL
		string sType;
		auto typeIt = queryParameters.find("type");
		if (typeIt != queryParameters.end() && typeIt->second != "")
		{
			if (typeIt->second == "SHARED")
				type = 1;
			else if (typeIt->second == "DEDICATED")
				type = 2;
		}
		{
			json cdn77ChannelConfListRoot = _mmsEngineDBFacade->getCDN77ChannelConfList(workspace->_workspaceKey, -1, label, type);

			string responseBody = JSONUtils::toString(cdn77ChannelConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addRTMPChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addRTMPChannelConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey =
				_mmsEngineDBFacade->addRTMPChannelConf(workspace->_workspaceKey, label, rtmpURL, streamName, userName, password, playURL, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addRTMPChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addRTMPChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyRTMPChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyRTMPChannelConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "rtmpURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			rtmpURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "streamName";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "userName";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			userName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "password";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			password = JSONUtils::asString(requestBodyRoot, field, "");

			field = "playURL";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			playURL = JSONUtils::asString(requestBodyRoot, field, "");

			field = "type";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			_mmsEngineDBFacade->modifyRTMPChannelConf(
				confKey, workspace->_workspaceKey, label, rtmpURL, streamName, userName, password, playURL, type
			);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyRTMPChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyRTMPChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeRTMPChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeRTMPChannelConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeRTMPChannelConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeRTMPChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeRTMPChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::rtmpChannelConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "rtmpChannelConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
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

		int type = 0; // ALL
		string sType;
		auto typeIt = queryParameters.find("type");
		if (typeIt != queryParameters.end() && typeIt->second != "")
		{
			if (typeIt->second == "SHARED")
				type = 1;
			else if (typeIt->second == "DEDICATED")
				type = 2;
		}

		{
			json rtmpChannelConfListRoot = _mmsEngineDBFacade->getRTMPChannelConfList(workspace->_workspaceKey, -1, label, type);

			string responseBody = JSONUtils::toString(rtmpChannelConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addHLSChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addHLSChannelConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "deliveryCode";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey =
				_mmsEngineDBFacade->addHLSChannelConf(workspace->_workspaceKey, label, deliveryCode, segmentDuration, playlistEntriesNumber, type);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addHLSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addHLSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyHLSChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyHLSChannelConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "deliveryCode";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			deliveryCode = JSONUtils::asInt64(requestBodyRoot, field, -1);

			field = "segmentDuration";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			segmentDuration = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "playlistEntriesNumber";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			playlistEntriesNumber = JSONUtils::asInt(requestBodyRoot, field, -1);

			field = "type";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			type = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			_mmsEngineDBFacade->modifyHLSChannelConf(
				confKey, workspace->_workspaceKey, label, deliveryCode, segmentDuration, playlistEntriesNumber, type
			);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyHLSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyHLSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeHLSChannelConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeHLSChannelConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeHLSChannelConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeHLSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeHLSChannelConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::hlsChannelConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "hlsChannelConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
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

		int type = 0; // ALL
		string sType;
		auto typeIt = queryParameters.find("type");
		if (typeIt != queryParameters.end() && typeIt->second != "")
		{
			if (typeIt->second == "SHARED")
				type = 1;
			else if (typeIt->second == "DEDICATED")
				type = 2;
		}

		{
			json hlsChannelConfListRoot = _mmsEngineDBFacade->getHLSChannelConfList(workspace->_workspaceKey, -1, label, type);

			string responseBody = JSONUtils::toString(hlsChannelConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addFTPConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addFTPConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
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
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "label";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Server";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			server = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Port";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			port = JSONUtils::asInt(requestBodyRoot, field, 0);

			field = "UserName";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			userName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Password";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			password = JSONUtils::asString(requestBodyRoot, field, "");

			field = "RemoteDirectory";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			remoteDirectory = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addFTPConf(workspace->_workspaceKey, label, server, port, userName, password, remoteDirectory);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFTPConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addFTPConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyFTPConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyFTPConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
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
			json requestBodyRoot = JSONUtils::toJson(requestBody);

			string field = "label";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Server";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			server = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Port";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			port = JSONUtils::asInt(requestBodyRoot, field, 0);

			field = "UserName";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			userName = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Password";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			password = JSONUtils::asString(requestBodyRoot, field, "");

			field = "RemoteDirectory";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			remoteDirectory = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			_mmsEngineDBFacade->modifyFTPConf(confKey, workspace->_workspaceKey, label, server, port, userName, password, remoteDirectory);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFTPConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyFTPConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeFTPConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeFTPConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeFTPConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFTPConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeFTPConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::ftpConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace
)
{
	string api = "ftpConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
		{

			json ftpConfListRoot = _mmsEngineDBFacade->getFTPConfList(workspace->_workspaceKey);

			string responseBody = JSONUtils::toString(ftpConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::addEMailConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "addEMailConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Addresses";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			addresses = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Subject";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			subject = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Message";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			message = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string sResponse;
		try
		{
			int64_t confKey = _mmsEngineDBFacade->addEMailConf(workspace->_workspaceKey, label, addresses, subject, message);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEMailConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->addEMailConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 201, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::modifyEMailConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "modifyEMailConf";

	_logger->info(
		__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", requestBody: " + requestBody
	);

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			label = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Addresses";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			addresses = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Subject";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			subject = JSONUtils::asString(requestBodyRoot, field, "");

			field = "Message";
			if (!JSONUtils::isMetadataPresent(requestBodyRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			message = JSONUtils::asString(requestBodyRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody + ", e.what(): " + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("requestBody json is not well format") + ", requestBody: " + requestBody;
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

			_mmsEngineDBFacade->modifyEMailConf(confKey, workspace->_workspaceKey, label, addresses, subject, message);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEMailConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->modifyEMailConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::removeEMailConf(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters
)
{
	string api = "removeEMailConf";

	_logger->info(__FILEREF__ + "Received " + api + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey));

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

			_mmsEngineDBFacade->removeEMailConf(workspace->_workspaceKey, confKey);

			sResponse = (string("{ ") + "\"confKey\": " + to_string(confKey) + "}");
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEMailConf failed" + ", e.what(): " + e.what());

			throw e;
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "_mmsEngineDBFacade->removeEMailConf failed" + ", e.what(): " + e.what());

			throw e;
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, sResponse);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}

void API::emailConfList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace
)
{
	string api = "emailConfList";

	_logger->info(__FILEREF__ + "Received " + api);

	try
	{
		{

			json emailConfListRoot = _mmsEngineDBFacade->getEMailConfList(workspace->_workspaceKey);

			string responseBody = JSONUtils::toString(emailConfListRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error: ") + e.what();
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", e.what(): " + e.what());

		string errorMessage = string("Internal server error");
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
}
