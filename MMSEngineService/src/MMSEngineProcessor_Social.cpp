
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include <regex>

void MMSEngineProcessor::postOnFacebookThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "postOnFacebookThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be posted on Facebook" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		string facebookConfigurationLabel;
		string facebookNodeType;
		string facebookNodeId;
		{
			string field = "facebookConfigurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookNodeType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookNodeType = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookNodeId";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookNodeId = JSONUtils::asString(parametersRoot, field, "");
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				string mmsAssetPathName;
				int64_t sizeInBytes;
				MMSEngineDBFacade::ContentType contentType;

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(ignore, mmsAssetPathName, ignore, ignore, ignore, sizeInBytes, ignore) = physicalPathDetails;

					{
						bool warningIfMissing = false;
						tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
							contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						tie(contentType, ignore, ignore, ignore, ignore, ignore) = contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
					}
				}
				else
				{
					tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(mmsAssetPathName, ignore, ignore, ignore, sizeInBytes, ignore) = physicalPathDetails;

					{
						bool warningIfMissing = false;
						tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
								_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
									workspace->_workspaceKey, key, warningIfMissing,
									// 2022-12-18: MIK potrebbe essere stato
									// appena aggiunto
									true
								);

						tie(ignore, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
					}
				}

				// check on thread availability was done at the beginning in
				// this method
				if (contentType == MMSEngineDBFacade::ContentType::Video)
				{
					postVideoOnFacebook(
						mmsAssetPathName, sizeInBytes, ingestionJobKey, workspace, facebookConfigurationLabel, facebookNodeType, facebookNodeId
					);
				}
				else // if (contentType == ContentType::Audio)
				{
				}
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "post on facebook failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = fmt::format(
					"post on facebook failed"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencyIndex: {}"
					", dependencies.size(): {}",
					_processorIdentifier, ingestionJobKey, dependencyIndex, dependencies.size()
				);
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

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
			string() + "postOnFacebookTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "postOnFacebookTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::postOnYouTubeThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "postOnYouTubeThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No configured any media to be posted on YouTube" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			_logger->warn(errorMessage);

			// throw runtime_error(errorMessage);
		}

		string youTubeConfigurationLabel;
		string youTubeTitle;
		string youTubeDescription;
		json youTubeTags = nullptr;
		int youTubeCategoryId = -1;
		string youTubePrivacyStatus;
		bool youTubeMadeForKids;
		{
			string field = "configurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			youTubeConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "title";
			youTubeTitle = JSONUtils::asString(parametersRoot, field, "");

			field = "description";
			youTubeDescription = JSONUtils::asString(parametersRoot, field, "");

			field = "tags";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				youTubeTags = parametersRoot[field];

			field = "categoryId";
			youTubeCategoryId = JSONUtils::asInt(parametersRoot, field, -1);

			field = "privacyStatus";
			youTubePrivacyStatus = JSONUtils::asString(parametersRoot, field, "private");

			field = "madeForKids";
			youTubeMadeForKids = JSONUtils::asBool(parametersRoot, field, false);
		}

		int dependencyIndex = 0;
		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			bool stopIfReferenceProcessingError = false;

			try
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

				string mmsAssetPathName;
				int64_t sizeInBytes;
				MMSEngineDBFacade::ContentType contentType;
				string title;

				if (dependencyType == Validator::DependencyType::MediaItemKey)
				{
					int64_t encodingProfileKey = -1;

					bool warningIfMissing = false;
					tuple<int64_t, string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
					tie(ignore, mmsAssetPathName, ignore, ignore, ignore, sizeInBytes, ignore) = physicalPathDetails;

					{
						bool warningIfMissing = false;
						tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
							contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

						tie(contentType, ignore, ignore, ignore, ignore, ignore) = contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
					}
				}
				else
				{
					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPathDetails(
							key,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);
					tie(mmsAssetPathName, ignore, ignore, ignore, sizeInBytes, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;

					{
						bool warningIfMissing = false;
						tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
								_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
									workspace->_workspaceKey, key, warningIfMissing,
									// 2022-12-18: MIK potrebbe essere stato
									// appena aggiunto
									true
								);

						tie(ignore, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
							mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
					}
				}

				if (youTubeTitle == "")
					youTubeTitle = title;

				postVideoOnYouTube(
					mmsAssetPathName, sizeInBytes, ingestionJobKey, workspace, youTubeConfigurationLabel, youTubeTitle, youTubeDescription,
					youTubeTags, youTubeCategoryId, youTubePrivacyStatus, youTubeMadeForKids
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = string() + "post on youtube failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencyIndex: " + to_string(dependencyIndex) +
									  ", dependencies.size(): " + to_string(dependencies.size()) + ", e.what(): " + e.what();
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}
			catch (exception e)
			{
				string errorMessage = fmt::format(
					"post on youtube failed"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencyIndex: {}"
					", dependencies.size(): {}",
					_processorIdentifier, ingestionJobKey, dependencyIndex, dependencies.size()
				);
				SPDLOG_ERROR(errorMessage);

				if (dependencies.size() > 1)
				{
					if (stopIfReferenceProcessingError)
						throw runtime_error(errorMessage);
				}
				else
					throw runtime_error(errorMessage);
			}

			dependencyIndex++;
		}

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
			string() + "postOnYouTubeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

		// it's a thread, no throw
		// throw e;
		return;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "postOnYouTubeTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

		// it's a thread, no throw
		// throw e;
		return;
	}
}

