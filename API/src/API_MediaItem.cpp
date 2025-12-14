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
	const FCGIRequestData& requestData
)
{
	string api = "updateMediaItem";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditMedia)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditMedia: {}",
			apiAuthorizationDetails->canEditMedia
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		int64_t mediaItemKey = requestData.getQueryParameter("mediaItemKey", static_cast<int64_t>(-1), true);

		json metadataRoot = JSONUtils::toJson(requestData.requestBody);

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

			sendSuccess(sThreadId, requestIdentifier, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
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
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::updatePhysicalPath(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "updatePhysicalPath";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	if (!apiAuthorizationDetails->admin && !apiAuthorizationDetails->canEditMedia)
	{
		string errorMessage = std::format(
			"APIKey does not have the permission"
			", canEditMedia: {}",
			apiAuthorizationDetails->canEditMedia
		);
		SPDLOG_ERROR(errorMessage);
		throw FCGIRequestData::HTTPError(403);
	}

	try
	{
		int64_t newRetentionInMinutes;

		int64_t mediaItemKey = requestData.getQueryParameter("mediaItemKey", static_cast<int64_t>(-1), true);
		int64_t physicalPathKey = requestData.getQueryParameter("physicalPathKey", static_cast<int64_t>(-1), true);

		json metadataRoot = JSONUtils::toJson(requestData.requestBody);

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

			sendSuccess(sThreadId, requestIdentifier, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (exception &e)
		{
			string errorMessage = std::format(
				"API failed"
				", API: {}"
				", requestData.requestBody: {}"
				", e.what(): {}",
				api, requestData.requestBody, e.what()
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
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::mediaItemsList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "mediaItemsList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		chrono::system_clock::time_point startAPI = chrono::system_clock::now();

		int64_t mediaItemKey = requestData.getQueryParameter("mediaItemKey", static_cast<int64_t>(-1), false);
		// client could send 0 (see CatraMMSAPI::getMEdiaItem) in case it does not have mediaItemKey
		// but other parameters
		if (mediaItemKey == 0)
			mediaItemKey = -1;

		string uniqueName = requestData.getQueryParameter("uniqueName", string(), false);

		int64_t physicalPathKey = requestData.getQueryParameter("physicalPathKey", static_cast<int64_t>(-1), false);
		if (physicalPathKey == 0)
			physicalPathKey = -1;

		int32_t start = requestData.getQueryParameter("start", static_cast<int32_t>(0), false);

		int32_t rows = requestData.getQueryParameter("rows", static_cast<int32_t>(10), false);
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

		string sContentType = requestData.getQueryParameter("contentType", string(), false);
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
		int32_t liveRecordingChunk = requestData.getQueryParameter("liveRecordingChunk", false) == false ? 0 : 1;

		string startIngestionDate = requestData.getQueryParameter("startIngestionDate", string(), false);

		string endIngestionDate = requestData.getQueryParameter("endIngestionDate", string(), false);

		string title = requestData.getQueryParameter("title", string(), false);

		vector<string> tagsIn = requestData.getQueryParameter("tagsIn", ',', vector<string>(), false);
		vector<string> tagsNotIn = requestData.getQueryParameter("tagsNotIn", ',', vector<string>(), false);
		vector<int64_t> otherMediaItemsKey = requestData.getQueryParameter("otherMIKs", ',', vector<int64_t>(), false);
		set<string> responseFields = requestData.getQueryParameter("responseFields", ',', set<string>(), false);

		int64_t recordingCode = requestData.getQueryParameter("recordingCode", static_cast<int64_t>(-1), false);

		string jsonCondition = requestData.getQueryParameter("jsonCondition", string(), false);

		string orderBy = requestData.getQueryParameter("orderBy", string(), false);

		string jsonOrderBy = requestData.getQueryParameter("jsonOrderBy", string(), false);

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

			sendSuccess(sThreadId, requestIdentifier, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
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
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}

void API::tagsList(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "tagsList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	SPDLOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		int32_t start = requestData.getQueryParameter("start", static_cast<int32_t>(0));
		int32_t rows = requestData.getQueryParameter("rows", static_cast<int32_t>(10));
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

		optional<MMSEngineDBFacade::ContentType> contentType;
		optional<string> sContentType = requestData.getOptQueryParameter<string>("contentType");
		if (sContentType)
			contentType = MMSEngineDBFacade::toContentType(*sContentType);

		string tagNameFilter = requestData.getQueryParameter("tagNameFilter", "");

		/*
		 * liveRecordingChunk:
		 * -1: no condition in select
		 *  0: look for NO liveRecordingChunk (default)
		 *  1: look for liveRecordingChunk
		 */
		int32_t liveRecordingChunk = requestData.getQueryParameter("liveRecordingChunk", false) == false ? 0 : 1;

		{
			json tagsRoot = _mmsEngineDBFacade->getTagsList(
				apiAuthorizationDetails->workspace->_workspaceKey, start, rows, liveRecordingChunk, contentType, tagNameFilter,
				// 2022-12-18: false because from API(get)
				false
			);

			string responseBody = JSONUtils::toString(tagsRoot);

			sendSuccess(sThreadId, requestIdentifier, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		string errorMessage = std::format(
			"API failed"
			", API: {}"
			", requestData.requestBody: {}"
			", e.what(): {}",
			api, requestData.requestBody, e.what()
		);
		SPDLOG_ERROR(errorMessage);
		throw;
	}
}
