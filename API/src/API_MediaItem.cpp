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
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>

#include "API.h"

void API::updateMediaItem(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
		unordered_map<string, string> queryParameters,
        string requestBody,
		bool admin)
{
    string api = "updateMediaItem";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

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

        Json::Value metadataRoot;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &metadataRoot, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = string("Json metadata failed during the parsing")
                        + ", errors: " + errors
                        + ", json data: " + requestBody
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(exception e)
        {
            string errorMessage = string("Json metadata failed during the parsing"
                    ", json data: " + requestBody
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

		bool titleModified = false;
        string newTitle;
		bool userDataModified = false;
		string newUserData;
		bool retentionInMinutesModified = false;
		int64_t newRetentionInMinutes;
		bool tagsModified = false;
		Json::Value newTagsRoot;
		bool uniqueNameModified = false;
		string newUniqueName;

        {
			string field = "Title";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				titleModified = true;
				newTitle = metadataRoot.get("Title", "").asString();
			}

			field = "UserData";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				userDataModified = true;
				newUserData = metadataRoot.get("UserData", "").asString();
			}

			field = "RetentionInMinutes";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				retentionInMinutesModified = true;
				newRetentionInMinutes = JSONUtils::asInt64(metadataRoot, "RetentionInMinutes", 0);
			}

			field = "Tags";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				tagsModified = true;
				newTagsRoot = metadataRoot[field];
			}

			field = "UniqueName";
			if (JSONUtils::isMetadataPresent(metadataRoot, field))
			{
				uniqueNameModified = true;
				newUniqueName = metadataRoot.get(field, "").asString();
			}
        }

        try
        {
            _logger->info(__FILEREF__ + "Updating MediaItem"
                + ", userKey: " + to_string(userKey)
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
            );
            
			Json::Value mediaItemRoot = _mmsEngineDBFacade->updateMediaItem (
				workspace->_workspaceKey,
				mediaItemKey,
				titleModified, newTitle,
				userDataModified, newUserData,
				retentionInMinutesModified, newRetentionInMinutes,
				tagsModified, newTagsRoot,
				uniqueNameModified, newUniqueName,
				admin
			);

            _logger->info(__FILEREF__ + "MediaItem updated"
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
                + ", mediaItemKey: " + to_string(mediaItemKey)
            );
            
            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, mediaItemRoot);
            
            sendSuccess(request, 200, responseBody);            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        throw e;
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

