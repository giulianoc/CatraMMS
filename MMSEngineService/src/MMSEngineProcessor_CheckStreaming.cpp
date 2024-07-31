
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

void MMSEngineProcessor::checkStreamingThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "checkStreamingThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		SPDLOG_INFO(
			string() + "checkStreamingThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
			to_string(ingestionJobKey) + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
		);

		string field = "inputType";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string inputType = JSONUtils::asString(parametersRoot, field, "Stream");

		string streamingUrl;
		if (inputType == "Stream")
		{
			string field = "configurationLabel";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string configurationLabel = JSONUtils::asString(parametersRoot, field, "");

			/*
			bool warningIfMissing = false;
			tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
				ipChannelDetails = _mmsEngineDBFacade->getStreamDetails(workspace->_workspaceKey, configurationLabel, warningIfMissing);
			tie(ignore, streamSourceType, ignore, streamingUrl, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
				ignore, ignore, ignore, ignore) = ipChannelDetails;
			*/
			string streamSourceType;
			tie(streamSourceType, streamingUrl) = _mmsEngineDBFacade->stream_sourceTypeUrl(workspace->_workspaceKey, configurationLabel);
		}
		else
		{
			// StreamingName is mandatory even if it is not used here
			// It is mandatory because in case into the workflow we have the
			// EMail task, the Email task may need the StreamingName information
			// to add it into the email
			string field = "streamingName";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string streamingName = JSONUtils::asString(parametersRoot, field, "");

			field = "streamingUrl";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamingUrl = JSONUtils::asString(parametersRoot, field, "");
		}

		bool isVideo = JSONUtils::asBool(parametersRoot, "isVideo", true);

		SPDLOG_INFO(
			string() + "checkStreamingThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", _ingestionJobKey: " + to_string(ingestionJobKey) + ", inputType: " + inputType + ", streamingUrl: " + streamingUrl
		);

		if (streamingUrl == "")
		{
			string errorMessage = string() + "streamingUrl is wrong" + ", streamingUrl: " + streamingUrl;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			SPDLOG_INFO(
				"Calling ffmpeg.getMediaInfo"
				", processorIdentifier: {}"
				", ingestionJobKey: {}"
				", streamingUrl: {}",
				_processorIdentifier, ingestionJobKey, streamingUrl
			);
			int timeoutInSeconds = 20;
			bool isMMSAssetPathName = false;
			tuple<int64_t, long, json> mediaInfoDetails;
			vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
			vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
			FFMpeg ffmpeg(_configurationRoot, _logger);
			mediaInfoDetails = ffmpeg.getMediaInfo(ingestionJobKey, isMMSAssetPathName, timeoutInSeconds, streamingUrl, videoTracks, audioTracks);
			SPDLOG_INFO(
				"Called ffmpeg.getMediaInfo"
				", _processorIdentifier: {}"
				", _ingestionJobKey: {}"
				", streamingUrl: {}"
				", videoTracks.size: {}"
				", audioTracks.size: {}",
				_processorIdentifier, ingestionJobKey, streamingUrl, videoTracks.size(), audioTracks.size()
			);

			if (isVideo && (videoTracks.size() == 0 || audioTracks.size() == 0))
			{
				string errorMessage = fmt::format(
					"Video with wrong video or audio tracks"
					", processorIdentifier: {}"
					", ingestionJobKey: {}"
					", videoTracks.size: {}"
					", audioTracks.size: {}",
					_processorIdentifier, ingestionJobKey, videoTracks.size(), audioTracks.size()
				);
				SPDLOG_ERROR(errorMessage);
				throw runtime_error(errorMessage);
			}
			else if (!isVideo && (videoTracks.size() > 0 || audioTracks.size() == 0))
			{
				string errorMessage = fmt::format(
					"Audio with wrong video or audio tracks"
					", processorIdentifier: {}"
					", ingestionJobKey: {}"
					", videoTracks.size: {}"
					", audioTracks.size: {}",
					_processorIdentifier, ingestionJobKey, videoTracks.size(), audioTracks.size()
				);
				SPDLOG_ERROR(errorMessage);
				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			"Update IngestionJob"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", IngestionStatus: End_TaskSuccess"
			", errorMessage: ",
			_processorIdentifier, ingestionJobKey
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"checkStreamingThread failed"
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
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"checkStreamingThread failed"
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
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "checkStreamingThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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
	}
}
