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

#include "API.h"
#include "spdlog/spdlog.h"

void API::updateMediaItem(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "updateMediaItem";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditMedia)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditMedia: {}",
			apiAuthorizationDetails->canEditMedia
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		int64_t mediaItemKey = -1;
		auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
		if (mediaItemKeyIt == queryParameters.end() || mediaItemKeyIt->second.empty())
		{
			string errorMessage = "'mediaItemKey' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		mediaItemKey = stoll(mediaItemKeyIt->second);

		json metadataRoot = JSONUtils::toJson(requestBody);

		bool titleModified = false;
		string newTitle;
		bool userDataModified = false;
		string newUserData;
		bool retentionInMinutesModified = false;
		int64_t newRetentionInMinutes;
		bool tagsModified = false;
		json newTagsRoot;
		bool uniqueNameModified = false;
		string newUniqueName;
		json crossReferencesRoot = nullptr;

		{
			string field = "title";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				titleModified = true;
				newTitle = JSONUtils::asString(metadataRoot, "title", "");
			}

			field = "userData";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				userDataModified = true;
				newUserData = JSONUtils::asString(metadataRoot, "userData", "");
			}

			field = "RetentionInMinutes";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				retentionInMinutesModified = true;
				newRetentionInMinutes = JSONUtils::asInt64(metadataRoot, "RetentionInMinutes", 0);
			}

			field = "tags";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				tagsModified = true;
				newTagsRoot = metadataRoot[field];
			}

			field = "uniqueName";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				uniqueNameModified = true;
				newUniqueName = JSONUtils::asString(metadataRoot, field, "");
			}

			field = "crossReferences";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
				crossReferencesRoot = metadataRoot[field];
		}

		try
		{
			SPDLOG_INFO(
				"Updating MediaItem"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			json mediaItemRoot = _mmsEngineDBFacade->updateMediaItem(
				apiAuthorizationDetails->workspace->_workspaceKey, mediaItemKey, titleModified, newTitle, userDataModified, newUserData, retentionInMinutesModified,
				newRetentionInMinutes, tagsModified, newTagsRoot, uniqueNameModified, newUniqueName, crossReferencesRoot, apiAuthorizationDetails->admin
			);

			SPDLOG_INFO(
				"MediaItem updated"
				", workspaceKey: {}"
				", mediaItemKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, mediaItemKey
			);

			string responseBody = JSONUtils::toString(mediaItemRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestBody: {}"
				", e.what(): {}",
				api, requestBody, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(500);
	}
}

void API::updatePhysicalPath(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "updatePhysicalPath";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditMedia)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditMedia: {}",
			apiAuthorizationDetails->canEditMedia
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(403);
	}

	try
	{
		int64_t newRetentionInMinutes;

		int64_t mediaItemKey = -1;
		auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
		if (mediaItemKeyIt == queryParameters.end() || mediaItemKeyIt->second.empty())
		{
			string errorMessage = "'mediaItemKey' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		mediaItemKey = stoll(mediaItemKeyIt->second);

		int64_t physicalPathKey = -1;
		auto physicalPathKeyIt = queryParameters.find("physicalPathKey");
		if (physicalPathKeyIt == queryParameters.end() || physicalPathKeyIt->second.empty())
		{
			string errorMessage = "'physicalPathKey' URI parameter is missing";
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		physicalPathKey = stoll(physicalPathKeyIt->second);

		json metadataRoot = JSONUtils::toJson(requestBody);

		{
			vector<string> mandatoryFields = {"RetentionInMinutes"};
			for (string field : mandatoryFields)
			{
				if (!JSONUtils::isMetadataPresent(metadataRoot, field))
				{
					string errorMessage = std::format(
						"Json field is not present or it is null"
						", Json field: {}",
						field
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			newRetentionInMinutes = JSONUtils::asInt64(metadataRoot, "RetentionInMinutes", 0);
		}

		try
		{
			SPDLOG_INFO(
				"Updating MediaItem"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			json mediaItemRoot =
				_mmsEngineDBFacade->updatePhysicalPath(apiAuthorizationDetails->workspace->_workspaceKey, mediaItemKey, physicalPathKey, newRetentionInMinutes, apiAuthorizationDetails->admin);

			SPDLOG_INFO(
				"PhysicalPath updated"
				", workspaceKey: {}"
				", mediaItemKey: {}"
				", physicalPathKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, mediaItemKey, physicalPathKey
			);

			string responseBody = JSONUtils::toString(mediaItemRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestBody: {}"
				", e.what(): {}",
				api, requestBody, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(500);
	}
}

void API::mediaItemsList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "mediaItemsList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	try
	{
		chrono::system_clock::time_point startAPI = chrono::system_clock::now();

		int64_t mediaItemKey = getQueryParameter(queryParameters, "mediaItemKey", static_cast<int64_t>(-1), false);
		// client could send 0 (see CatraMMSAPI::getMEdiaItem) in case it does not have mediaItemKey
		// but other parameters
		if (mediaItemKey == 0)
			mediaItemKey = -1;

		string uniqueName = getQueryParameter(queryParameters, "uniqueName", string(), false);

		int64_t physicalPathKey = getQueryParameter(queryParameters, "physicalPathKey", static_cast<int64_t>(-1), false);
		if (physicalPathKey == 0)
			physicalPathKey = -1;

		int32_t start = getQueryParameter(queryParameters, "start", static_cast<int32_t>(0), false);

		int32_t rows = getQueryParameter(queryParameters, "rows", static_cast<int32_t>(10), false);
		if (rows > _maxPageSize)
		{
			// 2022-02-13: changed to return an error otherwise the user
			//	think to ask for a huge number of items while the return is much less

			// rows = _maxPageSize;

			string errorMessage = std::format(
				"rows parameter too big"
				", rows: {}"
				", _maxPageSize: {}",
				rows, _maxPageSize
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sContentType = getQueryParameter(queryParameters, "contentType", string(), false);
		bool contentTypePresent = false;
		MMSEngineDBFacade::ContentType contentType;
		if (!sContentType.empty())
		{
			contentType = MMSEngineDBFacade::toContentType(sContentType);
			contentTypePresent = true;
		}

		/*
		 * liveRecordingChunk:
		 * -1: no condition in select
		 *  0: look for NO liveRecordingChunk (default)
		 *  1: look for liveRecordingChunk
		 */
		int liveRecordingChunk = 0;
		auto liveRecordingChunkIt = queryParameters.find("liveRecordingChunk");
		if (liveRecordingChunkIt != queryParameters.end() && !liveRecordingChunkIt->second.empty())
		{
			if (liveRecordingChunkIt->second == "true")
				liveRecordingChunk = 1;
			else if (liveRecordingChunkIt->second == "false")
				liveRecordingChunk = 0;
		}

		string startIngestionDate = getQueryParameter(queryParameters, "startIngestionDate", string(), false);

		string endIngestionDate = getQueryParameter(queryParameters, "endIngestionDate", string(), false);

		string title = getQueryParameter(queryParameters, "title", string(), false);

		vector<string> tagsIn = getQueryParameter(queryParameters, "tagsIn", ',', vector<string>(), false);
		vector<string> tagsNotIn = getQueryParameter(queryParameters, "tagsNotIn", ',', vector<string>(), false);
		vector<int64_t> otherMediaItemsKey = getQueryParameter(queryParameters, "otherMIKs", ',', vector<int64_t>(), false);
		set<string> responseFields = getQueryParameter(queryParameters, "responseFields", ',', set<string>(), false);

		int64_t recordingCode = getQueryParameter(queryParameters, "recordingCode", static_cast<int64_t>(-1), false);

		string jsonCondition = getQueryParameter(queryParameters, "jsonCondition", string(), false);

		string orderBy = getQueryParameter(queryParameters, "orderBy", string(), false);

		string jsonOrderBy = getQueryParameter(queryParameters, "jsonOrderBy", string(), false);

		{
			int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
			int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;

			json ingestionStatusRoot = _mmsEngineDBFacade->getMediaItemsList(
				apiAuthorizationDetails->workspace->_workspaceKey, mediaItemKey, uniqueName, physicalPathKey, otherMediaItemsKey, start, rows, contentTypePresent, contentType,
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate, title, liveRecordingChunk, recordingCode, utcCutPeriodStartTimeInMilliSeconds,
				utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, jsonCondition, tagsIn, tagsNotIn, orderBy, jsonOrderBy, responseFields, apiAuthorizationDetails->admin,
				// 2022-12-18: false because from API(get)
				false
			);

			string responseBody = JSONUtils::toString(ingestionStatusRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}

		SPDLOG_INFO(
			"{}, @API statistics@ - elapsed (seconds): @{}@", api,
			chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startAPI).count()
		);
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(500);
	}
}

void API::tagsList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody,
	bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "tagsList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestBody
	);

	try
	{
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && !startIt->second.empty())
		{
			start = stoll(startIt->second);
		}

		int rows = 10;
		auto rowsIt = queryParameters.find("rows");
		if (rowsIt != queryParameters.end() && !rowsIt->second.empty())
		{
			rows = stoll(rowsIt->second);
			if (rows > _maxPageSize)
			{
				// 2022-02-13: changed to return an error otherwise the user
				//	think to ask for a huge number of items while the return is much less

				// rows = _maxPageSize;

				string errorMessage = std::format(
					"rows parameter too big"
					", rows: {}"
					", _maxPageSize: {}",
					rows, _maxPageSize
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		bool contentTypePresent = false;
		MMSEngineDBFacade::ContentType contentType;
		auto contentTypeIt = queryParameters.find("contentType");
		if (contentTypeIt != queryParameters.end() && !contentTypeIt->second.empty())
		{
			contentType = MMSEngineDBFacade::toContentType(contentTypeIt->second);

			contentTypePresent = true;
		}

		string tagNameFilter;
		auto tagNameFilterIt = queryParameters.find("tagNameFilter");
		if (tagNameFilterIt != queryParameters.end() && !tagNameFilterIt->second.empty())
			tagNameFilter = tagNameFilterIt->second;

		/*
		 * liveRecordingChunk:
		 * -1: no condition in select
		 *  0: look for NO liveRecordingChunk (default)
		 *  1: look for liveRecordingChunk
		 */
		int liveRecordingChunk = 0;
		auto liveRecordingChunkIt = queryParameters.find("liveRecordingChunk");
		if (liveRecordingChunkIt != queryParameters.end() && !liveRecordingChunkIt->second.empty())
		{
			if (liveRecordingChunkIt->second == "true")
				liveRecordingChunk = 1;
			else if (liveRecordingChunkIt->second == "false")
				liveRecordingChunk = 0;
		}

		{
			json tagsRoot = _mmsEngineDBFacade->getTagsList(
				apiAuthorizationDetails->workspace->_workspaceKey, start, rows, liveRecordingChunk, contentTypePresent, contentType, tagNameFilter,
				// 2022-12-18: false because from API(get)
				false
			);

			string responseBody = JSONUtils::toString(tagsRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw HTTPError(500);
	}
}
