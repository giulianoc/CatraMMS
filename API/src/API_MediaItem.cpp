/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   API.cpp
 *
 * Created on February 18, 2018, 1:27 AM
 */

#include "JSONUtils.h"

#include "API.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;

void API::updateMediaItem(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "updateMediaItem";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
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
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		int64_t mediaItemKey = requestData.getQueryParameter("mediaItemKey", static_cast<int64_t>(-1), true);

		json metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);

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
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				titleModified = true;
				newTitle = JSONUtils::as<string>(metadataRoot, "title", "");
			}

			field = "userData";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				userDataModified = true;
				newUserData = JSONUtils::as<string>(metadataRoot, "userData", "");
			}

			field = "RetentionInMinutes";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				retentionInMinutesModified = true;
				newRetentionInMinutes = JSONUtils::as<int64_t>(metadataRoot, "RetentionInMinutes", 0);
			}

			field = "tags";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				tagsModified = true;
				newTagsRoot = metadataRoot[field];
			}

			field = "uniqueName";
			if (JSONUtils::isPresent(metadataRoot, field))
			{
				uniqueNameModified = true;
				newUniqueName = JSONUtils::as<string>(metadataRoot, field, "");
			}

			field = "crossReferences";
			if (JSONUtils::isPresent(metadataRoot, field))
				crossReferencesRoot = metadataRoot[field];
		}

		try
		{
			LOG_INFO(
				"Updating MediaItem"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			json mediaItemRoot = _mmsEngineDBFacade->updateMediaItem(
				apiAuthorizationDetails->workspace->_workspaceKey, mediaItemKey, titleModified, newTitle, userDataModified, newUserData, retentionInMinutesModified,
				newRetentionInMinutes, tagsModified, newTagsRoot, uniqueNameModified, newUniqueName, crossReferencesRoot, apiAuthorizationDetails->admin
			);

			LOG_INFO(
				"MediaItem updated"
				", workspaceKey: {}"
				", mediaItemKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, mediaItemKey
			);

			string responseBody = JSONUtils::toString(mediaItemRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
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
			LOG_ERROR(errorMessage);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::updatePhysicalPath(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "updatePhysicalPath";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
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
		LOG_ERROR(errorMessage);
		throw FastCGIError::HTTPError(403);
	}

	try
	{
		int64_t newRetentionInMinutes;

		int64_t mediaItemKey = requestData.getQueryParameter("mediaItemKey", static_cast<int64_t>(-1), true);
		int64_t physicalPathKey = requestData.getQueryParameter("physicalPathKey", static_cast<int64_t>(-1), true);

		json metadataRoot = JSONUtils::toJson<json>(requestData.requestBody);

		{
			vector<string> mandatoryFields = {"RetentionInMinutes"};
			for (string field : mandatoryFields)
			{
				if (!JSONUtils::isPresent(metadataRoot, field))
				{
					string errorMessage = std::format(
						"Json field is not present or it is null"
						", Json field: {}",
						field
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			newRetentionInMinutes = JSONUtils::as<int64_t>(metadataRoot, "RetentionInMinutes", 0);
		}

		try
		{
			LOG_INFO(
				"Updating MediaItem"
				", userKey: {}"
				", workspaceKey: {}",
				apiAuthorizationDetails->userKey, apiAuthorizationDetails->workspace->_workspaceKey
			);

			json mediaItemRoot =
				_mmsEngineDBFacade->updatePhysicalPath(apiAuthorizationDetails->workspace->_workspaceKey, mediaItemKey, physicalPathKey, newRetentionInMinutes, apiAuthorizationDetails->admin);

			LOG_INFO(
				"PhysicalPath updated"
				", workspaceKey: {}"
				", mediaItemKey: {}"
				", physicalPathKey: {}",
				apiAuthorizationDetails->workspace->_workspaceKey, mediaItemKey, physicalPathKey
			);

			string responseBody = JSONUtils::toString(mediaItemRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
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
			LOG_ERROR(errorMessage);

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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::mediaItemsList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "mediaItemsList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
		"Received {}"
		", workspace->_workspaceKey: {}"
		", requestData.requestBody: {}",
		api, apiAuthorizationDetails->workspace->_workspaceKey, requestData.requestBody
	);

	try
	{
		chrono::system_clock::time_point startAPI = chrono::system_clock::now();

		std::optional<int64_t> mediaItemKey = requestData.getOptQueryParameter<int64_t>("mediaItemKey");
		// client could send 0 (see CatraMMSAPI::getMEdiaItem) in case it does not have mediaItemKey
		// but other parameters
		if (mediaItemKey && *mediaItemKey == 0)
			mediaItemKey = nullopt;

		std::optional<int64_t> physicalPathKey = requestData.getOptQueryParameter<int64_t>("physicalPathKey");
		if (physicalPathKey && *physicalPathKey == 0)
			physicalPathKey = nullopt;

		string sContentType = requestData.getQueryParameter("contentType", string(), false);
		std::optional<MMSEngineDBFacade::ContentType> contentType;
		if (!sContentType.empty())
			contentType = MMSEngineDBFacade::toContentType(sContentType);

		MMSEngineDBFacade::MediaItemsListParams mediaItemsListParams {
			.workspaceKey = apiAuthorizationDetails->workspace->_workspaceKey,
			.mediaItemKey = mediaItemKey,
			.uniqueName = requestData.getQueryParameter<string>("uniqueName", string(), false),
			.physicalPathKey = physicalPathKey,
			.otherMediaItemsKey = requestData.getQueryParameter("otherMIKs", ',', vector<int64_t>(),
				false),
			.start = requestData.getQueryParameter("start", 0, false),
			.rows = requestData.getQueryParameter("rows", 10, false),
			.contentType = contentType,
			.startIngestionDate = requestData.getQueryParameter("startIngestionDate", string(), false),
			.endIngestionDate = requestData.getQueryParameter("endIngestionDate", string(), false),
			.title = requestData.getQueryParameter("title", string(), false),
			/*
			 * liveRecordingChunk:
			 * -1 (nullopt): no condition in select
			 *  0 (false): look for NO liveRecordingChunk (default of the API)
			 *  1 (true): look for liveRecordingChunk
			 */
			.liveRecordingChunk = requestData.getQueryParameter("liveRecordingChunk", false),
			.recordingCode = requestData.getOptQueryParameter<int64_t>("recordingCode"),
			.jsonCondition = requestData.getQueryParameter("jsonCondition", string(), false),
			.tagsIn = requestData.getQueryParameter("tagsIn", ',', vector<string>(), false),
			.tagsNotIn = requestData.getQueryParameter("tagsNotIn", ',', vector<string>(), false),
			.orderBy = requestData.getQueryParameter("orderBy", string(), false),
			.jsonOrderBy = requestData.getQueryParameter("jsonOrderBy", string(), false),
			.responseFields = requestData.getQueryParameter("responseFields", ',', set<string>(), false),
			.admin = apiAuthorizationDetails->admin
		};

		if (mediaItemsListParams.rows > _maxPageSize)
		{
			// 2022-02-13: changed to return an error otherwise the user
			//	think to ask for a huge number of items while the return is much less

			// rows = _maxPageSize;

			string errorMessage = std::format(
				"rows parameter too big"
				", rows: {}"
				", _maxPageSize: {}",
				mediaItemsListParams.rows, _maxPageSize
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			json ingestionStatusRoot = _mmsEngineDBFacade->getMediaItemsList(mediaItemsListParams);

			string responseBody = JSONUtils::toString(ingestionStatusRoot);

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
		}

		LOG_INFO(
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
		LOG_ERROR(errorMessage);
		throw;
	}
}

void API::tagsList(
	const string_view& sThreadId, FCGX_Request &request,
	const FCGIRequestData& requestData
)
{
	string api = "tagsList";

	shared_ptr<APIAuthorizationDetails> apiAuthorizationDetails = static_pointer_cast<APIAuthorizationDetails>(requestData.authorizationDetails);

	LOG_INFO(
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
			LOG_ERROR(errorMessage);

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

			sendSuccess(sThreadId, requestData.responseBodyCompressed, request, "", api, 200, responseBody);
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
		LOG_ERROR(errorMessage);
		throw;
	}
}
