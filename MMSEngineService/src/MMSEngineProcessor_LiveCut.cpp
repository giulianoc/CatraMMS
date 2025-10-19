
#include "CurlWrapper.h"
#include "Datetime.h"
#include "Encrypt.h"
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"

void MMSEngineProcessor::manageLiveCutThread_streamSegmenter(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json liveCutParametersRoot
)
{
	try
	{
		SPDLOG_INFO(
			string() + "manageLiveCutThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		// string streamSourceType;
		// string ipConfigurationLabel;
		// string satConfigurationLabel;
		int64_t recordingCode;
		string cutPeriodStartTimeInMilliSeconds;
		string cutPeriodEndTimeInMilliSeconds;
		int maxWaitingForLastChunkInSeconds = 90;
		bool errorIfAChunkIsMissing = false;
		{
			/*
			string field = "streamSourceType";
			if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it
			is null"
					+ ", _processorIdentifier: " +
			to_string(_processorIdentifier)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamSourceType = liveCutParametersRoot.get(field, "").asString();

			if (streamSourceType == "IP_PULL")
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or
			it is null"
						+ ", _processorIdentifier: " +
			to_string(_processorIdentifier)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				ipConfigurationLabel = liveCutParametersRoot.get(field,
			"").asString();
			}
			else if (streamSourceType == "Satellite")
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or
			it is null"
						+ ", _processorIdentifier: " +
			to_string(_processorIdentifier)
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				satConfigurationLabel = liveCutParametersRoot.get(field,
			"").asString();
			}
			*/

			// else if (streamSourceType == "IP_PUSH")
			string field = "recordingCode";
			if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			recordingCode = JSONUtils::asInt64(liveCutParametersRoot, field, -1);

			field = "maxWaitingForLastChunkInSeconds";
			maxWaitingForLastChunkInSeconds = JSONUtils::asInt64(liveCutParametersRoot, field, 90);

			field = "errorIfAChunkIsMissing";
			errorIfAChunkIsMissing = JSONUtils::asBool(liveCutParametersRoot, field, false);

			field = "cutPeriod";
			json cutPeriodRoot = liveCutParametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			cutPeriodStartTimeInMilliSeconds = JSONUtils::asString(cutPeriodRoot, field, "");

			field = "end";
			if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			cutPeriodEndTimeInMilliSeconds = JSONUtils::asString(cutPeriodRoot, field, "");
		}

		// Validator validator(_logger, _mmsEngineDBFacade, _configuration);

		int64_t utcCutPeriodStartTimeInMilliSeconds = Datetime::sDateMilliSecondsToUtc(cutPeriodStartTimeInMilliSeconds);

		// next code is the same in the Validator class
		int64_t utcCutPeriodEndTimeInMilliSeconds = Datetime::sDateMilliSecondsToUtc(cutPeriodEndTimeInMilliSeconds);

		/*
		 * 2020-03-30: scenario: period end time is 300 seconds (5 minutes). In
		 * case the chunk is 1 minute, we will take 5 chunks. The result is that
		 * the Cut will fail because:
		 * - we need to cut to 300 seconds
		 * - the duration of the video is 298874 milliseconds
		 * For this reason, when we retrieve the chunks, we will use 'period end
		 * time' plus one second
		 */
		int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = utcCutPeriodEndTimeInMilliSeconds + 1000;

		/*
		int64_t confKey = -1;
		if (streamSourceType == "IP_PULL")
		{
			bool warningIfMissing = false;
			pair<int64_t, string> confKeyAndLiveURL =
		_mmsEngineDBFacade->getIPChannelConfDetails( workspace->_workspaceKey,
		ipConfigurationLabel, warningIfMissing); tie(confKey, ignore) =
		confKeyAndLiveURL;
		}
		else if (streamSourceType == "Satellite")
		{
			bool warningIfMissing = false;
			confKey = _mmsEngineDBFacade->getSATChannelConfDetails(
				workspace->_workspaceKey, satConfigurationLabel,
		warningIfMissing);
		}
		*/

		json mediaItemKeyReferencesRoot = json::array();
		int64_t utcFirstChunkStartTime;
		string firstChunkStartTime;
		int64_t utcLastChunkEndTime;
		string lastChunkEndTime;

		chrono::system_clock::time_point startLookingForChunks = chrono::system_clock::now();

		bool firstRequestedChunk = false;
		bool lastRequestedChunk = false;
		while (!lastRequestedChunk && (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startLookingForChunks).count() <
									   maxWaitingForLastChunkInSeconds))
		{
			int64_t mediaItemKey = -1;
			int64_t physicalPathKey = -1;
			string uniqueName;
			vector<int64_t> otherMediaItemsKey;
			int start = 0;
			int rows = 60 * 1; // assuming every MediaItem is one minute, let's take 1 hour
			bool contentTypePresent = true;
			MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::ContentType::Video;
			// bool startAndEndIngestionDatePresent = false;
			string startIngestionDate;
			string endIngestionDate;
			string title;
			int liveRecordingChunk = 1;
			vector<string> tagsIn;
			vector<string> tagsNotIn;
			string orderBy = "";
			bool admin = false;

			firstRequestedChunk = false;
			lastRequestedChunk = false;

			string jsonCondition;
			{
				// SC: Start Chunk
				// PS: Playout Start, PE: Playout End
				// --------------SC--------------SC--------------SC--------------SC
				//                       PS-------------------------------PE

				jsonCondition = "(";

				// first chunk of the cut
				jsonCondition +=
					("(JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') * "
					 "1000 <= " +
					 to_string(utcCutPeriodStartTimeInMilliSeconds) + " " + "and " + to_string(utcCutPeriodStartTimeInMilliSeconds) +
					 " < JSON_EXTRACT(userData, '$.mmsData.utcChunkEndTime') * "
					 "1000 ) ");

				jsonCondition += " or ";

				// internal chunk of the cut
				jsonCondition +=
					("( " + to_string(utcCutPeriodStartTimeInMilliSeconds) +
					 " <= JSON_EXTRACT(userData, "
					 "'$.mmsData.utcChunkStartTime') * 1000 " +
					 "and JSON_EXTRACT(userData, '$.mmsData.utcChunkEndTime') "
					 "* 1000 <= " +
					 to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond) + ") ");

				jsonCondition += " or ";

				// last chunk of the cut
				jsonCondition +=
					("( JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') "
					 "* 1000 < " +
					 to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond) + " " + "and " +
					 to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond) +
					 " <= JSON_EXTRACT(userData, '$.mmsData.utcChunkEndTime') "
					 "* 1000 ) ");

				jsonCondition += ")";
			}
			string jsonOrderBy = "JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') asc";

			long utcPreviousUtcChunkEndTime = -1;
			bool firstRetrievedChunk = true;

			// retrieve the reference of all the MediaItems to be concatenate
			mediaItemKeyReferencesRoot.clear();

			// json mediaItemsListRoot;
			json mediaItemsRoot;
			do
			{
				int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
				int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;
				set<string> responseFields;
				json mediaItemsListRoot = _mmsEngineDBFacade->getMediaItemsList(
					workspace->_workspaceKey, mediaItemKey, uniqueName, physicalPathKey, otherMediaItemsKey, start, rows, contentTypePresent,
					contentType,
					// startAndEndIngestionDatePresent,
					startIngestionDate, endIngestionDate, title, liveRecordingChunk, recordingCode, utcCutPeriodStartTimeInMilliSeconds,
					utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, jsonCondition, tagsIn, tagsNotIn, orderBy, jsonOrderBy, responseFields, admin,
					// 2022-12-18: MIKs potrebbero essere stati appena
					// aggiunti
					true
				);

				string field = "response";
				json responseRoot = mediaItemsListRoot[field];

				field = "mediaItems";
				mediaItemsRoot = responseRoot[field];

				for (int mediaItemIndex = 0; mediaItemIndex < mediaItemsRoot.size(); mediaItemIndex++)
				{
					json mediaItemRoot = mediaItemsRoot[mediaItemIndex];

					field = "mediaItemKey";
					int64_t mediaItemKey = JSONUtils::asInt64(mediaItemRoot, field, 0);

					json userDataRoot;
					{
						field = "userData";
						string userData = JSONUtils::asString(mediaItemRoot, field, "");
						if (userData == "")
						{
							string errorMessage = string() + "recording media item without userData!!!" +
												  ", _processorIdentifier: " + to_string(_processorIdentifier) +
												  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						userDataRoot = JSONUtils::toJson(userData);
					}

					field = "mmsData";
					json mmsDataRoot = userDataRoot[field];

					field = "utcChunkStartTime";
					time_t currentUtcChunkStartTime = JSONUtils::asInt64(mmsDataRoot, field, 0);

					field = "utcChunkEndTime";
					time_t currentUtcChunkEndTime = JSONUtils::asInt64(mmsDataRoot, field, 0);

					string currentChunkStartTime;
					string currentChunkEndTime;
					{
						// char strDateTime[64];
						tm tmDateTime;

						localtime_r(&currentUtcChunkStartTime, &tmDateTime);
						/*
						sprintf(
							strDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
						);
						currentChunkStartTime = strDateTime;
						*/
						currentChunkStartTime = std::format(
							"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
						);

						localtime_r(&currentUtcChunkEndTime, &tmDateTime);
						/*
						sprintf(
							strDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
						);
						currentChunkEndTime = strDateTime;
						*/
						currentChunkEndTime = std::format(
							"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
						);
					}

					SPDLOG_INFO(
						string() + "Retrieved chunk" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaITemKey: " + to_string(mediaItemKey) +
						", currentUtcChunkStartTime: " + to_string(currentUtcChunkStartTime) + " (" + currentChunkStartTime + ")" +
						", currentUtcChunkEndTime: " + to_string(currentUtcChunkEndTime) + " (" + currentChunkEndTime + ")"
					);

					// check if it is the next chunk
					if (utcPreviousUtcChunkEndTime != -1 && utcPreviousUtcChunkEndTime != currentUtcChunkStartTime)
					{
						string previousUtcChunkEndTime;
						{
							// char strDateTime[64];
							tm tmDateTime;

							localtime_r(&utcPreviousUtcChunkEndTime, &tmDateTime);

							/*
							sprintf(
								strDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
								tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
							);
							previousUtcChunkEndTime = strDateTime;
							*/
							previousUtcChunkEndTime = std::format(
								"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
								tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
							);
						}

						// it is not the next chunk
						string errorMessage =
							string("#Chunks check. Next chunk was not found") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) +
							", utcPreviousUtcChunkEndTime: " + to_string(utcPreviousUtcChunkEndTime) + " (" + previousUtcChunkEndTime + ")" +
							", currentUtcChunkStartTime: " + to_string(currentUtcChunkStartTime) + " (" + currentChunkStartTime + ")" +
							", currentUtcChunkEndTime: " + to_string(currentUtcChunkEndTime) + " (" + currentChunkEndTime + ")" +
							", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" +
							cutPeriodStartTimeInMilliSeconds + ")" +
							", utcCutPeriodEndTimeInMilliSeconds: " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" +
							cutPeriodEndTimeInMilliSeconds + ")";
						if (errorIfAChunkIsMissing)
						{
							SPDLOG_ERROR(string() + errorMessage);

							throw runtime_error(errorMessage);
						}
						else
						{
							_logger->warn(string() + errorMessage);
						}
					}

					// check if it is the first chunk
					if (firstRetrievedChunk)
					{
						firstRetrievedChunk = false;

						// check that it is the first chunk

						if (!(currentUtcChunkStartTime * 1000 <= utcCutPeriodStartTimeInMilliSeconds &&
							  utcCutPeriodStartTimeInMilliSeconds < currentUtcChunkEndTime * 1000))
						{
							firstRequestedChunk = false;

							// it is not the first chunk
							string errorMessage =
								string("#Chunks check. First chunk was not found") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								", ingestionJobKey: " + to_string(ingestionJobKey) + ", first utcChunkStart: " + to_string(currentUtcChunkStartTime) +
								" (" + currentChunkStartTime + ")" + ", first currentUtcChunkEndTime: " + to_string(currentUtcChunkEndTime) + " (" +
								currentChunkEndTime + ")" +
								", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" +
								cutPeriodStartTimeInMilliSeconds + ")" +
								", utcCutPeriodEndTimeInMilliSeconds: " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" +
								cutPeriodEndTimeInMilliSeconds + ")";
							if (errorIfAChunkIsMissing)
							{
								SPDLOG_ERROR(string() + errorMessage);

								throw runtime_error(errorMessage);
							}
							else
							{
								_logger->warn(string() + errorMessage);
							}
						}
						else
						{
							firstRequestedChunk = true;
						}

						utcFirstChunkStartTime = currentUtcChunkStartTime;
						firstChunkStartTime = currentChunkStartTime;
					}

					{
						json mediaItemKeyReferenceRoot;

						field = "mediaItemKey";
						mediaItemKeyReferenceRoot[field] = mediaItemKey;

						mediaItemKeyReferencesRoot.push_back(mediaItemKeyReferenceRoot);
					}

					{
						// check if it is the last chunk

						if (!(currentUtcChunkStartTime * 1000 < utcCutPeriodEndTimeInMilliSecondsPlusOneSecond &&
							  utcCutPeriodEndTimeInMilliSecondsPlusOneSecond <= currentUtcChunkEndTime * 1000))
							lastRequestedChunk = false;
						else
						{
							lastRequestedChunk = true;
							utcLastChunkEndTime = currentUtcChunkEndTime;
							lastChunkEndTime = currentChunkEndTime;
						}
					}

					utcPreviousUtcChunkEndTime = currentUtcChunkEndTime;
				}

				start += rows;

				SPDLOG_INFO(
					string() + "Retrieving chunk" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", start: " + to_string(start) + ", rows: " + to_string(rows) +
					", mediaItemsRoot.size: " + to_string(mediaItemsRoot.size()) + ", lastRequestedChunk: " + to_string(lastRequestedChunk)
				);
			} while (mediaItemsRoot.size() == rows);

			// just waiting if the last chunk was not finished yet
			if (!lastRequestedChunk)
			{
				if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startLookingForChunks).count() <
					maxWaitingForLastChunkInSeconds)
				{
					int secondsToWaitLastChunk = 30;

					this_thread::sleep_for(chrono::seconds(secondsToWaitLastChunk));
				}
			}
		}

		if (!firstRequestedChunk || !lastRequestedChunk)
		{
			string errorMessage = string("#Chunks check. Chunks not available") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", firstRequestedChunk: " + to_string(firstRequestedChunk) +
								  ", lastRequestedChunk: " +
								  to_string(lastRequestedChunk)
								  // + ", streamSourceType: " + streamSourceType
								  // + ", ipConfigurationLabel: " + ipConfigurationLabel
								  // + ", satConfigurationLabel: " + satConfigurationLabel
								  + ", recordingCode: " + to_string(recordingCode) +
								  ", cutPeriodStartTimeInMilliSeconds: " + cutPeriodStartTimeInMilliSeconds +
								  ", cutPeriodEndTimeInMilliSeconds: " + cutPeriodEndTimeInMilliSeconds +
								  ", maxWaitingForLastChunkInSeconds: " + to_string(maxWaitingForLastChunkInSeconds);
			if (errorIfAChunkIsMissing)
			{
				SPDLOG_ERROR(string() + errorMessage);

				throw runtime_error(errorMessage);
			}
			else
			{
				_logger->warn(string() + errorMessage);
			}
		}

		SPDLOG_INFO(
			string() + "Preparing workflow to ingest..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		json liveCutOnSuccess = nullptr;
		json liveCutOnError = nullptr;
		json liveCutOnComplete = nullptr;
		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
			if (JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
			{
				json internalMMSRoot = liveCutParametersRoot[field];

				field = "credentials";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					json credentialsRoot = internalMMSRoot[field];

					field = "userKey";
					userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

					field = "apiKey";
					string apiKeyEncrypted = JSONUtils::asString(credentialsRoot, field, "");
					apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
				}

				field = "events";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					json eventsRoot = internalMMSRoot[field];

					field = "onSuccess";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						liveCutOnSuccess = eventsRoot[field];

					field = "onError";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						liveCutOnError = eventsRoot[field];

					field = "onComplete";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						liveCutOnComplete = eventsRoot[field];
				}
			}
		}

		// create workflow to ingest
		string workflowMetadata;
		{
			json concatDemuxerRoot;
			json concatDemuxerParametersRoot;
			{
				string field = "label";
				concatDemuxerRoot[field] = "Concat from " + to_string(utcFirstChunkStartTime) + " (" + firstChunkStartTime + ") to " +
										   to_string(utcLastChunkEndTime) + " (" + lastChunkEndTime + ")";

				field = "type";
				concatDemuxerRoot[field] = "Concat-Demuxer";

				concatDemuxerParametersRoot = liveCutParametersRoot;
				{
					field = "recordingCode";
					concatDemuxerParametersRoot.erase(field);
				}

				{
					field = "cutPeriod";
					concatDemuxerParametersRoot.erase(field);
				}
				{
					field = "maxWaitingForLastChunkInSeconds";
					if (JSONUtils::isMetadataPresent(concatDemuxerParametersRoot, field))
					{
						concatDemuxerParametersRoot.erase(field);
					}
				}

				field = "retention";
				concatDemuxerParametersRoot[field] = "0";

				field = "references";
				concatDemuxerParametersRoot[field] = mediaItemKeyReferencesRoot;

				field = "parameters";
				concatDemuxerRoot[field] = concatDemuxerParametersRoot;
			}

			json cutRoot;
			{
				string field = "label";
				cutRoot[field] = string("Cut (Live) from ") + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" +
								 cutPeriodStartTimeInMilliSeconds + ") to " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" +
								 cutPeriodEndTimeInMilliSeconds + ")";

				field = "type";
				cutRoot[field] = "Cut";

				json cutParametersRoot = concatDemuxerParametersRoot;
				{
					field = "references";
					cutParametersRoot.erase(field);
				}

				field = "retention";
				cutParametersRoot[field] = JSONUtils::asString(liveCutParametersRoot, field, "");

				double startTimeInMilliSeconds = utcCutPeriodStartTimeInMilliSeconds - (utcFirstChunkStartTime * 1000);
				double startTimeInSeconds = startTimeInMilliSeconds / 1000;
				field = "startTime";
				cutParametersRoot[field] = startTimeInSeconds;

				double endTimeInMilliSeconds = utcCutPeriodEndTimeInMilliSeconds - (utcFirstChunkStartTime * 1000);
				double endTimeInSeconds = endTimeInMilliSeconds / 1000;
				field = "endTime";
				cutParametersRoot[field] = endTimeInSeconds;

				// 2020-07-19: keyFrameSeeking by default it is true.
				//	Result is that the cut is a bit over (in my test it was
				// about one second more). 	Using keyFrameSeeking false the Cut
				// is accurate.
				string cutType = "FrameAccurateWithoutEncoding";
				field = "cutType";
				cutParametersRoot[field] = cutType;

				bool fixEndTimeIfOvercomeDuration;
				if (!errorIfAChunkIsMissing)
					fixEndTimeIfOvercomeDuration = true;
				else
					fixEndTimeIfOvercomeDuration = false;
				field = "fixEndTimeIfOvercomeDuration";
				cutParametersRoot[field] = fixEndTimeIfOvercomeDuration;

				{
					json userDataRoot;

					field = "userData";
					if (JSONUtils::isMetadataPresent(liveCutParametersRoot, field))
					{
						// to_string(static_cast<int>(liveCutParametersRoot[field].type()))
						// == 7 means objectValue
						//		(see Json::ValueType definition:
						// http://jsoncpp.sourceforge.net/value_8h_source.html)

						json::value_t valueType = liveCutParametersRoot[field].type();

						SPDLOG_INFO(
							string() + "Preparing workflow to ingest... (2)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", type: " + to_string(static_cast<int>(valueType))
						);

						if (valueType == json::value_t::string)
						{
							string sUserData = JSONUtils::asString(liveCutParametersRoot, field, "");

							if (sUserData != "")
								userDataRoot = JSONUtils::toJson(sUserData);
						}
						else // if (valueType == Json::ValueType::objectValue)
						{
							userDataRoot = liveCutParametersRoot[field];
						}
					}

					json mmsDataRoot;

					/*
					 * liveCutUtcStartTimeInMilliSecs and
					liveCutUtcEndTimeInMilliSecs was
					 * commented because:
					 *	1. they do not have the right name
					 *	2. LiveCut generates a workflow of
					 *		- concat of chunks --> cut of the concat
					 *		The Concat media will have the TimeCode because they
					are *		automatically generated by the task (see the
					concat method *		in this class) *		The Cut media
					will have the TimeCode because they are * automatically
					generated by the task (see the cut method *		in this
					class)

					field = "liveCutUtcStartTimeInMilliSecs";
					mmsDataRoot[field] = utcCutPeriodStartTimeInMilliSeconds;

					field = "liveCutUtcEndTimeInMilliSecs";
					mmsDataRoot[field] = utcCutPeriodEndTimeInMilliSeconds;
					*/

					/*
					field = "streamSourceType";
					mmsDataRoot[field] = streamSourceType;

					if (streamSourceType == "IP_PULL")
					{
						field = "configurationLabel";
						mmsDataRoot[field] = ipConfigurationLabel;
					}
					else if (streamSourceType == "Satellite")
					{
						field = "configurationLabel";
						mmsDataRoot[field] = satConfigurationLabel;
					}
					else // if (streamSourceType == "IP_PUSH")
					{
						field = "actAsServerChannelCode";
						mmsDataRoot[field] = actAsServerChannelCode;
					}
					*/
					field = "recordingCode";
					mmsDataRoot[field] = recordingCode;

					field = "mmsData";
					userDataRoot["mmsData"] = mmsDataRoot;

					field = "userData";
					cutParametersRoot[field] = userDataRoot;
				}

				field = "parameters";
				cutRoot[field] = cutParametersRoot;

				if (liveCutOnSuccess != nullptr)
				{
					field = "onSuccess";
					cutRoot[field] = liveCutOnSuccess;
				}
				if (liveCutOnError != nullptr)
				{
					field = "onError";
					cutRoot[field] = liveCutOnError;
				}
				if (liveCutOnComplete != nullptr)
				{
					field = "onComplete";
					cutRoot[field] = liveCutOnComplete;
				}
			}

			json concatOnSuccessRoot;
			{
				json cutTaskRoot;
				string field = "task";
				cutTaskRoot[field] = cutRoot;

				field = "onSuccess";
				concatDemuxerRoot[field] = cutTaskRoot;
			}

			json workflowRoot;
			{
				string field = "label";
				workflowRoot[field] = string("Cut from ") + to_string(utcCutPeriodStartTimeInMilliSeconds) + " (" + cutPeriodStartTimeInMilliSeconds +
									  ") to " + to_string(utcCutPeriodEndTimeInMilliSeconds) + " (" + cutPeriodEndTimeInMilliSeconds + ")";

				field = "type";
				workflowRoot[field] = "Workflow";

				field = "task";
				workflowRoot[field] = concatDemuxerRoot;
			}

			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		vector<string> otherHeaders;
		string sResponse =
			CurlWrapper::httpPostString(
				_mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey), workflowMetadata,
				"application/json", // contentType
				otherHeaders, std::format(", ingestionJobKey: {}", ingestionJobKey)
			)
				.second;

		// mancherebbe la parte aggiunta a LiveCut hls segmenter

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" +
			", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageLiveCutThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_IngestionFailure" + ", errorMessage: " + e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", errorMessage: " + ex.what()
			);
		}

		return;
		// throw e;
	}
}