void API::updatePhysicalPath(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
		unordered_map<string, string> queryParameters,
        string requestBody,
		bool admin)
{
    string api = "updatePhysicalPath";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

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

        Json::Value metadataRoot;
        try
        {
            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(requestBody.c_str(),
                    requestBody.c_str() + requestBody.size(), 
                    &metadataRoot, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = string("Json metadata failed during the parsing")
                        + ", errors: " + errors
                        + ", json data: " + requestBody
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                sendError(request, 400, errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch(exception e)
        {
            string errorMessage = string("Json metadata failed during the parsing"
                    ", json data: " + requestBody
                    );
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 400, errorMessage);

            throw runtime_error(errorMessage);
        }

        {
            vector<string> mandatoryFields = {
				"RetentionInMinutes"
            };
            for (string field: mandatoryFields)
            {
                if (!JSONUtils::isMetadataPresent(metadataRoot, field))
                {
                    string errorMessage = string("Json field is not present or it is null")
                            + ", Json field: " + field;
                    _logger->error(__FILEREF__ + errorMessage);

                    sendError(request, 400, errorMessage);

                    throw runtime_error(errorMessage);
                }
            }

            newRetentionInMinutes = JSONUtils::asInt64(metadataRoot, "RetentionInMinutes", 0);
        }

        try
        {
            _logger->info(__FILEREF__ + "Updating MediaItem"
                + ", userKey: " + to_string(userKey)
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
            );
            
			Json::Value mediaItemRoot = _mmsEngineDBFacade->updatePhysicalPath (
				workspace->_workspaceKey,
				mediaItemKey,
				physicalPathKey,
				newRetentionInMinutes,
				admin
			);

            _logger->info(__FILEREF__ + "PhysicalPath updated"
                + ", workspaceKey: " + to_string(workspace->_workspaceKey)
                + ", mediaItemKey: " + to_string(mediaItemKey)
                + ", physicalPathKey: " + to_string(physicalPathKey)
            );
            
            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, mediaItemRoot);
            
            sendSuccess(request, 200, responseBody);            
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error: ") + e.what();
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + api + " failed"
                + ", e.what(): " + e.what()
            );

            string errorMessage = string("Internal server error");
            _logger->error(__FILEREF__ + errorMessage);

            sendError(request, 500, errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "API failed"
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + e.what()
        );

        throw e;
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

void API::mediaItemsList(
	FCGX_Request& request,
	shared_ptr<Workspace> workspace,
	unordered_map<string, string> queryParameters,
	string requestBody,
	bool admin)
{
    string api = "mediaItemsList";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

    try
    {
		chrono::system_clock::time_point startAPI = chrono::system_clock::now();

        int64_t mediaItemKey = -1;
        auto mediaItemKeyIt = queryParameters.find("mediaItemKey");
        if (mediaItemKeyIt != queryParameters.end() && mediaItemKeyIt->second != "")
        {
            mediaItemKey = stoll(mediaItemKeyIt->second);
			// client could send 0 (see CatraMMSAPI::getMEdiaItem) in case it does not have mediaItemKey
			// but other parameters
            if (mediaItemKey == 0)
                mediaItemKey = -1;
        }

        string uniqueName;
        auto uniqueNameIt = queryParameters.find("uniqueName");
        if (uniqueNameIt != queryParameters.end() && uniqueNameIt->second != "")
        {
            uniqueName = uniqueNameIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(uniqueName, regex(plus), plusDecoded);

			uniqueName = curlpp::unescape(firstDecoding);
        }

        int64_t physicalPathKey = -1;
        auto physicalPathKeyIt = queryParameters.find("physicalPathKey");
        if (physicalPathKeyIt != queryParameters.end() && physicalPathKeyIt->second != "")
        {
            physicalPathKey = stoll(physicalPathKeyIt->second);
            if (physicalPathKey == 0)
                physicalPathKey = -1;
        }

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

				string errorMessage = __FILEREF__ + "rows parameter too big"
					+ ", rows: " + to_string(rows)
					+ ", _maxPageSize: " + to_string(_maxPageSize)
				;
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

		/*
        bool startAndEndIngestionDatePresent = false;
        string startIngestionDate;
        string endIngestionDate;
        auto startIngestionDateIt = queryParameters.find("startIngestionDate");
        auto endIngestionDateIt = queryParameters.find("endIngestionDate");
        if (startIngestionDateIt != queryParameters.end()
			|| endIngestionDateIt != queryParameters.end())
        {
			if (startIngestionDateIt != queryParameters.end())
				startIngestionDate = startIngestionDateIt->second;
			else
			{
				tm tmUTCDateTime;
				char sUTCDateTime[64];

				chrono::system_clock::time_point now = chrono::system_clock::now();
				time_t utcNow  = chrono::system_clock::to_time_t(now);

				gmtime_r (&utcNow, &tmUTCDateTime);
				sprintf (sUTCDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
					tmUTCDateTime. tm_year + 1900,
					tmUTCDateTime. tm_mon + 1,
					tmUTCDateTime. tm_mday,
					tmUTCDateTime. tm_hour,
					tmUTCDateTime. tm_min,
					tmUTCDateTime. tm_sec);

				startIngestionDate = sUTCDateTime;
			}

			if (endIngestionDateIt != queryParameters.end())
				endIngestionDate = endIngestionDateIt->second;
			else
			{
				tm tmUTCDateTime;
				char sUTCDateTime[64];

				chrono::system_clock::time_point now = chrono::system_clock::now();
				time_t utcNow  = chrono::system_clock::to_time_t(now);

				gmtime_r (&utcNow, &tmUTCDateTime);
				sprintf (sUTCDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
					tmUTCDateTime. tm_year + 1900,
					tmUTCDateTime. tm_mon + 1,
					tmUTCDateTime. tm_mday,
					tmUTCDateTime. tm_hour,
					tmUTCDateTime. tm_min,
					tmUTCDateTime. tm_sec);

				endIngestionDate = sUTCDateTime;
			}
            
            startAndEndIngestionDatePresent = true;
        }
		*/
        string startIngestionDate;
        auto startIngestionDateIt = queryParameters.find("startIngestionDate");
        if (startIngestionDateIt != queryParameters.end())
			startIngestionDate = startIngestionDateIt->second;

        string endIngestionDate;
        auto endIngestionDateIt = queryParameters.find("endIngestionDate");
        if (endIngestionDateIt != queryParameters.end())
			endIngestionDate = endIngestionDateIt->second;

        string title;
        auto titleIt = queryParameters.find("title");
        if (titleIt != queryParameters.end() && titleIt->second != "")
        {
            title = titleIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(title, regex(plus), plusDecoded);

			title = curlpp::unescape(firstDecoding);
        }

		vector<string> tagsIn;
		vector<string> tagsNotIn;
		vector<int64_t> otherMediaItemsKey;
		if (requestBody != "")
		{
			Json::Value otherInputsRoot;
			try
			{
				Json::CharReaderBuilder builder;
				Json::CharReader* reader = builder.newCharReader();
				string errors;

				bool parsingSuccessful = reader->parse(requestBody.c_str(),
						requestBody.c_str() + requestBody.size(), 
						&otherInputsRoot, &errors);
				delete reader;

				if (!parsingSuccessful)
				{
					string errorMessage = string("Json tags failed during the parsing")
                        + ", errors: " + errors
                        + ", json data: " + requestBody
                        ;
					_logger->error(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch(exception e)
			{
				string errorMessage = string("Json tags failed during the parsing"
                    ", json data: " + requestBody
                    );
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			string field = "tagsIn";
            if (JSONUtils::isMetadataPresent(otherInputsRoot, field))
            {
				Json::Value tagsInRoot = otherInputsRoot[field];

				for (int tagIndex = 0; tagIndex < tagsInRoot.size(); ++tagIndex)
				{
					tagsIn.push_back (tagsInRoot[tagIndex].asString());
				}
			}

			field = "tagsNotIn";
            if (JSONUtils::isMetadataPresent(otherInputsRoot, field))
            {
				Json::Value tagsNotInRoot = otherInputsRoot[field];

				for (int tagIndex = 0; tagIndex < tagsNotInRoot.size(); ++tagIndex)
				{
					tagsNotIn.push_back (tagsNotInRoot[tagIndex].asString());
				}
			}

			field = "otherMediaItemsKey";
            if (JSONUtils::isMetadataPresent(otherInputsRoot, field))
            {
				Json::Value otherMediaItemsKeyRoot = otherInputsRoot[field];

				for (int mediaItemsIndex = 0; mediaItemsIndex < otherMediaItemsKeyRoot.size(); ++mediaItemsIndex)
				{
					otherMediaItemsKey.push_back (JSONUtils::asInt64(otherMediaItemsKeyRoot[mediaItemsIndex]));
				}
			}
		}

        int64_t deliveryCode = -1;
        auto deliveryCodeIt = queryParameters.find("deliveryCode");
        if (deliveryCodeIt != queryParameters.end() && deliveryCodeIt->second != "")
        {
			deliveryCode = stoll(deliveryCodeIt->second);
        }

        string jsonCondition;
        auto jsonConditionIt = queryParameters.find("jsonCondition");
        if (jsonConditionIt != queryParameters.end() && jsonConditionIt->second != "")
        {
            jsonCondition = jsonConditionIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(jsonCondition, regex(plus), plusDecoded);

			jsonCondition = curlpp::unescape(firstDecoding);
        }

		/*
        string ingestionDateOrder;
        auto ingestionDateOrderIt = queryParameters.find("ingestionDateOrder");
        if (ingestionDateOrderIt != queryParameters.end() && ingestionDateOrderIt->second != "")
        {
            if (ingestionDateOrderIt->second == "asc" || ingestionDateOrderIt->second == "desc")
                ingestionDateOrder = ingestionDateOrderIt->second;
            else
                _logger->warn(__FILEREF__ + "mediaItemsList: 'ingestionDateOrder' parameter is unknown"
                    + ", ingestionDateOrder: " + ingestionDateOrderIt->second);
        }
		*/
        string orderBy;
        auto orderByIt = queryParameters.find("orderBy");
        if (orderByIt != queryParameters.end() && orderByIt->second != "")
        {
            orderBy = orderByIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(orderBy, regex(plus), plusDecoded);

			orderBy = curlpp::unescape(firstDecoding);
        }

        string jsonOrderBy;
        auto jsonOrderByIt = queryParameters.find("jsonOrderBy");
        if (jsonOrderByIt != queryParameters.end() && jsonOrderByIt->second != "")
        {
            jsonOrderBy = jsonOrderByIt->second;

			// 2021-01-07: Remark: we have FIRST to replace + in space and then apply curlpp::unescape
			//	That  because if we have really a + char (%2B into the string), and we do the replace
			//	after curlpp::unescape, this char will be changed to space and we do not want it
			string plus = "\\+";
			string plusDecoded = " ";
			string firstDecoding = regex_replace(jsonOrderBy, regex(plus), plusDecoded);

			jsonOrderBy = curlpp::unescape(firstDecoding);
        }

        {
			int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
			int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;

            Json::Value ingestionStatusRoot = _mmsEngineDBFacade->getMediaItemsList(
                    workspace->_workspaceKey, mediaItemKey, uniqueName, physicalPathKey, otherMediaItemsKey,
                    start, rows,
                    contentTypePresent, contentType,
                    // startAndEndIngestionDatePresent,
					startIngestionDate, endIngestionDate,
                    title, liveRecordingChunk,
					deliveryCode,
					utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
					jsonCondition,
					tagsIn, tagsNotIn, orderBy, jsonOrderBy, admin);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, ingestionStatusRoot);
            
            sendSuccess(request, 200, responseBody);
        }

		_logger->info(__FILEREF__ + api
			+ ", @API statistics@ - elapsed (seconds): @" + to_string(
				chrono::duration_cast<chrono::seconds>(
				chrono::system_clock::now() - startAPI).count()) + "@"
		);
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

void API::tagsList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody)
{
    string api = "tagsList";

    _logger->info(__FILEREF__ + "Received " + api
        + ", requestBody: " + requestBody
    );

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

				string errorMessage = __FILEREF__ + "rows parameter too big"
					+ ", rows: " + to_string(rows)
					+ ", _maxPageSize: " + to_string(_maxPageSize)
				;
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
        if (liveRecordingChunkIt != queryParameters.end()
			&& liveRecordingChunkIt->second != "")
        {
			if (liveRecordingChunkIt->second == "true")
				liveRecordingChunk = 1;
			else if (liveRecordingChunkIt->second == "false")
				liveRecordingChunk = 0;
        }

        {
            Json::Value tagsRoot = _mmsEngineDBFacade->getTagsList(
                    workspace->_workspaceKey, start, rows,
                    liveRecordingChunk, contentTypePresent, contentType, tagNameFilter);

            Json::StreamWriterBuilder wbuilder;
            string responseBody = Json::writeString(wbuilder, tagsRoot);
            
            sendSuccess(request, 200, responseBody);
        }
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

