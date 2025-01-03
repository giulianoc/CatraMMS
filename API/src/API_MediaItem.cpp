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

#include "API.h"

void API::updateMediaItem(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace, int64_t userKey,
	unordered_map<string, string> queryParameters, string requestBody, bool admin
)
{
	string api = "updateMediaItem";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		int64_t mediaItemKey = -1;
		auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
		if (mediaItemKeyIt == queryParameters.end() || mediaItemKeyIt->second == "")
		{
			string errorMessage = string("'mediaItemKey' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		mediaItemKey = stoll(mediaItemKeyIt->second);

		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson(requestBody);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + e.what());

			sendError(request, 400, e.what());

			throw runtime_error(e.what());
		}

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
			_logger->info(
				__FILEREF__ + "Updating MediaItem" + ", userKey: " + to_string(userKey) + ", workspaceKey: " + to_string(workspace->_workspaceKey)
			);

			json mediaItemRoot = _mmsEngineDBFacade->updateMediaItem(
				workspace->_workspaceKey, mediaItemKey, titleModified, newTitle, userDataModified, newUserData, retentionInMinutesModified,
				newRetentionInMinutes, tagsModified, newTagsRoot, uniqueNameModified, newUniqueName, crossReferencesRoot, admin
			);

			_logger->info(
				__FILEREF__ + "MediaItem updated" + ", workspaceKey: " + to_string(workspace->_workspaceKey) +
				", mediaItemKey: " + to_string(mediaItemKey)
			);

			string responseBody = JSONUtils::toString(mediaItemRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error: ") + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		throw e;
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

void API::updatePhysicalPath(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace, int64_t userKey,
	unordered_map<string, string> queryParameters, string requestBody, bool admin
)
{
	string api = "updatePhysicalPath";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		int64_t newRetentionInMinutes;

		int64_t mediaItemKey = -1;
		auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
		if (mediaItemKeyIt == queryParameters.end() || mediaItemKeyIt->second == "")
		{
			string errorMessage = string("'mediaItemKey' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		mediaItemKey = stoll(mediaItemKeyIt->second);

		int64_t physicalPathKey = -1;
		auto physicalPathKeyIt = queryParameters.find("physicalPathKey");
		if (physicalPathKeyIt == queryParameters.end() || physicalPathKeyIt->second == "")
		{
			string errorMessage = string("'physicalPathKey' URI parameter is missing");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		physicalPathKey = stoll(physicalPathKeyIt->second);

		json metadataRoot;
		try
		{
			metadataRoot = JSONUtils::toJson(requestBody);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + e.what());

			sendError(request, 400, e.what());

			throw runtime_error(e.what());
		}

		{
			vector<string> mandatoryFields = {"RetentionInMinutes"};
			for (string field : mandatoryFields)
			{
				if (!JSONUtils::isMetadataPresent(metadataRoot, field))
				{
					string errorMessage = string("Json field is not present or it is null") + ", Json field: " + field;
					_logger->error(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			newRetentionInMinutes = JSONUtils::asInt64(metadataRoot, "RetentionInMinutes", 0);
		}

		try
		{
			_logger->info(
				__FILEREF__ + "Updating MediaItem" + ", userKey: " + to_string(userKey) + ", workspaceKey: " + to_string(workspace->_workspaceKey)
			);

			json mediaItemRoot =
				_mmsEngineDBFacade->updatePhysicalPath(workspace->_workspaceKey, mediaItemKey, physicalPathKey, newRetentionInMinutes, admin);

			_logger->info(
				__FILEREF__ + "PhysicalPath updated" + ", workspaceKey: " + to_string(workspace->_workspaceKey) +
				", mediaItemKey: " + to_string(mediaItemKey) + ", physicalPathKey: " + to_string(physicalPathKey)
			);

			string responseBody = JSONUtils::toString(mediaItemRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error: ") + e.what();
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + api + " failed" + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + "API failed" + ", API: " + api + ", requestBody: " + requestBody + ", e.what(): " + e.what());

		throw e;
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

void API::mediaItemsList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody, bool admin
)
{
	string api = "mediaItemsList";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

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

			string errorMessage =
				__FILEREF__ + "rows parameter too big" + ", rows: " + to_string(rows) + ", _maxPageSize: " + to_string(_maxPageSize);
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		string sContentType = getQueryParameter(queryParameters, "contentType", string(), false);
		bool contentTypePresent = false;
		MMSEngineDBFacade::ContentType contentType;
		if (sContentType != "")
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
		if (liveRecordingChunkIt != queryParameters.end() && liveRecordingChunkIt->second != "")
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
				workspace->_workspaceKey, mediaItemKey, uniqueName, physicalPathKey, otherMediaItemsKey, start, rows, contentTypePresent, contentType,
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate, title, liveRecordingChunk, recordingCode, utcCutPeriodStartTimeInMilliSeconds,
				utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, jsonCondition, tagsIn, tagsNotIn, orderBy, jsonOrderBy, responseFields, admin,
				// 2022-12-18: false because from API(get)
				false
			);

			string responseBody = JSONUtils::toString(ingestionStatusRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}

		_logger->info(
			__FILEREF__ + api + ", @API statistics@ - elapsed (seconds): @" +
			to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startAPI).count()) + "@"
		);
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

void API::tagsList(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters, string requestBody
)
{
	string api = "tagsList";

	_logger->info(__FILEREF__ + "Received " + api + ", requestBody: " + requestBody);

	try
	{
		int start = 0;
		auto startIt = queryParameters.find("start");
		if (startIt != queryParameters.end() && startIt->second != "")
		{
			start = stoll(startIt->second);
		}

		int rows = 10;
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

		bool contentTypePresent = false;
		MMSEngineDBFacade::ContentType contentType;
		auto contentTypeIt = queryParameters.find("contentType");
		if (contentTypeIt != queryParameters.end() && contentTypeIt->second != "")
		{
			contentType = MMSEngineDBFacade::toContentType(contentTypeIt->second);

			contentTypePresent = true;
		}

		string tagNameFilter;
		auto tagNameFilterIt = queryParameters.find("tagNameFilter");
		if (tagNameFilterIt != queryParameters.end() && tagNameFilterIt->second != "")
			tagNameFilter = tagNameFilterIt->second;

		/*
		 * liveRecordingChunk:
		 * -1: no condition in select
		 *  0: look for NO liveRecordingChunk (default)
		 *  1: look for liveRecordingChunk
		 */
		int liveRecordingChunk = 0;
		auto liveRecordingChunkIt = queryParameters.find("liveRecordingChunk");
		if (liveRecordingChunkIt != queryParameters.end() && liveRecordingChunkIt->second != "")
		{
			if (liveRecordingChunkIt->second == "true")
				liveRecordingChunk = 1;
			else if (liveRecordingChunkIt->second == "false")
				liveRecordingChunk = 0;
		}

		{
			json tagsRoot = _mmsEngineDBFacade->getTagsList(
				workspace->_workspaceKey, start, rows, liveRecordingChunk, contentTypePresent, contentType, tagNameFilter,
				// 2022-12-18: false because from API(get)
				false
			);

			string responseBody = JSONUtils::toString(tagsRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, "", api, 200, responseBody);
		}
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