void MMSEngineProcessor::youTubeLiveBroadcastThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string ingestionJobLabel, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	try
	{
		SPDLOG_INFO(
			string() + "youTubeLiveBroadcastThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string youTubeConfigurationLabel;
		string youTubeLiveBroadcastTitle;
		string youTubeLiveBroadcastDescription;
		string youTubeLiveBroadcastPrivacyStatus;
		bool youTubeLiveBroadcastMadeForKids;
		string youTubeLiveBroadcastLatencyPreference;

		json scheduleRoot;
		string scheduleStartTimeInSeconds;
		string scheduleEndTimeInSeconds;
		string sourceType;
		// streamConfigurationLabel or referencesRoot has to be present
		string streamConfigurationLabel;
		json referencesRoot;
		{
			string field = "YouTubeConfigurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			youTubeConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "title";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			youTubeLiveBroadcastTitle = JSONUtils::asString(parametersRoot, field, "");

			field = "MadeForKids";
			youTubeLiveBroadcastMadeForKids = JSONUtils::asBool(parametersRoot, "MadeForKids", true);

			field = "Description";
			youTubeLiveBroadcastDescription = JSONUtils::asString(parametersRoot, field, "");

			field = "PrivacyStatus";
			youTubeLiveBroadcastPrivacyStatus = JSONUtils::asString(parametersRoot, field, "unlisted");

			field = "LatencyPreference";
			youTubeLiveBroadcastLatencyPreference = JSONUtils::asString(parametersRoot, field, "normal");

			field = "youTubeSchedule";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			scheduleRoot = parametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(scheduleRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			scheduleStartTimeInSeconds = JSONUtils::asString(scheduleRoot, field, "");

			field = "end";
			if (!JSONUtils::isMetadataPresent(scheduleRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			scheduleEndTimeInSeconds = JSONUtils::asString(scheduleRoot, field, "");

			field = "SourceType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceType = JSONUtils::asString(parametersRoot, field, "");

			if (sourceType == "Live")
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				streamConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");
			}
			else // if (sourceType == "MediaItem")
			{
				field = "references";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				referencesRoot = parametersRoot[field];
			}
		}

		// 1. get refresh_token from the configuration
		// 2. call google API
		// 3. the response will have the access token to be used
		string youTubeAccessToken = getYouTubeAccessTokenByConfigurationLabel(ingestionJobKey, workspace, youTubeConfigurationLabel);

		string youTubeURL;
		string sResponse;
		string broadcastId;

		// first call, create the Live Broadcast
		try
		{
			/*
			* curl -v --request POST \
				'https://youtube.googleapis.com/youtube/v3/liveBroadcasts?part=snippet%2CcontentDetails%2Cstatus'
			\
				--header 'Authorization: Bearer
			ya29.a0ARrdaM9t2WqGKTgB9rZtoZU4oUCnW96Pe8qmgdk6ryYxEEe21T9WXWr8Eai1HX3AzG9zdOAEzRm8T6MhBmuQEDj4C5iDmfhRVjmUakhCKbZ7mWmqLOP9M6t5gha1QsH5ocNKqAZkhbCnWK0euQxGoK79MBjA'
			\
				--header 'Accept: application/json' \
				--header 'Content-Type: application/json' \
				--data '{"snippet":{"title":"Test
			CiborTV","scheduledStartTime":"2021-11-19T20:00:00.000Z","scheduledEndTime":"2021-11-19T22:00:00.000Z"},"contentDetails":{"enableClosedCaptions":true,"enableContentEncryption":true,"enableDvr":true,"enableEmbed":true,"recordFromStart":true,"startWithSlate":true},"status":{"privacyStatus":"unlisted"}}'
			\
				--compressed
			*/
			/*
				2023-02-23: L'account deve essere abilitato a fare live
			   streaming vedi: https://www.youtube.com/watch?v=wnjzdRpOIUA
			*/
			youTubeURL =
				_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + _youTubeDataAPILiveBroadcastURI;

			string body;
			{
				json bodyRoot;

				{
					json snippetRoot;

					string field = "title";
					snippetRoot[field] = youTubeLiveBroadcastTitle;

					if (youTubeLiveBroadcastDescription != "")
					{
						field = "description";
						snippetRoot[field] = youTubeLiveBroadcastDescription;
					}

					if (streamConfigurationLabel != "")
					{
						field = "channelId";
						snippetRoot[field] = streamConfigurationLabel;
					}

					// scheduledStartTime
					{
						int64_t utcScheduleStartTimeInSeconds = DateTime::sDateSecondsToUtc(scheduleStartTimeInSeconds);

						// format: YYYY-MM-DDTHH:MI:SS.000Z
						string scheduleStartTimeInMilliSeconds = scheduleStartTimeInSeconds;
						scheduleStartTimeInMilliSeconds.insert(scheduleStartTimeInSeconds.length() - 1, ".000", 4);

						field = "scheduledStartTime";
						snippetRoot[field] = scheduleStartTimeInMilliSeconds;
					}

					// scheduledEndTime
					{
						int64_t utcScheduleEndTimeInSeconds = DateTime::sDateSecondsToUtc(scheduleEndTimeInSeconds);

						// format: YYYY-MM-DDTHH:MI:SS.000Z
						string scheduleEndTimeInMilliSeconds = scheduleEndTimeInSeconds;
						scheduleEndTimeInMilliSeconds.insert(scheduleEndTimeInSeconds.length() - 1, ".000", 4);

						field = "scheduledEndTime";
						snippetRoot[field] = scheduleEndTimeInMilliSeconds;
					}

					field = "snippet";
					bodyRoot[field] = snippetRoot;
				}

				{
					json contentDetailsRoot;

					bool enableContentEncryption = true;
					string field = "enableContentEncryption";
					contentDetailsRoot[field] = enableContentEncryption;

					bool enableDvr = true;
					field = "enableDvr";
					contentDetailsRoot[field] = enableDvr;

					bool enableEmbed = true;
					field = "enableEmbed";
					contentDetailsRoot[field] = enableEmbed;

					bool recordFromStart = true;
					field = "recordFromStart";
					contentDetailsRoot[field] = recordFromStart;

					bool startWithSlate = true;
					field = "startWithSlate";
					contentDetailsRoot[field] = startWithSlate;

					bool enableAutoStart = true;
					field = "enableAutoStart";
					contentDetailsRoot[field] = enableAutoStart;

					bool enableAutoStop = true;
					field = "enableAutoStop";
					contentDetailsRoot[field] = enableAutoStop;

					field = "latencyPreference";
					contentDetailsRoot[field] = youTubeLiveBroadcastLatencyPreference;

					field = "contentDetails";
					bodyRoot[field] = contentDetailsRoot;
				}

				{
					json statusRoot;

					string field = "privacyStatus";
					statusRoot[field] = youTubeLiveBroadcastPrivacyStatus;

					field = "selfDeclaredMadeForKids";
					statusRoot[field] = youTubeLiveBroadcastMadeForKids;

					field = "status";
					bodyRoot[field] = statusRoot;
				}

				body = JSONUtils::toString(bodyRoot);
			}

			vector<string> headerList;
			{
				string header = "Authorization: Bearer " + youTubeAccessToken;
				headerList.push_back(header);

				header = "Content-Length: " + to_string(body.length());
				headerList.push_back(header);

				header = "Accept: application/json";
				headerList.push_back(header);
			}

			json responseRoot = MMSCURL::httpPostStringAndGetJson(
				_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
				"application/json", // contentType
				headerList
			);

			/* sResponse:
			HTTP/2 200
			content-type: application/json; charset=UTF-8
			vary: Origin
			vary: X-Origin
			vary: Referer
			content-encoding: gzip
			date: Sat, 20 Nov 2021 11:19:49 GMT
			server: scaffolding on HTTPServer2
			cache-control: private
			content-length: 858
			x-xss-protection: 0
			x-frame-options: SAMEORIGIN
			x-content-type-options: nosniff
			alt-svc: h3=":443"; ma=2592000,h3-29=":443";
			ma=2592000,h3-Q050=":443"; ma=2592000,h3-Q046=":443";
			ma=2592000,h3-Q043=":443"; ma=2592000,quic=":443"; ma=2592000;
			v="46,43"

			{
				"kind": "youtube#liveBroadcast",
				"etag": "AwdnruQBkHYB37w_2Rp6zWvtVbw",
				"id": "xdAak4DmKPI",
				"snippet": {
					"publishedAt": "2021-11-19T14:12:14Z",
					"channelId": "UC2WYB3NxVDD0mf-jML8qGAA",
					"title": "Test broadcast 2",
					"description": "",
					"thumbnails": {
						"default": {
							"url":
			"https://i.ytimg.com/vi/xdAak4DmKPI/default_live.jpg", "width": 120,
							"height": 90
						},
						"medium": {
							"url":
			"https://i.ytimg.com/vi/xdAak4DmKPI/mqdefault_live.jpg", "width":
			320, "height": 180
						},
						"high": {
							"url":
			"https://i.ytimg.com/vi/xdAak4DmKPI/hqdefault_live.jpg", "width":
			480, "height": 360
						}
					},
					"scheduledStartTime": "2021-11-19T16:00:00Z",
					"scheduledEndTime": "2021-11-19T17:00:00Z",
					"isDefaultBroadcast": false,
					"liveChatId":
			"KicKGFVDMldZQjNOeFZERDBtZi1qTUw4cUdBQRILeGRBYWs0RG1LUEk"
				},
				"status": {
					"lifeCycleStatus": "created",
					"privacyStatus": "unlisted",
					"recordingStatus": "notRecording",
					"madeForKids": false,
					"selfDeclaredMadeForKids": false
				},
				"contentDetails": {
					"monitorStream": {
					"enableMonitorStream": true,
					"broadcastStreamDelayMs": 0,
					"embedHtml": "\u003ciframe width=\"425\" height=\"344\"
			src=\"https://www.youtube.com/embed/xdAak4DmKPI?autoplay=1&livemonitor=1\"
			frameborder=\"0\" allow=\"accelerometer; autoplay; clipboard-write;
			encrypted-media; gyroscope; picture-in-picture\"
			allowfullscreen\u003e\u003c/iframe\u003e"
					},
					"enableEmbed": true,
					"enableDvr": true,
					"enableContentEncryption": true,
					"startWithSlate": true,
					"recordFromStart": true,
					"enableClosedCaptions": true,
					"closedCaptionsType": "closedCaptionsHttpPost",
					"enableLowLatency": false,
					"latencyPreference": "normal",
					"projection": "rectangular",
					"enableAutoStart": false,
					"enableAutoStop": false
				}
			}
			*/

			string field = "id";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			broadcastId = JSONUtils::asString(responseRoot, field, "");

			sResponse = "";
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("YouTube live broadcast management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse +
								  ", e.what(): " + e.what();
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("YouTube live broadcast management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse;
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}

		string streamId;
		string rtmpURL;

		// second call, create the Live Stream
		try
		{
			/*
			* curl -v --request POST \
				'https://youtube.googleapis.com/youtube/v3/liveStreams?part=snippet%2Ccdn%2CcontentDetails%2Cstatus'
			\
				--header 'Authorization: Bearer
			ya29.a0ARrdaM9t2WqGKTgB9rZtoZU4oUCnW96Pe8qmgdk6ryYxEEe21T9WXWr8Eai1HX3AzG9zdOAEzRm8T6MhBmuQEDj4C5iDmfhRVjmUakhCKbZ7mWmqLOP9M6t5gha1QsH5ocNKqAZkhbCnWK0euQxGoK79MBjA'
			\
				--header 'Accept: application/json' \
				--header 'Content-Type: application/json' \
				--data '{"snippet":{"title":"my new video stream
			name","description":"A description of your video stream. This field
			is
			optional."},"cdn":{"frameRate":"60fps","ingestionType":"rtmp","resolution":"1080p"},"contentDetails":{"isReusable":true}}'
			\
				--compressed
			*/
			youTubeURL =
				_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + _youTubeDataAPILiveStreamURI;

			string body;
			{
				json bodyRoot;

				{
					json snippetRoot;

					string field = "title";
					snippetRoot[field] = youTubeLiveBroadcastTitle;

					if (youTubeLiveBroadcastDescription != "")
					{
						field = "description";
						snippetRoot[field] = youTubeLiveBroadcastDescription;
					}

					if (streamConfigurationLabel != "")
					{
						field = "channelId";
						snippetRoot[field] = streamConfigurationLabel;
					}

					field = "snippet";
					bodyRoot[field] = snippetRoot;
				}

				{
					json cdnRoot;

					string field = "frameRate";
					cdnRoot[field] = "variable";

					field = "ingestionType";
					cdnRoot[field] = "rtmp";

					field = "resolution";
					cdnRoot[field] = "variable";

					field = "cdn";
					bodyRoot[field] = cdnRoot;
				}
				{
					json contentDetailsRoot;

					bool isReusable = true;
					string field = "isReusable";
					contentDetailsRoot[field] = isReusable;

					field = "contentDetails";
					bodyRoot[field] = contentDetailsRoot;
				}

				body = JSONUtils::toString(bodyRoot);
			}

			vector<string> headerList;
			{
				string header = "Authorization: Bearer " + youTubeAccessToken;
				headerList.push_back(header);

				header = "Content-Length: " + to_string(body.length());
				headerList.push_back(header);

				header = "Accept: application/json";
				headerList.push_back(header);
			}

			json responseRoot = MMSCURL::httpPostStringAndGetJson(
				_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
				"application/json", // contentType
				headerList
			);

			/* sResponse:
			HTTP/2 200
			content-type: application/json; charset=UTF-8
			vary: Origin
			vary: X-Origin
			vary: Referer
			content-encoding: gzip
			date: Sat, 20 Nov 2021 11:19:49 GMT
			server: scaffolding on HTTPServer2
			cache-control: private
			content-length: 858
			x-xss-protection: 0
			x-frame-options: SAMEORIGIN
			x-content-type-options: nosniff
			alt-svc: h3=":443"; ma=2592000,h3-29=":443";
			ma=2592000,h3-Q050=":443"; ma=2592000,h3-Q046=":443";
			ma=2592000,h3-Q043=":443"; ma=2592000,quic=":443"; ma=2592000;
			v="46,43"

			{
				"kind": "youtube#liveStream",
				"etag": "MYZZfdTjQds1ghPCh_jyIjtsT9c",
				"id": "2WYB3NxVDD0mf-jML8qGAA1637335849431228",
				"snippet": {
					"publishedAt": "2021-11-19T15:30:49Z",
					"channelId": "UC2WYB3NxVDD0mf-jML8qGAA",
					"title": "my new video stream name",
					"description": "A description of your video stream. This
			field is optional.", "isDefaultStream": false
				},
				"cdn": {
					"ingestionType": "rtmp",
					"ingestionInfo": {
						"streamName": "py80-04jp-6jq3-eq29-407j",
						"ingestionAddress": "rtmp://a.rtmp.youtube.com/live2",
						"backupIngestionAddress":
			"rtmp://b.rtmp.youtube.com/live2?backup=1", "rtmpsIngestionAddress":
			"rtmps://a.rtmps.youtube.com/live2", "rtmpsBackupIngestionAddress":
			"rtmps://b.rtmps.youtube.com/live2?backup=1"
					},
					"resolution": "1080p",
					"frameRate": "60fps"
				},
				"status": {
					"streamStatus": "ready",
					"healthStatus": {
						"status": "noData"
					}
				},
				"contentDetails": {
					"closedCaptionsIngestionUrl":
			"http://upload.youtube.com/closedcaption?cid=py80-04jp-6jq3-eq29-407j",
					"isReusable": true
				}
			}
			*/

			string field = "id";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamId = JSONUtils::asString(responseRoot, field, "");

			field = "cdn";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json cdnRoot = responseRoot[field];

			field = "ingestionInfo";
			if (!JSONUtils::isMetadataPresent(cdnRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json ingestionInfoRoot = cdnRoot[field];

			field = "streamName";
			if (!JSONUtils::isMetadataPresent(ingestionInfoRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string streamName = JSONUtils::asString(ingestionInfoRoot, field, "");

			field = "ingestionAddress";
			if (!JSONUtils::isMetadataPresent(ingestionInfoRoot, field))
			{
				string errorMessage =
					string() + "YouTube response, Field is not present or it is null" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string ingestionAddress = JSONUtils::asString(ingestionInfoRoot, field, "");

			rtmpURL = ingestionAddress + "/" + streamName;

			sResponse = "";
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("YouTube live stream management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse +
								  ", e.what(): " + e.what();
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("YouTube live stream management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse;
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}

		// third call, bind live broadcast - live stream
		try
		{
			/*
			* curl -v --request POST \
				'https://youtube.googleapis.com/youtube/v3/liveBroadcasts/bind?id=xdAak4DmKPI&part=snippet&streamId=2WYB3NxVDD0mf-jML8qGAA1637335849431228'
			\
				--header 'Authorization: Bearer
			ya29.a0ARrdaM9t2WqGKTgB9rZtoZU4oUCnW96Pe8qmgdk6ryYxEEe21T9WXWr8Eai1HX3AzG9zdOAEzRm8T6MhBmuQEDj4C5iDmfhRVjmUakhCKbZ7mWmqLOP9M6t5gha1QsH5ocNKqAZkhbCnWK0euQxGoK79MBjA'
			\
				--header 'Accept: application/json' \
				--compressed
			*/
			string youTubeDataAPILiveBroadcastBindURI = regex_replace(_youTubeDataAPILiveBroadcastBindURI, regex("__BROADCASTID__"), broadcastId);
			youTubeDataAPILiveBroadcastBindURI = regex_replace(youTubeDataAPILiveBroadcastBindURI, regex("__STREAMID__"), streamId);

			youTubeURL =
				_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + youTubeDataAPILiveBroadcastBindURI;

			vector<string> headerList;
			{
				string header = "Authorization: Bearer " + youTubeAccessToken;
				headerList.push_back(header);

				header = "Accept: application/json";
				headerList.push_back(header);
			}

			string body;

			json responseRoot = MMSCURL::httpPostStringAndGetJson(
				_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
				"", // contentType
				headerList
			);

			/* sResponse:
			HTTP/2 200 ^M
			content-type: application/json; charset=UTF-8^M
			vary: X-Origin^M
			vary: Referer^M
			vary: Origin,Accept-Encoding^M
			date: Wed, 24 Nov 2021 22:35:48 GMT^M
			server: scaffolding on HTTPServer2^M
			cache-control: private^M
			x-xss-protection: 0^M
			x-frame-options: SAMEORIGIN^M
			x-content-type-options: nosniff^M
			accept-ranges: none^M
			alt-svc: h3=":443"; ma=2592000,h3-29=":443";
			ma=2592000,h3-Q050=":443"; ma=2592000,h3-Q046=":443";
			ma=2592000,h3-Q043=":443"; ma=2592000,quic=":443"; ma=2592000;
			v="46,43"^M ^M
			{
			"kind": "youtube#liveBroadcast",
			"etag": "1NM7pffpR8009CHTdckGzn0rN-o",
			"id": "tP_L5RKFrQM",
			"snippet": {
				"publishedAt": "2021-11-24T22:35:46Z",
				"channelId": "UC2WYB3NxVDD0mf-jML8qGAA",
				"title": "test",
				"description": "",
				"thumbnails": {
				"default": {
					"url":
			"https://i.ytimg.com/vi/tP_L5RKFrQM/default_live.jpg", "width": 120,
					"height": 90
				},
				"medium": {
					"url":
			"https://i.ytimg.com/vi/tP_L5RKFrQM/mqdefault_live.jpg", "width":
			320, "height": 180
				},
				"high": {
					"url":
			"https://i.ytimg.com/vi/tP_L5RKFrQM/hqdefault_live.jpg", "width":
			480, "height": 360
				},
				"standard": {
					"url":
			"https://i.ytimg.com/vi/tP_L5RKFrQM/sddefault_live.jpg", "width":
			640, "height": 480
				}
				},
				"scheduledStartTime": "2021-11-24T22:25:00Z",
				"scheduledEndTime": "2021-11-24T22:50:00Z",
				"isDefaultBroadcast": false,
				"liveChatId":
			"KicKGFVDMldZQjNOeFZERDBtZi1qTUw4cUdBQRILdFBfTDVSS0ZyUU0"
			}
			}
			*/
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("YouTube live stream management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse +
								  ", e.what(): " + e.what();
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("YouTube live stream management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse;
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}

		SPDLOG_INFO(
			string() + "Preparing workflow to ingest..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		json youTubeLiveBroadcastOnSuccess = nullptr;
		json youTubeLiveBroadcastOnError = nullptr;
		json youTubeLiveBroadcastOnComplete = nullptr;
		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				json internalMMSRoot = parametersRoot[field];

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
						youTubeLiveBroadcastOnSuccess = eventsRoot[field];

					field = "onError";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						youTubeLiveBroadcastOnError = eventsRoot[field];

					field = "onComplete";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						youTubeLiveBroadcastOnComplete = eventsRoot[field];
				}
			}
		}

		// create workflow to ingest
		string workflowMetadata;
		{
			string proxyLabel;
			json proxyRoot;

			if (sourceType == "Live")
			{
				json liveProxyParametersRoot;
				{
					string field = "label";
					proxyLabel = "Proxy " + streamConfigurationLabel + " to YouTube (" + youTubeConfigurationLabel + ")";
					proxyRoot[field] = proxyLabel;

					field = "type";
					proxyRoot[field] = "Live-Proxy";

					liveProxyParametersRoot = parametersRoot;
					{
						field = "YouTubeConfigurationLabel";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "title";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "Description";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "SourceType";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "internalMMS";
						if (JSONUtils::isMetadataPresent(liveProxyParametersRoot, field))
							liveProxyParametersRoot.erase(field);
					}
					{
						field = "youTubeSchedule";
						if (JSONUtils::isMetadataPresent(liveProxyParametersRoot, field))
							liveProxyParametersRoot.erase(field);
					}

					{
						bool timePeriod = true;
						field = "timePeriod";
						liveProxyParametersRoot[field] = timePeriod;

						field = "schedule";
						liveProxyParametersRoot[field] = scheduleRoot;
					}

					json outputsRoot = json::array();
					{
						// we will create/modify the RTMP_Channel using
						// youTubeConfigurationLabel as his label
						try
						{
							bool warningIfMissing = true;
							tuple<int64_t, string, string, string, string, string> rtmpChannelDetails =
								_mmsEngineDBFacade->getRTMPChannelDetails(workspace->_workspaceKey, youTubeConfigurationLabel, warningIfMissing);

							int64_t confKey;
							tie(confKey, ignore, ignore, ignore, ignore, ignore) = rtmpChannelDetails;

							_mmsEngineDBFacade->modifyRTMPChannelConf(
								confKey, workspace->_workspaceKey, youTubeConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}
						catch (DBRecordNotFound &e)
						{
							_mmsEngineDBFacade->addRTMPChannelConf(
								workspace->_workspaceKey, youTubeConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}

						json outputRoot;

						field = "outputType";
						outputRoot[field] = "RTMP_Channel";

						field = "rtmpChannelConfigurationLabel";
						outputRoot[field] = youTubeConfigurationLabel;

						outputsRoot.push_back(outputRoot);
					}
					field = "outputs";
					liveProxyParametersRoot[field] = outputsRoot;
				}
				string field = "parameters";
				proxyRoot[field] = liveProxyParametersRoot;
			}
			else // if (sourceType == "MediaItem")
			{
				json vodProxyParametersRoot;
				{
					string field = "label";
					proxyLabel = "VOD-Proxy MediaItem to YouTube (" + youTubeConfigurationLabel + ")";
					proxyRoot[field] = proxyLabel;

					field = "type";
					proxyRoot[field] = "VOD-Proxy";

					vodProxyParametersRoot = parametersRoot;
					{
						field = "YouTubeConfigurationLabel";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "title";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "Description";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "SourceType";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "internalMMS";
						if (JSONUtils::isMetadataPresent(vodProxyParametersRoot, field))
							vodProxyParametersRoot.erase(field);
					}
					{
						field = "youTubeSchedule";
						if (JSONUtils::isMetadataPresent(vodProxyParametersRoot, field))
							vodProxyParametersRoot.erase(field);
					}

					{
						bool timePeriod = true;
						field = "timePeriod";
						vodProxyParametersRoot[field] = timePeriod;

						field = "schedule";
						vodProxyParametersRoot[field] = scheduleRoot;
					}

					field = "references";
					vodProxyParametersRoot[field] = referencesRoot;

					json outputsRoot = json::array();
					{
						// we will create/modify the RTMP_Channel using
						// youTubeConfigurationLabel as his label
						try
						{
							bool warningIfMissing = true;
							tuple<int64_t, string, string, string, string, string> rtmpChannelDetails =
								_mmsEngineDBFacade->getRTMPChannelDetails(workspace->_workspaceKey, youTubeConfigurationLabel, warningIfMissing);

							int64_t confKey;
							tie(confKey, ignore, ignore, ignore, ignore, ignore) = rtmpChannelDetails;

							_mmsEngineDBFacade->modifyRTMPChannelConf(
								confKey, workspace->_workspaceKey, youTubeConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}
						catch (DBRecordNotFound &e)
						{
							_mmsEngineDBFacade->addRTMPChannelConf(
								workspace->_workspaceKey, youTubeConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}

						json outputRoot;

						field = "outputType";
						outputRoot[field] = "RTMP_Channel";

						field = "rtmpChannelConfigurationLabel";
						outputRoot[field] = youTubeConfigurationLabel;

						outputsRoot.push_back(outputRoot);
					}
					field = "outputs";
					vodProxyParametersRoot[field] = outputsRoot;
				}
				string field = "parameters";
				proxyRoot[field] = vodProxyParametersRoot;
			}

			json workflowRoot;
			{
				string field = "label";
				workflowRoot[field] = ingestionJobLabel + ". " + proxyLabel;

				field = "type";
				workflowRoot[field] = "Workflow";

				field = "task";
				workflowRoot[field] = proxyRoot;
			}

			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		vector<string> otherHeaders;
		MMSCURL::httpPostString(
			_logger, ingestionJobKey, _mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, to_string(userKey), apiKey, workflowMetadata,
			"application/json", // contentType
			otherHeaders
		);

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
			string() + "youTubeLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
			string() + "youTubeLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

void MMSEngineProcessor::facebookLiveBroadcastThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string ingestionJobLabel, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	try
	{
		SPDLOG_INFO(
			string() + "facebookLiveBroadcastThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string facebookConfigurationLabel;
		string facebookNodeType;
		string facebookNodeId;
		string title;
		string description;
		string facebookLiveType;

		json scheduleRoot;
		int64_t utcScheduleStartTimeInSeconds;
		string sourceType;
		// configurationLabel or referencesRoot has to be present
		string configurationLabel;
		json referencesRoot;
		{
			string field = "facebookNodeType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookNodeType = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookNodeId";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookNodeId = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookLiveType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookLiveType = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookConfigurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookConfigurationLabel = JSONUtils::asString(parametersRoot, field, "");

			field = "title";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			title = JSONUtils::asString(parametersRoot, field, "");

			field = "description";
			description = JSONUtils::asString(parametersRoot, field, "");

			field = "facebookSchedule";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			scheduleRoot = parametersRoot[field];

			field = "start";
			if (!JSONUtils::isMetadataPresent(scheduleRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string scheduleStartTimeInSeconds = JSONUtils::asString(scheduleRoot, field, "");
			utcScheduleStartTimeInSeconds = DateTime::sDateSecondsToUtc(scheduleStartTimeInSeconds);

			field = "sourceType";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceType = JSONUtils::asString(parametersRoot, field, "");

			if (sourceType == "Live")
			{
				field = "configurationLabel";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				configurationLabel = JSONUtils::asString(parametersRoot, field, "");
			}
			else // if (sourceType == "MediaItem")
			{
				field = "references";
				if (!JSONUtils::isMetadataPresent(parametersRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" +
										  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + field;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				referencesRoot = parametersRoot[field];
			}
		}

		string facebookToken;
		if (facebookNodeType == "Page")
			facebookToken = getFacebookPageToken(ingestionJobKey, workspace, facebookConfigurationLabel, facebookNodeId);
		else // if (facebookNodeType == "User")
			facebookToken = _mmsEngineDBFacade->getFacebookUserAccessTokenByConfigurationLabel(workspace->_workspaceKey, facebookConfigurationLabel);
		/* 2023-01-08: capire se bisogna recuperare un altro tipo di token
		else if (facebookDestination == "Event")
		{
		}
		else // if (facebookDestination == "Group")
		{
		}
		*/

		string facebookURL;
		json responseRoot;
		string rtmpURL;
		try
		{
			/*
				curl -i -X POST \
					"https://graph.facebook.com/{page-id}/live_videos
					?status=LIVE_NOW
					&title=Today%27s%20Page%20Live%20Video
					&description=This%20is%20the%20live%20video%20for%20the%20Page%20for%20today
					&access_token=EAAC..."

				curl -i -X POST \
					"https://graph.facebook.com/{page-id}/live_videos
					?status=SCHEDULED_UNPUBLISHED
					&planned_start_time=1541539800
					&access_token={access-token}"
			*/
			facebookURL = _facebookGraphAPIProtocol + "://" + _facebookGraphAPIHostName + ":" + to_string(_facebookGraphAPIPort) + "/" +
						  _facebookGraphAPIVersion + regex_replace(_facebookGraphAPILiveVideosURI, regex("__NODEID__"), facebookNodeId) +
						  "?title=" + curlpp::escape(title) + (description != "" ? ("&description=" + curlpp::escape(description)) : "") +
						  "&access_token=" + curlpp::escape(facebookToken);
			if (facebookLiveType == "LiveNow")
			{
				facebookURL += "&status=LIVE_NOW";
			}
			else
			{
				facebookURL += (string("&status=SCHEDULED_UNPUBLISHED") + "&planned_start_time=" + to_string(utcScheduleStartTimeInSeconds));
			}

			SPDLOG_INFO(string() + "create a Live Video object" + ", facebookURL: " + facebookURL);

			vector<string> otherHeaders;
			json responseRoot =
				MMSCURL::httpPostStringAndGetJson(_logger, ingestionJobKey, facebookURL, _mmsAPITimeoutInSeconds, "", "", "", "", otherHeaders);

			/*
				{
				"id": "1953020644813108",
				"stream_url": "rtmp://rtmp-api.facebook...",
				"secure_stream_url":"rtmps://rtmp-api.facebook..."
				}
			*/

			string field = "secure_stream_url";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field + ", response: " + JSONUtils::toString(responseRoot);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			rtmpURL = JSONUtils::asString(responseRoot, field, "");
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("Facebook live broadcast management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", facebookURL: " + facebookURL +
								  ", response: " + JSONUtils::toString(responseRoot) + ", e.what(): " + e.what();
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("Facebook live broadcast management failed") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", facebookURL: " + facebookURL +
								  ", response: " + JSONUtils::toString(responseRoot);
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}

		SPDLOG_INFO(
			string() + "Preparing workflow to ingest..." + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", rtmpURL: " + rtmpURL
		);

		json facebookLiveBroadcastOnSuccess = nullptr;
		json facebookLiveBroadcastOnError = nullptr;
		json facebookLiveBroadcastOnComplete = nullptr;
		int64_t userKey;
		string apiKey;
		{
			string field = "internalMMS";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				json internalMMSRoot = parametersRoot[field];

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
						facebookLiveBroadcastOnSuccess = eventsRoot[field];

					field = "onError";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						facebookLiveBroadcastOnError = eventsRoot[field];

					field = "onComplete";
					if (JSONUtils::isMetadataPresent(eventsRoot, field))
						facebookLiveBroadcastOnComplete = eventsRoot[field];
				}
			}
		}

		// create workflow to ingest
		string workflowMetadata;
		{
			string proxyLabel;
			json proxyRoot;

			if (sourceType == "Live")
			{
				json liveProxyParametersRoot;
				{
					string field = "label";
					proxyLabel = "Proxy " + configurationLabel + " to Facebook (" + facebookConfigurationLabel + ")";
					proxyRoot[field] = proxyLabel;

					field = "type";
					proxyRoot[field] = "Live-Proxy";

					liveProxyParametersRoot = parametersRoot;
					{
						field = "facebookConfigurationLabel";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "title";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "description";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "sourceType";
						liveProxyParametersRoot.erase(field);
					}
					{
						field = "internalMMS";
						if (JSONUtils::isMetadataPresent(liveProxyParametersRoot, field))
							liveProxyParametersRoot.erase(field);
					}
					{
						field = "facebookSchedule";
						if (JSONUtils::isMetadataPresent(liveProxyParametersRoot, field))
							liveProxyParametersRoot.erase(field);
					}

					{
						bool timePeriod = true;
						field = "timePeriod";
						liveProxyParametersRoot[field] = timePeriod;

						if (facebookLiveType == "LiveNow")
						{
							string sNow;
							{
								tm tmDateTime;
								char strDateTime[64];

								chrono::system_clock::time_point now = chrono::system_clock::now();
								time_t utcNow = chrono::system_clock::to_time_t(now);

								gmtime_r(&utcNow, &tmDateTime);
								sprintf(
									strDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1,
									tmDateTime.tm_mday, tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
								);
								sNow = strDateTime;
							}

							field = "start";
							scheduleRoot[field] = sNow;
						}

						field = "schedule";
						liveProxyParametersRoot[field] = scheduleRoot;
					}

					json outputsRoot = json::array();
					{
						// we will create/modify the RTMP_Channel using
						// facebookConfigurationLabel as his label
						try
						{
							bool warningIfMissing = true;
							tuple<int64_t, string, string, string, string, string> rtmpChannelDetails =
								_mmsEngineDBFacade->getRTMPChannelDetails(workspace->_workspaceKey, facebookConfigurationLabel, warningIfMissing);

							int64_t confKey;
							tie(confKey, ignore, ignore, ignore, ignore, ignore) = rtmpChannelDetails;

							_mmsEngineDBFacade->modifyRTMPChannelConf(
								confKey, workspace->_workspaceKey, facebookConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}
						catch (DBRecordNotFound &e)
						{
							_mmsEngineDBFacade->addRTMPChannelConf(
								workspace->_workspaceKey, facebookConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}

						json outputRoot;

						field = "outputType";
						outputRoot[field] = "RTMP_Channel";

						field = "rtmpChannelConfigurationLabel";
						outputRoot[field] = facebookConfigurationLabel;

						outputsRoot.push_back(outputRoot);
					}
					field = "outputs";
					liveProxyParametersRoot[field] = outputsRoot;
				}
				string field = "parameters";
				proxyRoot[field] = liveProxyParametersRoot;
			}
			else // if (sourceType == "MediaItem")
			{
				json vodProxyParametersRoot;
				{
					string field = "label";
					proxyLabel = "Proxy MediaItem to Facebook (" + facebookConfigurationLabel + ")";
					proxyRoot[field] = proxyLabel;

					field = "type";
					proxyRoot[field] = "VOD-Proxy";

					vodProxyParametersRoot = parametersRoot;
					{
						field = "facebookConfigurationLabel";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "title";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "description";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "sourceType";
						vodProxyParametersRoot.erase(field);
					}
					{
						field = "internalMMS";
						if (JSONUtils::isMetadataPresent(vodProxyParametersRoot, field))
							vodProxyParametersRoot.erase(field);
					}
					{
						field = "facebookSchedule";
						if (JSONUtils::isMetadataPresent(vodProxyParametersRoot, field))
							vodProxyParametersRoot.erase(field);
					}

					field = "references";
					vodProxyParametersRoot[field] = referencesRoot;

					{
						bool timePeriod = true;
						field = "timePeriod";
						vodProxyParametersRoot[field] = timePeriod;

						if (facebookLiveType == "LiveNow")
						{
							field = "start";
							scheduleRoot[field] = utcScheduleStartTimeInSeconds;
						}

						field = "schedule";
						vodProxyParametersRoot[field] = scheduleRoot;
					}

					json outputsRoot = json::array();
					{
						// we will create/modify the RTMP_Channel using
						// facebookConfigurationLabel as his label
						try
						{
							bool warningIfMissing = true;
							tuple<int64_t, string, string, string, string, string> rtmpChannelDetails =
								_mmsEngineDBFacade->getRTMPChannelDetails(workspace->_workspaceKey, facebookConfigurationLabel, warningIfMissing);

							int64_t confKey;
							tie(confKey, ignore, ignore, ignore, ignore, ignore) = rtmpChannelDetails;

							_mmsEngineDBFacade->modifyRTMPChannelConf(
								confKey, workspace->_workspaceKey, facebookConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}
						catch (DBRecordNotFound &e)
						{
							_mmsEngineDBFacade->addRTMPChannelConf(
								workspace->_workspaceKey, facebookConfigurationLabel, rtmpURL, "", "", "", "", "DEDICATED"
							);
						}

						json outputRoot;

						field = "outputType";
						outputRoot[field] = "RTMP_Channel";

						field = "rtmpChannelConfigurationLabel";
						outputRoot[field] = facebookConfigurationLabel;

						outputsRoot.push_back(outputRoot);
					}
					field = "outputs";
					vodProxyParametersRoot[field] = outputsRoot;
				}
				string field = "parameters";
				proxyRoot[field] = vodProxyParametersRoot;
			}

			json workflowRoot;
			{
				string field = "label";
				workflowRoot[field] = ingestionJobLabel + ". " + proxyLabel;

				field = "type";
				workflowRoot[field] = "Workflow";

				field = "task";
				workflowRoot[field] = proxyRoot;
			}

			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		vector<string> otherHeaders;
		MMSCURL::httpPostString(
			_logger, ingestionJobKey, _mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, to_string(userKey), apiKey, workflowMetadata,
			"application/json", // contentType
			otherHeaders
		);

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
			string() + "facebookLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
			string() + "facebookLiveBroadcastThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

void MMSEngineProcessor::postVideoOnFacebook(
	string mmsAssetPathName, int64_t sizeInBytes, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string facebookConfigurationLabel,
	string facebookDestination, string facebookNodeId
)
{

	string facebookURL;
	string sResponse;

	try
	{
		SPDLOG_INFO(
			string() + "postVideoOnFacebook" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName + ", sizeInBytes: " + to_string(sizeInBytes) +
			", facebookDestination: " + facebookDestination + ", facebookConfigurationLabel: " + facebookConfigurationLabel
		);

		string facebookToken;
		if (facebookDestination == "Page")
			facebookToken = getFacebookPageToken(ingestionJobKey, workspace, facebookConfigurationLabel, facebookNodeId);
		else // if (facebookDestination == "User")
			facebookToken = _mmsEngineDBFacade->getFacebookUserAccessTokenByConfigurationLabel(workspace->_workspaceKey, facebookConfigurationLabel);
		/* 2023-01-08: capire se bisogna recuperare un altro tipo di token
		else if (facebookDestination == "Event")
		{
		}
		else // if (facebookDestination == "Group")
		{
		}
		*/

		string fileFormat;
		{
			size_t extensionIndex = mmsAssetPathName.find_last_of(".");
			if (extensionIndex == string::npos)
			{
				string errorMessage = string() +
									  "No fileFormat (extension of the file) found in "
									  "mmsAssetPathName" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			fileFormat = mmsAssetPathName.substr(extensionIndex + 1);
		}

		/*
			details:
		   https://developers.facebook.com/docs/video-api/guides/publishing/

			curl \
				-X POST
		   "https://graph-video.facebook.com/v2.3/1533641336884006/videos"  \
				-F "access_token=XXXXXXXXX" \
				-F "upload_phase=start" \
				-F "file_size=152043520"

				{"upload_session_id":"1564747013773438","video_id":"1564747010440105","start_offset":"0","end_offset":"52428800"}
		*/
		string uploadSessionId;
		string videoId;
		int64_t startOffset;
		int64_t endOffset;
		// start
		{
			string facebookURI = string("/") + _facebookGraphAPIVersion + "/" + facebookNodeId + "/videos";

			facebookURL = _facebookGraphAPIProtocol + "://" + _facebookGraphAPIVideoHostName + ":" + to_string(_facebookGraphAPIPort) + facebookURI;

			vector<pair<string, string>> formData;
			formData.push_back(make_pair("access_token", facebookToken));
			formData.push_back(make_pair("upload_phase", "start"));
			formData.push_back(make_pair("file_size", to_string(sizeInBytes)));

			json facebookResponseRoot =
				MMSCURL::httpPostFormDataAndGetJson(_logger, ingestionJobKey, facebookURL, formData, _facebookGraphAPITimeoutInSeconds);

			string field = "upload_session_id";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage =
					string() + "Field into the response is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			uploadSessionId = JSONUtils::asString(facebookResponseRoot, field, "");

			field = "video_id";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage =
					string() + "Field into the response is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			videoId = JSONUtils::asString(facebookResponseRoot, field, "");

			field = "start_offset";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage =
					string() + "Field into the response is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sStartOffset = JSONUtils::asString(facebookResponseRoot, field, "");
			startOffset = stoll(sStartOffset);

			field = "end_offset";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage =
					string() + "Field into the response is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sEndOffset = JSONUtils::asString(facebookResponseRoot, field, "");
			endOffset = stoll(sEndOffset);
		}

		while (startOffset < endOffset)
		{
			/*
				curl \
					-X POST
			   "https://graph-video.facebook.com/v2.3/1533641336884006/videos" \
					-F "access_token=XXXXXXX" \
					-F "upload_phase=transfer" \
					-F “start_offset=0" \
					-F "upload_session_id=1564747013773438" \
					-F "video_file_chunk=@chunk1.mp4"
			*/
			// transfer
			{
				string facebookURI = string("/") + _facebookGraphAPIVersion + "/" + facebookNodeId + "/videos";

				facebookURL =
					_facebookGraphAPIProtocol + "://" + _facebookGraphAPIVideoHostName + ":" + to_string(_facebookGraphAPIPort) + facebookURI;

				string mediaContentType = string("video") + "/" + fileFormat;

				vector<pair<string, string>> formData;
				formData.push_back(make_pair("access_token", facebookToken));
				formData.push_back(make_pair("upload_phase", "transfer"));
				formData.push_back(make_pair("start_offset", to_string(startOffset)));
				formData.push_back(make_pair("upload_session_id", uploadSessionId));

				json facebookResponseRoot = MMSCURL::httpPostFileByFormDataAndGetJson(
					_logger, ingestionJobKey, facebookURL, formData, _facebookGraphAPITimeoutInSeconds, mmsAssetPathName, sizeInBytes,
					mediaContentType,
					1,	// maxRetryNumber
					15, // secondsToWaitBeforeToRetry
					startOffset, endOffset
				);

				string field = "start_offset";
				if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field +
										  ", facebookResponseRoot: " + JSONUtils::toString(facebookResponseRoot);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				string sStartOffset = JSONUtils::asString(facebookResponseRoot, field, "");
				startOffset = stoll(sStartOffset);

				field = "end_offset";
				if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
				{
					string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				string sEndOffset = JSONUtils::asString(facebookResponseRoot, field, "");
				endOffset = stoll(sEndOffset);
			}
		}

		/*
			curl \
				-X POST
		   "https://graph-video.facebook.com/v2.3/1533641336884006/videos"  \
				-F "access_token=XXXXXXXX" \
				-F "upload_phase=finish" \
				-F "upload_session_id=1564747013773438"

			{"success":true}
		*/
		// finish: pubblica il video e mettilo in coda per la codifica asincrona
		bool success;
		{
			string facebookURI = string("/") + _facebookGraphAPIVersion + "/" + facebookNodeId + "/videos";

			facebookURL = _facebookGraphAPIProtocol + "://" + _facebookGraphAPIVideoHostName + ":" + to_string(_facebookGraphAPIPort) + facebookURI;

			vector<pair<string, string>> formData;
			formData.push_back(make_pair("access_token", facebookToken));
			formData.push_back(make_pair("upload_phase", "finish"));
			formData.push_back(make_pair("upload_session_id", uploadSessionId));

			json facebookResponseRoot =
				MMSCURL::httpPostFormDataAndGetJson(_logger, ingestionJobKey, facebookURL, formData, _facebookGraphAPITimeoutInSeconds);

			string field = "success";
			if (!JSONUtils::isMetadataPresent(facebookResponseRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field + ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			success = JSONUtils::asBool(facebookResponseRoot, field, false);

			if (!success)
			{
				string errorMessage = string() + "Post Video on Facebook failed" + ", Field: " + field + ", success: " + to_string(success) +
									  ", sResponse: " + sResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (runtime_error e)
	{
		string errorMessage = string() + "Post video on Facebook failed (runtime_error)" + ", facebookURL: " + facebookURL +
							  ", exception: " + e.what() + ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string() + "Post video on Facebook failed (exception)" + ", facebookURL: " + facebookURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

size_t curlUploadVideoOnYouTubeCallback(char *ptr, size_t size, size_t nmemb, void *f)
{
	MMSEngineProcessor::CurlUploadYouTubeData *curlUploadData = (MMSEngineProcessor::CurlUploadYouTubeData *)f;

	auto logger = spdlog::get("mmsEngineService");

	int64_t currentFilePosition = curlUploadData->mediaSourceFileStream.tellg();

	/*
	logger->info(string() + "curlUploadVideoOnYouTubeCallback"
		+ ", currentFilePosition: " + to_string(currentFilePosition)
		+ ", size: " + to_string(size)
		+ ", nmemb: " + to_string(nmemb)
		+ ", curlUploadData->fileSizeInBytes: " +
	to_string(curlUploadData->fileSizeInBytes)
	);
	*/

	if (currentFilePosition + (size * nmemb) <= curlUploadData->fileSizeInBytes)
		curlUploadData->mediaSourceFileStream.read(ptr, size * nmemb);
	else
		curlUploadData->mediaSourceFileStream.read(ptr, curlUploadData->fileSizeInBytes - currentFilePosition);

	int64_t charsRead = curlUploadData->mediaSourceFileStream.gcount();

	return charsRead;
};

void MMSEngineProcessor::postVideoOnYouTube(
	string mmsAssetPathName, int64_t sizeInBytes, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string youTubeConfigurationLabel,
	string youTubeTitle, string youTubeDescription, json youTubeTags, int youTubeCategoryId, string youTubePrivacy, bool youTubeMadeForKids
)
{

	string youTubeURL;
	string youTubeUploadURL;
	string sResponse;

	try
	{
		SPDLOG_INFO(
			string() + "postVideoOnYouTubeThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count()) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName + ", sizeInBytes: " + to_string(sizeInBytes) +
			", youTubeConfigurationLabel: " + youTubeConfigurationLabel + ", youTubeTitle: " + youTubeTitle +
			", youTubeDescription: " + youTubeDescription + ", youTubeCategoryId: " + to_string(youTubeCategoryId) +
			", youTubePrivacy: " + youTubePrivacy + ", youTubeMadeForKids: " + to_string(youTubeMadeForKids)
		);

		// 1. get refresh_token from the configuration
		// 2. call google API
		// 3. the response will have the access token to be used
		string youTubeAccessToken = getYouTubeAccessTokenByConfigurationLabel(ingestionJobKey, workspace, youTubeConfigurationLabel);

		string fileFormat;
		{
			size_t extensionIndex = mmsAssetPathName.find_last_of(".");
			if (extensionIndex == string::npos)
			{
				string errorMessage = string() +
									  "No fileFormat (extension of the file) found in "
									  "mmsAssetPathName" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			fileFormat = mmsAssetPathName.substr(extensionIndex + 1);
		}

		/*
			POST
		   /upload/youtube/v3/videos?uploadType=resumable&part=snippet,status,contentDetails
		   HTTP/1.1 Host: www.googleapis.com Authorization: Bearer AUTH_TOKEN
			Content-Length: 278
			Content-Type: application/json; charset=UTF-8
			X-Upload-Content-Length: 3000000
			X-Upload-Content-Type: video/*

			{
			  "snippet": {
				"title": "My video title",
				"description": "This is a description of my video",
				"tags": ["cool", "video", "more keywords"],
				"categoryId": 22
			  },
			  "status": {
				"privacyStatus": "public",
				"embeddable": True,
				"license": "youtube"
			  }
			}

			HTTP/1.1 200 OK
			Location:
		   https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&upload_id=xa298sd_f&part=snippet,status,contentDetails
			Content-Length: 0
		*/
		string videoContentType = "video/*";
		{
			youTubeURL =
				_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + _youTubeDataAPIUploadVideoURI;

			string body;
			{
				json bodyRoot;
				json snippetRoot;

				string field = "title";
				snippetRoot[field] = youTubeTitle;

				if (youTubeDescription != "")
				{
					field = "description";
					snippetRoot[field] = youTubeDescription;
				}

				if (youTubeTags != nullptr)
				{
					field = "tags";
					snippetRoot[field] = youTubeTags;
				}

				if (youTubeCategoryId != -1)
				{
					field = "categoryId";
					snippetRoot[field] = youTubeCategoryId;
				}

				field = "snippet";
				bodyRoot[field] = snippetRoot;

				json statusRoot;

				field = "privacyStatus";
				statusRoot[field] = youTubePrivacy;

				field = "selfDeclaredMadeForKids";
				statusRoot[field] = youTubeMadeForKids;

				field = "embeddable";
				statusRoot[field] = true;

				// field = "license";
				// statusRoot[field] = "youtube";

				field = "status";
				bodyRoot[field] = statusRoot;

				body = JSONUtils::toString(bodyRoot);
			}

			vector<string> headerList;
			{
				string header = "Authorization: Bearer " + youTubeAccessToken;
				headerList.push_back(header);

				header = "Content-Length: " + to_string(body.length());
				headerList.push_back(header);

				header = "X-Upload-Content-Length: " + to_string(sizeInBytes);
				headerList.push_back(header);

				header = string("X-Upload-Content-Type: ") + videoContentType;
				headerList.push_back(header);
			}

			pair<string, string> responseDetails = MMSCURL::httpPostString(
				_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
				"application/json; charset=UTF-8", // contentType
				headerList
			);

			string sHeaderResponse;
			string sBodyResponse;

			tie(sHeaderResponse, sBodyResponse) = responseDetails;

			if (sHeaderResponse.find("Location: ") == string::npos && sHeaderResponse.find("location: ") == string::npos)
			{
				string errorMessage = string() + "'Location' response header is not present" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", youTubeURL: " + youTubeURL + ", sHeaderResponse: " + sHeaderResponse;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			/* sResponse:
				HTTP/1.1 200 OK
				X-GUploader-UploadID:
			   AEnB2UqO5ml7GRPs5AjsOSPzSGwudclcEFbyXtEK_TLWRhggwxh9gTWBdusefTgmX2ul9axk4ztG_YBWQXGtm1M42Fz9QVE4xA
				Location:
			   https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&part=snippet,status,contentDetails&upload_id=AEnB2UqO5ml7GRPs5AjsOSPzSGwudclcEFbyXtEK_TLWRhggwxh9gTWBdusefTgmX2ul9axk4ztG_YBWQXGtm1M42Fz9QVE4xA
				ETag: "XI7nbFXulYBIpL0ayR_gDh3eu1k/bpNRC6h7Ng2_S5XJ6YzbSMF0qXE"
				Vary: Origin
				Vary: X-Origin
				X-Goog-Correlation-Id: FGN7H2Vxp5I
				Cache-Control: no-cache, no-store, max-age=0, must-revalidate
				Pragma: no-cache
				Expires: Mon, 01 Jan 1990 00:00:00 GMT
				Date: Sun, 09 Dec 2018 09:15:41 GMT
				Content-Length: 0
				Server: UploadServer
				Content-Type: text/html; charset=UTF-8
				Alt-Svc: quic=":443"; ma=2592000; v="44,43,39,35"
			 */

			int locationStartIndex = sHeaderResponse.find("Location: ");
			if (locationStartIndex == string::npos)
				locationStartIndex = sHeaderResponse.find("location: ");
			locationStartIndex += string("Location: ").length();
			int locationEndIndex = sHeaderResponse.find("\r", locationStartIndex);
			if (locationEndIndex == string::npos)
				locationEndIndex = sHeaderResponse.find("\n", locationStartIndex);
			if (locationEndIndex == string::npos)
				youTubeUploadURL = sHeaderResponse.substr(locationStartIndex);
			else
				youTubeUploadURL = sHeaderResponse.substr(locationStartIndex, locationEndIndex - locationStartIndex);
		}

		bool contentCompletelyUploaded = false;
		CurlUploadYouTubeData curlUploadData;
		curlUploadData.mediaSourceFileStream.open(mmsAssetPathName, ios::binary);
		curlUploadData.lastByteSent = -1;
		curlUploadData.fileSizeInBytes = sizeInBytes;
		while (!contentCompletelyUploaded)
		{
			/*
				// In case of the first request
				PUT UPLOAD_URL HTTP/1.1
				Authorization: Bearer AUTH_TOKEN
				Content-Length: CONTENT_LENGTH
				Content-Type: CONTENT_TYPE

				BINARY_FILE_DATA

				// in case of resuming
				PUT UPLOAD_URL HTTP/1.1
				Authorization: Bearer AUTH_TOKEN
				Content-Length: REMAINING_CONTENT_LENGTH
				Content-Range: bytes FIRST_BYTE-LAST_BYTE/TOTAL_CONTENT_LENGTH

				PARTIAL_BINARY_FILE_DATA
			*/

			{
				list<string> headerList;
				headerList.push_back(string("Authorization: Bearer ") + youTubeAccessToken);
				if (curlUploadData.lastByteSent == -1)
					headerList.push_back(string("Content-Length: ") + to_string(sizeInBytes));
				else
					headerList.push_back(string("Content-Length: ") + to_string(sizeInBytes - curlUploadData.lastByteSent + 1));
				if (curlUploadData.lastByteSent == -1)
					headerList.push_back(string("Content-Type: ") + videoContentType);
				else
					headerList.push_back(
						string("Content-Range: bytes ") + to_string(curlUploadData.lastByteSent) + "-" + to_string(sizeInBytes - 1) + "/" +
						to_string(sizeInBytes)
					);

				curlpp::Cleanup cleaner;
				curlpp::Easy request;

				{
					curlpp::options::ReadFunctionCurlFunction curlUploadCallbackFunction(curlUploadVideoOnYouTubeCallback);
					curlpp::OptionTrait<void *, CURLOPT_READDATA> curlUploadDataData(&curlUploadData);
					request.setOpt(curlUploadCallbackFunction);
					request.setOpt(curlUploadDataData);

					bool upload = true;
					request.setOpt(new curlpp::options::Upload(upload));
				}

				request.setOpt(new curlpp::options::CustomRequest{"PUT"});
				request.setOpt(new curlpp::options::Url(youTubeUploadURL));
				request.setOpt(new curlpp::options::Timeout(_youTubeDataAPITimeoutInSecondsForUploadVideo));

				if (_youTubeDataAPIProtocol == "https")
				{
					//                typedef curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLCERTPASSWD> SslCertPasswd;
					//                typedef curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLKEY> SslKey; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLKEYTYPE> SslKeyType; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLKEYPASSWD> SslKeyPasswd;
					//                typedef curlpp::OptionTrait<std::string,
					//                CURLOPT_SSLENGINE> SslEngine; typedef
					//                curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
					//                SslEngineDefault; typedef
					//                curlpp::OptionTrait<long,
					//                CURLOPT_SSLVERSION> SslVersion; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_CAINFO> CaInfo; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_CAPATH> CaPath; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_RANDOM_FILE> RandomFile; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_EGDSOCKET> EgdSocket; typedef
					//                curlpp::OptionTrait<std::string,
					//                CURLOPT_SSL_CIPHER_LIST> SslCipherList;
					//                typedef curlpp::OptionTrait<std::string,
					//                CURLOPT_KRB4LEVEL> Krb4Level;

					// cert is stored PEM coded in file...
					// since PEM is default, we needn't set it for PEM
					// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
					// curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE>
					// sslCertType("PEM"); equest.setOpt(sslCertType);

					// set the cert for client authentication
					// "testcert.pem"
					// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
					// curlpp::OptionTrait<string, CURLOPT_SSLCERT>
					// sslCert("cert.pem"); request.setOpt(sslCert);

					// sorry, for engine we must set the passphrase
					//   (if the key has one...)
					// const char *pPassphrase = NULL;
					// if(pPassphrase)
					//  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

					// if we use a key stored in a crypto engine,
					//   we must set the key type to "ENG"
					// pKeyType  = "PEM";
					// curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

					// set the private key (file or ID in engine)
					// pKeyName  = "testkey.pem";
					// curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

					// set the file with the certs vaildating the server
					// *pCACertFile = "cacert.pem";
					// curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

					// disconnect if we can't validate server's cert
					bool bSslVerifyPeer = false;
					curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
					request.setOpt(sslVerifyPeer);

					curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
					request.setOpt(sslVerifyHost);

					// request.setOpt(new curlpp::options::SslEngineDefault());
				}

				for (string headerMessage : headerList)
					SPDLOG_INFO(string() + "Adding header message: " + headerMessage);
				request.setOpt(new curlpp::options::HttpHeader(headerList));

				SPDLOG_INFO(
					string() + "Calling youTube (upload)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", youTubeUploadURL: " + youTubeUploadURL
				);
				request.perform();

				long responseCode = curlpp::infos::ResponseCode::get(request);

				SPDLOG_INFO(
					string() + "Called youTube (upload)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
				);

				if (responseCode == 200 || responseCode == 201)
				{
					SPDLOG_INFO(
						string() + "youTube upload successful" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
					);

					contentCompletelyUploaded = true;
				}
				else if (responseCode == 500 || responseCode == 502 || responseCode == 503 || responseCode == 504)
				{
					_logger->warn(
						string() + "youTube upload failed, trying to resume" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
					);

					/*
						PUT UPLOAD_URL HTTP/1.1
						Authorization: Bearer AUTH_TOKEN
						Content-Length: 0
						Content-Range: bytes *\/CONTENT_LENGTH

						308 Resume Incomplete
						Content-Length: 0
						Range: bytes=0-999999
					*/
					{
						list<string> headerList;
						headerList.push_back(string("Authorization: Bearer ") + youTubeAccessToken);
						headerList.push_back(string("Content-Length: 0"));
						headerList.push_back(string("Content-Range: bytes */") + to_string(sizeInBytes));

						curlpp::Cleanup cleaner;
						curlpp::Easy request;

						request.setOpt(new curlpp::options::CustomRequest{"PUT"});
						request.setOpt(new curlpp::options::Url(youTubeUploadURL));
						request.setOpt(new curlpp::options::Timeout(_youTubeDataAPITimeoutInSeconds));

						if (_youTubeDataAPIProtocol == "https")
						{
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLCERTPASSWD>
							//                SslCertPasswd; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLKEY> SslKey; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLKEYTYPE> SslKeyType;
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLKEYPASSWD>
							//                SslKeyPasswd; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSLENGINE> SslEngine;
							//                typedef
							//                curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT>
							//                SslEngineDefault; typedef
							//                curlpp::OptionTrait<long,
							//                CURLOPT_SSLVERSION> SslVersion;
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_CAINFO> CaInfo; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_CAPATH> CaPath; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_RANDOM_FILE> RandomFile;
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_EGDSOCKET> EgdSocket;
							//                typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_SSL_CIPHER_LIST>
							//                SslCipherList; typedef
							//                curlpp::OptionTrait<std::string,
							//                CURLOPT_KRB4LEVEL> Krb4Level;

							// cert is stored PEM coded in file...
							// since PEM is default, we needn't set it for PEM
							// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE,
							// "PEM"); curlpp::OptionTrait<string,
							// CURLOPT_SSLCERTTYPE> sslCertType("PEM");
							// equest.setOpt(sslCertType);

							// set the cert for client authentication
							// "testcert.pem"
							// curl_easy_setopt(curl, CURLOPT_SSLCERT,
							// pCertFile); curlpp::OptionTrait<string,
							// CURLOPT_SSLCERT> sslCert("cert.pem");
							// request.setOpt(sslCert);

							// sorry, for engine we must set the passphrase
							//   (if the key has one...)
							// const char *pPassphrase = NULL;
							// if(pPassphrase)
							//  curl_easy_setopt(curl, CURLOPT_KEYPASSWD,
							//  pPassphrase);

							// if we use a key stored in a crypto engine,
							//   we must set the key type to "ENG"
							// pKeyType  = "PEM";
							// curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE,
							// pKeyType);

							// set the private key (file or ID in engine)
							// pKeyName  = "testkey.pem";
							// curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

							// set the file with the certs vaildating the server
							// *pCACertFile = "cacert.pem";
							// curl_easy_setopt(curl, CURLOPT_CAINFO,
							// pCACertFile);

							// disconnect if we can't validate server's cert
							bool bSslVerifyPeer = false;
							curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
							request.setOpt(sslVerifyPeer);

							curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
							request.setOpt(sslVerifyHost);

							// request.setOpt(new
							// curlpp::options::SslEngineDefault());
						}

						for (string headerMessage : headerList)
							SPDLOG_INFO(string() + "Adding header message: " + headerMessage);
						request.setOpt(new curlpp::options::HttpHeader(headerList));

						ostringstream response;
						request.setOpt(new curlpp::options::WriteStream(&response));

						// store response headers in the response
						// You simply have to set next option to prefix the
						// header to the normal body output.
						request.setOpt(new curlpp::options::Header(true));

						SPDLOG_INFO(
							string() + "Calling youTube check status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", youTubeUploadURL: " + youTubeUploadURL + ", _youTubeDataAPIProtocol: " + _youTubeDataAPIProtocol +
							", _youTubeDataAPIHostName: " + _youTubeDataAPIHostName + ", _youTubeDataAPIPort: " + to_string(_youTubeDataAPIPort)
						);
						request.perform();

						sResponse = response.str();
						long responseCode = curlpp::infos::ResponseCode::get(request);

						SPDLOG_INFO(
							string() + "Called youTube check status" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode) + ", sResponse: " + sResponse
						);

						if (responseCode != 308 || sResponse.find("Range: bytes=") == string::npos)
						{
							// error
							string errorMessage(
								string() + "youTube check status failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						/* sResponse:
							HTTP/1.1 308 Resume Incomplete
							X-GUploader-UploadID:
						   AEnB2Ur8jQ5DSbXieg8krXWg0f7Bmawvf6XTacURJ7wbITyXdTv8ZeHpepaUwh6F9DB5TvBCzoS4quZMKegyo2x7H9EJOc6ozQ
							Range: bytes=0-1572863
							X-Range-MD5: d50bc8fc7ecc41926f841085db3909b3
							Content-Length: 0
							Date: Mon, 10 Dec 2018 13:09:51 GMT
							Server: UploadServer
							Content-Type: text/html; charset=UTF-8
							Alt-Svc: quic=":443"; ma=2592000; v="44,43,39,35"
						*/
						int rangeStartIndex = sResponse.find("Range: bytes=");
						rangeStartIndex += string("Range: bytes=").length();
						int rangeEndIndex = sResponse.find("\r", rangeStartIndex);
						if (rangeEndIndex == string::npos)
							rangeEndIndex = sResponse.find("\n", rangeStartIndex);
						string rangeHeader;
						if (rangeEndIndex == string::npos)
							rangeHeader = sResponse.substr(rangeStartIndex);
						else
							rangeHeader = sResponse.substr(rangeStartIndex, rangeEndIndex - rangeStartIndex);

						int rangeStartOffsetIndex = rangeHeader.find("-");
						if (rangeStartOffsetIndex == string::npos)
						{
							// error
							string errorMessage(
								string() + "youTube check status failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								", youTubeUploadURL: " + youTubeUploadURL + ", rangeHeader: " + rangeHeader
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						SPDLOG_INFO(
							string() + "Resuming" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", youTubeUploadURL: " + youTubeUploadURL +
							", rangeHeader: " + rangeHeader +
							", rangeHeader.substr(rangeStartOffsetIndex + "
							"1): " +
							rangeHeader.substr(rangeStartOffsetIndex + 1)
						);
						curlUploadData.lastByteSent = stoll(rangeHeader.substr(rangeStartOffsetIndex + 1)) + 1;
						curlUploadData.mediaSourceFileStream.seekg(curlUploadData.lastByteSent, ios::beg);
					}
				}
				else
				{
					// error
					string errorMessage(
						string() + "youTube upload failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", youTubeUploadURL: " + youTubeUploadURL + ", responseCode: " + to_string(responseCode)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
	}
	catch (curlpp::LogicError &e)
	{
		string errorMessage = string() + "Post video on YouTube failed (LogicError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", youTubeUploadURL: " + youTubeUploadURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (curlpp::RuntimeError &e)
	{
		string errorMessage = string() + "Post video on YouTube failed (RuntimeError)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", youTubeUploadURL: " + youTubeUploadURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (runtime_error e)
	{
		string errorMessage = string() + "Post video on YouTube failed (runtime_error)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", youTubeUploadURL: " + youTubeUploadURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception e)
	{
		string errorMessage = string() + "Post video on YouTube failed (exception)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", youTubeUploadURL: " + youTubeUploadURL + ", exception: " + e.what() +
							  ", sResponse: " + sResponse;
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

string MMSEngineProcessor::getYouTubeAccessTokenByConfigurationLabel(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string youTubeConfigurationLabel
)
{
	string youTubeURL;
	string sResponse;

	try
	{
		tuple<string, string, string> youTubeDetails =
			_mmsEngineDBFacade->getYouTubeDetailsByConfigurationLabel(workspace->_workspaceKey, youTubeConfigurationLabel);

		string youTubeTokenType;
		string youTubeRefreshToken;
		string youTubeAccessToken;
		tie(youTubeTokenType, youTubeRefreshToken, youTubeAccessToken) = youTubeDetails;

		if (youTubeTokenType == "AccessToken")
		{
			SPDLOG_INFO(
				string() + "Using the youTube access token" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", youTubeAccessToken: " + youTubeAccessToken
			);

			return youTubeAccessToken;
		}

		youTubeURL =
			_youTubeDataAPIProtocol + "://" + _youTubeDataAPIHostName + ":" + to_string(_youTubeDataAPIPort) + _youTubeDataAPIRefreshTokenURI;

		string body = string("client_id=") + _youTubeDataAPIClientId + "&client_secret=" + _youTubeDataAPIClientSecret +
					  "&refresh_token=" + youTubeRefreshToken + "&grant_type=refresh_token";

		/*
		list<string> headerList;
		{
			// header = "Content-Length: " + to_string(body.length());
			// headerList.push_back(header);

			string header = "Content-Type: application/x-www-form-urlencoded";
			headerList.push_back(header);
		}
		*/

		vector<string> otherHeaders;
		json youTubeResponseRoot = MMSCURL::httpPostStringAndGetJson(
			_logger, ingestionJobKey, youTubeURL, _youTubeDataAPITimeoutInSeconds, "", "", body,
			"application/x-www-form-urlencoded", // contentType
			otherHeaders
		);

		/*
			{
			  "access_token":
		   "ya29.GlxvBv2JUSUGmxHncG7KK118PHh4IY3ce6hbSRBoBjeXMiZjD53y3ZoeGchIkyJMb2rwQHlp-tQUZcIJ5zrt6CL2iWj-fV_2ArlAOCTy8y2B0_3KeZrbbJYgoFXCYA",
			  "expires_in": 3600,
			  "scope": "https://www.googleapis.com/auth/youtube
		   https://www.googleapis.com/auth/youtube.upload", "token_type":
		   "Bearer"
			}
		*/

		string field = "access_token";
		if (!JSONUtils::isMetadataPresent(youTubeResponseRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		return JSONUtils::asString(youTubeResponseRoot, field, "");
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("youTube refresh token failed") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse + ", e.what(): " + e.what();
		SPDLOG_ERROR(string() + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		string errorMessage = string("youTube refresh token failed") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", youTubeURL: " + youTubeURL + ", sResponse: " + sResponse;
		SPDLOG_ERROR(string() + errorMessage);

		throw runtime_error(errorMessage);
	}
}

string MMSEngineProcessor::getFacebookPageToken(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string facebookConfigurationLabel, string facebookPageId
)
{
	string facebookURL;
	json responseRoot;

	try
	{
		string userAccessToken =
			_mmsEngineDBFacade->getFacebookUserAccessTokenByConfigurationLabel(workspace->_workspaceKey, facebookConfigurationLabel);

		// curl -i -X GET "https://graph.facebook.com/PAGE-ID?
		// fields=access_token&
		// access_token=USER-ACCESS-TOKEN"

		facebookURL = _facebookGraphAPIProtocol + "://" + _facebookGraphAPIHostName + ":" + to_string(_facebookGraphAPIPort) + "/" +
					  _facebookGraphAPIVersion + "/" + facebookPageId + "?fields=access_token" + "&access_token=" + curlpp::escape(userAccessToken);

		SPDLOG_INFO(string() + "Retrieve page token" + ", facebookURL: " + facebookURL);

		vector<string> otherHeaders;
		json responseRoot = MMSCURL::httpGetJson(_logger, ingestionJobKey, facebookURL, _mmsAPITimeoutInSeconds, "", "", otherHeaders);

		/*
		{
		"access_token":"PAGE-ACCESS-TOKEN",
		"id":"PAGE-ID"
		}
		*/

		string field = "access_token";
		if (!JSONUtils::isMetadataPresent(responseRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		return JSONUtils::asString(responseRoot, field, "");
	}
	catch (runtime_error &e)
	{
		string errorMessage = string("facebook access token failed") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", facebookURL: " + facebookURL + ", response: " + JSONUtils::toString(responseRoot) + ", e.what(): " + e.what();
		SPDLOG_ERROR(string() + errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		string errorMessage = string("facebook access token failed") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							  ", facebookURL: " + facebookURL + ", response: " + JSONUtils::toString(responseRoot);
		SPDLOG_ERROR(string() + errorMessage);

		throw runtime_error(errorMessage);
	}
}