void MMSEngineProcessor::manageLiveCutThread_hlsSegmenter(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string ingestionJobLabel, shared_ptr<Workspace> workspace,
	json liveCutParametersRoot
)
{
	try
	{
		SPDLOG_INFO(
			"manageLiveCutThread"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", _processorsThreadsNumber.use_count(): {}",
			_processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
		);

		int64_t recordingCode;
		int64_t chunkEncodingProfileKey = -1;
		string chunkEncodingProfileLabel;
		string cutPeriodStartTimeInMilliSeconds;
		string cutPeriodEndTimeInMilliSeconds;
		int maxWaitingForLastChunkInSeconds = 90;
		bool errorIfAChunkIsMissing = false;
		{
			if (!JSONUtils::isMetadataPresent(liveCutParametersRoot, "recordingCode"))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", Field: {}",
					_processorIdentifier, ingestionJobKey, "recordingCode"
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			recordingCode = JSONUtils::asInt64(liveCutParametersRoot, "recordingCode", -1);

			chunkEncodingProfileKey = JSONUtils::asInt64(liveCutParametersRoot, "chunkEncodingProfileKey", -1);
			chunkEncodingProfileLabel = JSONUtils::asString(liveCutParametersRoot, "chunkEncodingProfileLabel", "");

			maxWaitingForLastChunkInSeconds = JSONUtils::asInt64(liveCutParametersRoot, "maxWaitingForLastChunkInSeconds", 90);

			errorIfAChunkIsMissing = JSONUtils::asBool(liveCutParametersRoot, "errorIfAChunkIsMissing", false);

			json cutPeriodRoot = liveCutParametersRoot["cutPeriod"];

			if (!JSONUtils::isMetadataPresent(cutPeriodRoot, "start"))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", Field: {}",
					_processorIdentifier, ingestionJobKey, "start"
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			cutPeriodStartTimeInMilliSeconds = JSONUtils::asString(cutPeriodRoot, "start", "");

			if (!JSONUtils::isMetadataPresent(cutPeriodRoot, "end"))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", Field: {}",
					_processorIdentifier, ingestionJobKey, "end"
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			cutPeriodEndTimeInMilliSeconds = JSONUtils::asString(cutPeriodRoot, "end", "");
		}

		// Validator validator(_logger, _mmsEngineDBFacade, _configuration);

		int64_t utcCutPeriodStartTimeInMilliSeconds = Datetime::sDateMilliSecondsToUtc(cutPeriodStartTimeInMilliSeconds);

		// next code is the same in the Validator class
		int64_t utcCutPeriodEndTimeInMilliSeconds = Datetime::sDateMilliSecondsToUtc(cutPeriodEndTimeInMilliSeconds);

		/*
		 * 2020-03-30: scenario: period end time is 300 seconds (5 minutes). In
		 * case the chunk is 1 minute, we will take 5 chunks. The result is that
		 * the Cut will fail because:
		 * - we need to cut to 300 seconds
		 * - the duration of the video is 298874 milliseconds
		 * For this reason, when we retrieve the chunks, we will use 'period end
		 * time' plus one second
		 */
		int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = utcCutPeriodEndTimeInMilliSeconds + 1000;

		json mediaItemKeyReferencesRoot = json::array();
		int64_t utcFirstChunkStartTimeInMilliSecs;
		string firstChunkStartTime;
		int64_t utcLastChunkEndTimeInMilliSecs;
		string lastChunkEndTime;

		chrono::system_clock::time_point startLookingForChunks = chrono::system_clock::now();

		bool firstRequestedChunk = false;
		bool lastRequestedChunk = false;
		while (!lastRequestedChunk && (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startLookingForChunks).count() <
									   maxWaitingForLastChunkInSeconds))
		{
			vector<int64_t> otherMediaItemsKey;
			int start = 0;
			int rows = 60 * 1; // assuming every MediaItem is one minute, let's take 1 hour
			// bool startAndEndIngestionDatePresent = false;
			vector<string> tagsIn;
			vector<string> tagsNotIn;

			firstRequestedChunk = false;
			lastRequestedChunk = false;

			/*
			{
				// SC: Start Chunk
				// PS: Playout Start, PE: Playout End
				//
			--------------SC--------------SC--------------SC--------------SC
				//                       PS-------------------------------PE
			}
			*/
			long utcPreviousUtcChunkEndTimeInMilliSecs = -1;
			bool firstRetrievedChunk = true;

			// retrieve the reference of all the MediaItems to be concatenate
			mediaItemKeyReferencesRoot.clear();

			// json mediaItemsListRoot;
			json mediaItemsRoot;
			do
			{
				set<string> responseFields;
				json mediaItemsListRoot = _mmsEngineDBFacade->getMediaItemsList(
					workspace->_workspaceKey, -1, "", -1, otherMediaItemsKey, start, rows, true, MMSEngineDBFacade::ContentType::Video, "", "", "", 1,
					recordingCode, utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, "", tagsIn, tagsNotIn,
					"utcStartTimeInMilliSecs_virtual asc", "", responseFields, false,
					// 2022-12-18: MIKs potrebbero essere stati appena aggiunti
					true
				);

				json responseRoot = mediaItemsListRoot["response"];

				mediaItemsRoot = responseRoot["mediaItems"];

				for (int mediaItemIndex = 0; mediaItemIndex < mediaItemsRoot.size(); mediaItemIndex++)
				{
					json mediaItemRoot = mediaItemsRoot[mediaItemIndex];

					int64_t mediaItemKey = JSONUtils::asInt64(mediaItemRoot, "mediaItemKey", 0);

					json userDataRoot;
					{
						string userData = JSONUtils::asString(mediaItemRoot, "userData", "");
						if (userData == "")
						{
							string errorMessage = std::format(
								"recording media item without userData!!!"
								", _processorIdentifier: {}"
								", ingestionJobKey: {}"
								", mediaItemKey: {}",
								_processorIdentifier, ingestionJobKey, mediaItemKey
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						userDataRoot = JSONUtils::toJson(userData);
					}

					json mmsDataRoot = userDataRoot["mmsData"];
					int64_t currentUtcChunkStartTimeInMilliSecs = JSONUtils::asInt64(mmsDataRoot, "utcStartTimeInMilliSecs", 0);
					int64_t currentUtcChunkEndTimeInMilliSecs = JSONUtils::asInt64(mmsDataRoot, "utcEndTimeInMilliSecs", 0);

					string currentChunkStartTime;
					string currentChunkEndTime;
					{
						// char strDateTime[64];
						tm tmDateTime;

						time_t currentUtcChunkStartTime = currentUtcChunkStartTimeInMilliSecs / 1000;
						localtime_r(&currentUtcChunkStartTime, &tmDateTime);
						/*
						sprintf(
							strDateTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec, (int)(currentUtcChunkStartTimeInMilliSecs % 1000)
						);
						currentChunkStartTime = strDateTime;
						*/
						currentChunkStartTime = std::format(
							"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}.{:0>3}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec, (int)(currentUtcChunkStartTimeInMilliSecs % 1000)
						);

						time_t currentUtcChunkEndTime = currentUtcChunkEndTimeInMilliSecs / 1000;
						localtime_r(&currentUtcChunkEndTime, &tmDateTime);
						/*
						sprintf(
							strDateTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec, (int)(currentUtcChunkEndTimeInMilliSecs % 1000)
						);
						currentChunkEndTime = strDateTime;
						*/
						currentChunkEndTime = std::format(
							"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}.{:0>3}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
							tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec, (int)(currentUtcChunkEndTimeInMilliSecs % 1000)
						);
					}

					SPDLOG_INFO(
						"Retrieved chunk"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", mediaITemKey: {}"
						", currentUtcChunkStartTimeInMilliSecs: {} ({})"
						", currentUtcChunkEndTimeInMilliSecs: {} ({})",
						_processorIdentifier, ingestionJobKey, mediaItemKey, currentUtcChunkStartTimeInMilliSecs, currentChunkStartTime,
						currentUtcChunkEndTimeInMilliSecs, currentChunkEndTime
					);

					// check if it is the next chunk
					if (utcPreviousUtcChunkEndTimeInMilliSecs != -1 && utcPreviousUtcChunkEndTimeInMilliSecs != currentUtcChunkStartTimeInMilliSecs)
					{
						string previousUtcChunkEndTime;
						{
							// char strDateTime[64];
							tm tmDateTime;

							time_t utcPreviousUtcChunkEndTime = utcPreviousUtcChunkEndTimeInMilliSecs / 1000;
							localtime_r(&utcPreviousUtcChunkEndTime, &tmDateTime);

							/*
							sprintf(
								strDateTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1,
								tmDateTime.tm_mday, tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec,
								(int)(utcPreviousUtcChunkEndTimeInMilliSecs % 1000)
							);
							previousUtcChunkEndTime = strDateTime;
							*/
							previousUtcChunkEndTime = std::format(
								"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}.{:0>3}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1,
								tmDateTime.tm_mday, tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec,
								(int)(utcPreviousUtcChunkEndTimeInMilliSecs % 1000)
							);
						}

						// it is not the next chunk
						string errorMessage = std::format(
							"Next chunk was not found"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", utcPreviousUtcChunkEndTimeInMilliSecs: {} ({})"
							", currentUtcChunkStartTimeInMilliSecs: {} ({})"
							", currentUtcChunkEndTimeInMilliSecs: {} ({})"
							", utcCutPeriodStartTimeInMilliSeconds: {} ({})"
							", utcCutPeriodEndTimeInMilliSeconds: {} ({})",
							_processorIdentifier, ingestionJobKey, utcPreviousUtcChunkEndTimeInMilliSecs, previousUtcChunkEndTime,
							currentUtcChunkStartTimeInMilliSecs, currentChunkStartTime, currentUtcChunkEndTimeInMilliSecs, currentChunkEndTime,
							utcCutPeriodStartTimeInMilliSeconds, cutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSeconds,
							cutPeriodEndTimeInMilliSeconds
						);
						if (errorIfAChunkIsMissing)
						{
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}
						else
							SPDLOG_WARN(errorMessage);
					}

					// check if it is the first chunk
					if (firstRetrievedChunk)
					{
						firstRetrievedChunk = false;

						// check that it is the first chunk

						if (!(currentUtcChunkStartTimeInMilliSecs <= utcCutPeriodStartTimeInMilliSeconds &&
							  utcCutPeriodStartTimeInMilliSeconds < currentUtcChunkEndTimeInMilliSecs))
						{
							firstRequestedChunk = false;

							// it is not the first chunk
							string errorMessage = std::format(
								"First chunk was not found"
								", _processorIdentifier: {}"
								", ingestionJobKey: {}"
								", first utcChunkStartInMilliSecs: {} ({})"
								", first currentUtcChunkEndTimeInMilliSecs: {} ({})"
								", utcCutPeriodStartTimeInMilliSeconds: {} ({})"
								", utcCutPeriodEndTimeInMilliSeconds: {} ({})",
								_processorIdentifier, ingestionJobKey, currentUtcChunkStartTimeInMilliSecs, currentChunkStartTime,
								currentUtcChunkEndTimeInMilliSecs, currentChunkEndTime, utcCutPeriodStartTimeInMilliSeconds,
								cutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSeconds, cutPeriodEndTimeInMilliSeconds
							);
							if (errorIfAChunkIsMissing)
							{
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
							else
								SPDLOG_WARN(errorMessage);
						}
						else
						{
							firstRequestedChunk = true;
						}

						utcFirstChunkStartTimeInMilliSecs = currentUtcChunkStartTimeInMilliSecs;
						firstChunkStartTime = currentChunkStartTime;
					}

					{
						json mediaItemKeyReferenceRoot;

						mediaItemKeyReferenceRoot["mediaItemKey"] = mediaItemKey;

						if (chunkEncodingProfileKey != -1)
							mediaItemKeyReferenceRoot["encodingProfileKey"] = chunkEncodingProfileKey;
						else if (chunkEncodingProfileLabel != "")
							mediaItemKeyReferenceRoot["encodingProfileLabel"] = chunkEncodingProfileLabel;

						mediaItemKeyReferencesRoot.push_back(mediaItemKeyReferenceRoot);
					}

					{
						// check if it is the last chunk

						if (!(currentUtcChunkStartTimeInMilliSecs < utcCutPeriodEndTimeInMilliSecondsPlusOneSecond &&
							  utcCutPeriodEndTimeInMilliSecondsPlusOneSecond <= currentUtcChunkEndTimeInMilliSecs))
							lastRequestedChunk = false;
						else
						{
							lastRequestedChunk = true;
							utcLastChunkEndTimeInMilliSecs = currentUtcChunkEndTimeInMilliSecs;
							lastChunkEndTime = currentChunkEndTime;
						}
					}

					utcPreviousUtcChunkEndTimeInMilliSecs = currentUtcChunkEndTimeInMilliSecs;
				}

				start += rows;

				SPDLOG_INFO(
					"Retrieving chunk"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", start: {}"
					", rows: {}"
					", mediaItemsRoot.size: {}"
					", lastRequestedChunk: {}",
					_processorIdentifier, ingestionJobKey, start, rows, mediaItemsRoot.size(), lastRequestedChunk
				);
			} while (mediaItemsRoot.size() == rows);

			// just waiting if the last chunk was not finished yet
			if (!lastRequestedChunk)
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (chrono::duration_cast<chrono::seconds>(now - startLookingForChunks).count() < maxWaitingForLastChunkInSeconds)
				{
					int secondsToWaitLastChunk = 15;

					SPDLOG_INFO(
						"Sleeping to wait the last chunk..."
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", maxWaitingForLastChunkInSeconds: {}"
						", seconds passed: {}"
						", secondsToWait before next check: {}",
						_processorIdentifier, ingestionJobKey, maxWaitingForLastChunkInSeconds,
						chrono::duration_cast<chrono::seconds>(now - startLookingForChunks).count(), secondsToWaitLastChunk
					);

					this_thread::sleep_for(chrono::seconds(secondsToWaitLastChunk));
				}
			}
		}

		if (!firstRequestedChunk || !lastRequestedChunk)
		{
			string errorMessage = std::format(
				"Chunks not available"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", firstRequestedChunk: {}"
				", lastRequestedChunk: {}"
				", recordingCode: {}"
				", cutPeriodStartTimeInMilliSeconds: {}"
				", cutPeriodEndTimeInMilliSeconds: {}"
				", maxWaitingForLastChunkInSeconds: {}",
				_processorIdentifier, ingestionJobKey, firstRequestedChunk, lastRequestedChunk, recordingCode, cutPeriodStartTimeInMilliSeconds,
				cutPeriodEndTimeInMilliSeconds, maxWaitingForLastChunkInSeconds
			);
			if (errorIfAChunkIsMissing)
			{
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			else
				SPDLOG_WARN(errorMessage);
		}

		SPDLOG_INFO(
			"Preparing workflow to ingest..."
			", _processorIdentifier: {}"
			", ingestionJobKey: {}",
			_processorIdentifier, ingestionJobKey
		);

		json liveCutOnSuccess = nullptr;
		json liveCutOnError = nullptr;
		json liveCutOnComplete = nullptr;
		int64_t userKey;
		string apiKey;
		{
			if (JSONUtils::isMetadataPresent(liveCutParametersRoot, "internalMMS"))
			{
				json internalMMSRoot = liveCutParametersRoot["internalMMS"];

				if (JSONUtils::isMetadataPresent(internalMMSRoot, "credentials"))
				{
					json credentialsRoot = internalMMSRoot["credentials"];

					userKey = JSONUtils::asInt64(credentialsRoot, "userKey", -1);

					string apiKeyEncrypted = JSONUtils::asString(credentialsRoot, "apiKey", "");
					apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
				}

				if (JSONUtils::isMetadataPresent(internalMMSRoot, "events"))
				{
					json eventsRoot = internalMMSRoot["events"];

					if (JSONUtils::isMetadataPresent(eventsRoot, "onSuccess"))
						liveCutOnSuccess = eventsRoot["onSuccess"];

					if (JSONUtils::isMetadataPresent(eventsRoot, "onError"))
						liveCutOnError = eventsRoot["onError"];

					if (JSONUtils::isMetadataPresent(eventsRoot, "onComplete"))
						liveCutOnComplete = eventsRoot["onComplete"];
				}
			}
		}

		// create workflow to ingest
		string workflowMetadata;
		string cutLabel;
		{
			json concatDemuxerRoot;
			json concatDemuxerParametersRoot;
			{
				concatDemuxerRoot["label"] = std::format(
					"Concat from {} ({}) to {} ({})", utcFirstChunkStartTimeInMilliSecs, firstChunkStartTime, utcLastChunkEndTimeInMilliSecs,
					lastChunkEndTime
				);

				concatDemuxerRoot["type"] = "Concat-Demuxer";

				concatDemuxerParametersRoot = liveCutParametersRoot;
				concatDemuxerParametersRoot.erase("recordingCode");

				concatDemuxerParametersRoot.erase("cutPeriod");

				if (JSONUtils::isMetadataPresent(concatDemuxerParametersRoot, "maxWaitingForLastChunkInSeconds"))
					concatDemuxerParametersRoot.erase("maxWaitingForLastChunkInSeconds");

				concatDemuxerParametersRoot["retention"] = "0";
				concatDemuxerParametersRoot["references"] = mediaItemKeyReferencesRoot;
				concatDemuxerRoot["parameters"] = concatDemuxerParametersRoot;
			}

			json cutRoot;
			{
				cutLabel = std::format(
					"Cut (Live) from {} ({}) to {} ({})", utcCutPeriodStartTimeInMilliSeconds, cutPeriodStartTimeInMilliSeconds,
					utcCutPeriodEndTimeInMilliSeconds, cutPeriodEndTimeInMilliSeconds
				);
				cutRoot["label"] = cutLabel;

				cutRoot["type"] = "Cut";

				json cutParametersRoot = concatDemuxerParametersRoot;
				cutParametersRoot.erase("references");

				cutParametersRoot["retention"] = JSONUtils::asString(liveCutParametersRoot, "retention", "");

				double startTimeInMilliSeconds = utcCutPeriodStartTimeInMilliSeconds - utcFirstChunkStartTimeInMilliSecs;
				double startTimeInSeconds = startTimeInMilliSeconds / 1000;
				if (startTimeInSeconds < 0)
					startTimeInSeconds = 0.0;
				cutParametersRoot["startTime"] = startTimeInSeconds;

				double endTimeInMilliSeconds = utcCutPeriodEndTimeInMilliSeconds - utcFirstChunkStartTimeInMilliSecs;
				double endTimeInSeconds = endTimeInMilliSeconds / 1000;
				cutParametersRoot["endTime"] = endTimeInSeconds;

				// 2020-07-19: keyFrameSeeking by default it is true.
				//	Result is that the cut is a bit over (in my test it was
				// about one second more). 	Using keyFrameSeeking false the Cut
				// is accurate because it could be a bframe too.
				string cutType = "FrameAccurateWithoutEncoding";
				cutParametersRoot["cutType"] = cutType;

				bool fixEndTimeIfOvercomeDuration;
				if (!errorIfAChunkIsMissing)
					fixEndTimeIfOvercomeDuration = true;
				else
					fixEndTimeIfOvercomeDuration = false;
				cutParametersRoot["fixEndTimeIfOvercomeDuration"] = fixEndTimeIfOvercomeDuration;

				{
					json userDataRoot;

					if (JSONUtils::isMetadataPresent(liveCutParametersRoot, "userData"))
					{
						// to_string(static_cast<int>(liveCutParametersRoot[field].type()))
						// == 7 means objectValue
						//		(see Json::ValueType definition:
						// http://jsoncpp.sourceforge.net/value_8h_source.html)

						json::value_t valueType = liveCutParametersRoot["userData"].type();

						SPDLOG_INFO(
							"Preparing workflow to ingest... (2)"
							", _processorIdentifier: {}"
							", ingestionJobKey: {}"
							", type: {}",
							_processorIdentifier, ingestionJobKey, static_cast<int>(valueType)
						);

						if (valueType == json::value_t::string)
						{
							string sUserData = JSONUtils::asString(liveCutParametersRoot, "userData", "");

							if (sUserData != "")
								userDataRoot = JSONUtils::toJson(sUserData);
						}
						else // if (valueType == Json::ValueType::objectValue)
							userDataRoot = liveCutParametersRoot["userData"];
					}

					json mmsDataRoot;

					/*
					 * liveCutUtcStartTimeInMilliSecs and liveCutUtcEndTimeInMilliSecs was commented because:
					 *	1. they do not have the right name
					 *	2. LiveCut generates a workflow of
					 *		- concat of chunks --> cut of the concat
					 *		The Concat media will have the TimeCode because they are automatically generated by the task (see the
					concat method *		in this class) The Cut media will have the TimeCode because they are * automatically
					generated by the task (see the cut method in this class)

					field = "liveCutUtcStartTimeInMilliSecs";
					mmsDataRoot[field] = utcCutPeriodStartTimeInMilliSeconds;

					field = "liveCutUtcEndTimeInMilliSecs";
					mmsDataRoot[field] = utcCutPeriodEndTimeInMilliSeconds;
					*/

					// field = "recordingCode";
					// mmsDataRoot[field] = recordingCode;

					// Per capire il motivo dell'aggiunta dei due campi liveCut
					// e ingestionJobKey, leggi il commento sotto (2023-08-10)
					// in particolare la parte "Per risolvere il problema nr. 2"
					json liveCutRoot;
					liveCutRoot["recordingCode"] = recordingCode;
					liveCutRoot["ingestionJobKey"] = (int64_t)(ingestionJobKey);
					mmsDataRoot["liveCut"] = liveCutRoot;

					userDataRoot["mmsData"] = mmsDataRoot;

					cutParametersRoot["userData"] = userDataRoot;
				}

				cutRoot["parameters"] = cutParametersRoot;

				if (liveCutOnSuccess != nullptr)
					cutRoot["onSuccess"] = liveCutOnSuccess;
				if (liveCutOnError != nullptr)
					cutRoot["onError"] = liveCutOnError;
				if (liveCutOnComplete != nullptr)
					cutRoot["onComplete"] = liveCutOnComplete;
			}

			json concatOnSuccessRoot;
			{
				json cutTaskRoot;
				cutTaskRoot["task"] = cutRoot;
				concatDemuxerRoot["onSuccess"] = cutTaskRoot;
			}

			json workflowRoot;
			{
				workflowRoot["label"] = std::format(
					"{}. Cut from {} ({}) to {} ({})", ingestionJobLabel, utcCutPeriodStartTimeInMilliSeconds, cutPeriodStartTimeInMilliSeconds,
					utcCutPeriodEndTimeInMilliSeconds, cutPeriodEndTimeInMilliSeconds
				);

				workflowRoot["type"] = "Workflow";
				workflowRoot["task"] = concatDemuxerRoot;
			}

			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		vector<string> otherHeaders;
		json workflowResponseRoot = CurlWrapper::httpPostStringAndGetJson(
			_mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey), workflowMetadata,
			"application/json", // contentType
			otherHeaders, std::format(", ingestionJobKey: {}", ingestionJobKey)
		);

		/*
			2023-08-10
			Scenario: abbiamo il seguente workflow:
					GroupOfTask (ingestionJobKey: 5624319) composto da due
LiveCut (ingestionJobKey: 5624317 e 5624318) Concat dipende dal GroupOfTask per concatenare i due file ottenuti dai due LiveCut L'engine esegue i due
LiveCut che creano ognuno un Workflow (Concat e poi Cut, vedi codice c++ sopra) Il GroupOfTask, e quindi il Concat, finiti i due LiveCut, vengono
eseguiti ma non ricevono i due file perchè il LiveCut non ottiene il file ma crea un Workflow per ottenere il file. La tabella
MMS_IngestionJobDependency contiene le seguenti due righe: mysql> select * from MMS_IngestionJobDependency where ingestionJobKey = 5624319;
+---------------------------+-----------------+-----------------+-------------------------+-------------+---------------------------+
| ingestionJobDependencyKey | ingestionJobKey | dependOnSuccess |
dependOnIngestionJobKey | orderNumber | referenceOutputDependency |
+---------------------------+-----------------+-----------------+-------------------------+-------------+---------------------------+
|                   7315565 |         5624319 |               1 | 5624317 | 0 |
0 | |                   7315566 |         5624319 |               1 | 5624318 |
1 |                         0 |
+---------------------------+-----------------+-----------------+-------------------------+-------------+---------------------------+
2 rows in set (0.00 sec)

			In questo scenario abbiamo 2 problemi:
				1. Il GroupOfTask aspetta i due LiveCut mentre dovrebbe
aspettare i due Cut che generano i files e che si trovano all'interno dei due
workflow generati dai LiveCut
				2. GroupOfTask, al suo interno ha come referenceOutput gli
ingestionJobKey dei due LiveCut. Poichè i LiveCut non generano files (perchè i
files vengono generati dai due Cut), GroupOfTask non riceverà alcun file di
output

			Per risolvere il problema nr. 1:
			Per risolvere questo problema, prima che il LiveCut venga marcato
come End_TaskSuccess, bisogna aggiornare la tabella sopra per cambiare le
dipendenze, in sostanza tutti gli ingestionJobKey che dipendevano dal LiveCut,
dovranno dipendere dall'ingestionJobKey del Cut che è il Task che genera il file
all'interno del workflow creato dal LiveCut. Per questo motivo, dalla risposta
dell'ingestion del workflow, ci andiamo a recuperare l'ingestionJobKey del Cut
ed eseguiamo una update della tabella MMS_IngestionJobDependency

			Per risolvere il problema nr. 2:
			Come già accade in altri casi (LiveRecorder con i chunks) indichiamo
al Task Cut, di aggiungere il suo output anche come output del livecut. Questo
accade anche quando viene ingestato un Chunk che appare anche come output del
task Live-Recorder.
		*/
		{
			int64_t cutIngestionJobKey = -1;
			{
				if (!JSONUtils::isMetadataPresent(workflowResponseRoot, "tasks"))
				{
					string errorMessage = std::format(
						"LiveCut workflow ingestion: wrong response, tasks not found"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", workflowResponseRoot: {}",
						_processorIdentifier, ingestionJobKey, JSONUtils::toString(workflowResponseRoot)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				json tasksRoot = workflowResponseRoot["tasks"];
				for (int taskIndex = 0; taskIndex < tasksRoot.size(); taskIndex++)
				{
					json taskRoot = tasksRoot[taskIndex];
					string taskIngestionJobLabel = JSONUtils::asString(taskRoot, "label", "");
					if (taskIngestionJobLabel == cutLabel)
					{
						cutIngestionJobKey = JSONUtils::asInt64(taskRoot, "ingestionJobKey", -1);

						break;
					}
				}
				if (cutIngestionJobKey == -1)
				{
					string errorMessage = std::format(
						"LiveCut workflow ingestion: wrong response, cutLabel not found"
						", _processorIdentifier: {}"
						", ingestionJobKey: {}"
						", cutLabel: {}"
						", workflowResponseRoot: {}",
						_processorIdentifier, ingestionJobKey, cutLabel, JSONUtils::toString(workflowResponseRoot)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			SPDLOG_INFO(
				"changeIngestionJobDependency"
				", ingestionJobKey: {}"
				", cutIngestionJobKey: {}",
				ingestionJobKey, cutIngestionJobKey
			);
			_mmsEngineDBFacade->changeIngestionJobDependency(ingestionJobKey, cutIngestionJobKey);
		}

		SPDLOG_INFO(
			"Update IngestionJob"
			", ingestionJobKey: {}"
			", IngestionStatus: End_TaskSuccess"
			", errorMessage: ",
			ingestionJobKey
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"manageLiveCutThread failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what(): {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		SPDLOG_INFO(
			"Update IngestionJob"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", IngestionStatus: End_IngestionFailure"
			", errorMessage: {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				"Update IngestionJob failed"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", errorMessage: {}",
				_processorIdentifier, ingestionJobKey, re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				"Update IngestionJob failed"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", errorMessage: {}",
				_processorIdentifier, ingestionJobKey, ex.what()
			);
		}

		return;
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"manageLiveCutThread failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}",
			_processorIdentifier, ingestionJobKey
		);

		SPDLOG_INFO(
			"Update IngestionJob"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", IngestionStatus: End_IngestionFailure"
			", errorMessage: {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);
		try
		{
			_mmsEngineDBFacade->updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
		}
		catch (runtime_error &re)
		{
			SPDLOG_INFO(
				"Update IngestionJob failed"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", errorMessage: {}",
				_processorIdentifier, ingestionJobKey, re.what()
			);
		}
		catch (exception &ex)
		{
			SPDLOG_INFO(
				"Update IngestionJob failed"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", errorMessage: {}",
				_processorIdentifier, ingestionJobKey, ex.what()
			);
		}

		return;
		// throw e;
	}
}
