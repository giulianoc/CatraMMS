
#include "FFMpegWrapper.h"
#include "JSONUtils.h"
#include "JsonPath.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;

void MMSEngineProcessor::checkStreamingThread(
	shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "checkStreamingThread", _processorIdentifier, _processorsThreadsNumber.use_count(), ingestionJobKey
	);

	try
	{
		LOG_INFO("checkStreamingThread"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", _processorsThreadsNumber.use_count(): ", _processorIdentifier, ingestionJobKey, _processorsThreadsNumber.use_count()
		);

		string field = "inputType";
		if (!JSONUtils::isPresent(parametersRoot, field))
		{
			string errorMessage = std::format("Field is not present or it is null"
				", Field: {}", field);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		auto inputType = JsonPath(&parametersRoot)[field].as<string>("Stream");

		string streamingUrl;
		if (inputType == "Stream")
		{
			string field = "configurationLabel";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = std::format("Field is not present or it is null"
					", Field: {}", field);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			auto configurationLabel = JsonPath(&parametersRoot)[field].as<string>("");

			tie(ignore, streamingUrl) = _mmsEngineDBFacade->stream_sourceTypeUrl(workspace->_workspaceKey, configurationLabel);
		}
		else
		{
			// StreamingName is mandatory even if it is not used here
			// It is mandatory because in case into the workflow we have the
			// EMail task, the Email task may need the StreamingName information
			// to add it into the email
			string field = "streamingName";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = std::format("Field is not present or it is null"
					", Field: {}", field);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			auto streamingName = JsonPath(&parametersRoot)[field].as<string>("");

			field = "streamingUrl";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = std::format("Field is not present or it is null"
					", Field: {}", field);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamingUrl = JsonPath(&parametersRoot)[field].as<string>("");
		}

		bool isVideo = JsonPath(&parametersRoot)["isVideo"].as<bool>(true);
		auto timeoutInSeconds = JsonPath(&parametersRoot)["timeoutInSeconds"].as<int32_t>(5);

		LOG_INFO("checkStreamingThread"
			", _processorIdentifier: {}"
			", _ingestionJobKey: {}"
			", inputType: {}"
			", streamingUrl: {}", _processorIdentifier, ingestionJobKey, inputType, streamingUrl
		);

		if (streamingUrl.empty())
		{
			string errorMessage = std::format("streamingUrl is wrong"
				", streamingUrl: {}", streamingUrl);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			LOG_INFO(
				"Calling ffmpeg.getMediaInfo"
				", processorIdentifier: {}"
				", ingestionJobKey: {}"
				", streamingUrl: {}",
				_processorIdentifier, ingestionJobKey, streamingUrl
			);
			bool isMMSAssetPathName = false;
			tuple<int64_t, long, json> mediaInfoDetails;
			vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
			vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
			FFMpegWrapper ffmpeg(_configurationRoot);
			mediaInfoDetails = ffmpeg.getMediaInfo(ingestionJobKey, isMMSAssetPathName, timeoutInSeconds, streamingUrl, videoTracks, audioTracks);
			LOG_INFO(
				"Called ffmpeg.getMediaInfo"
				", _processorIdentifier: {}"
				", _ingestionJobKey: {}"
				", streamingUrl: {}"
				", videoTracks.size: {}"
				", audioTracks.size: {}",
				_processorIdentifier, ingestionJobKey, streamingUrl, videoTracks.size(), audioTracks.size()
			);

			if (isVideo && (videoTracks.empty() || audioTracks.empty()))
			{
				string errorMessage = std::format(
					"Video with wrong video or audio tracks"
					", processorIdentifier: {}"
					", ingestionJobKey: {}"
					", videoTracks.size: {}"
					", audioTracks.size: {}",
					_processorIdentifier, ingestionJobKey, videoTracks.size(), audioTracks.size()
				);
				LOG_ERROR(errorMessage);
				throw runtime_error(errorMessage);
			}
			if (!isVideo && (!videoTracks.empty() || audioTracks.empty()))
			{
				string errorMessage = std::format(
					"Audio with wrong video or audio tracks"
					", processorIdentifier: {}"
					", ingestionJobKey: {}"
					", videoTracks.size: {}"
					", audioTracks.size: {}",
					_processorIdentifier, ingestionJobKey, videoTracks.size(), audioTracks.size()
				);
				LOG_ERROR(errorMessage);
				throw runtime_error(errorMessage);
			}
		}

		LOG_INFO(
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
	catch (const exception &e)
	{
		LOG_ERROR(
			"checkStreamingThread failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what(): {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		LOG_INFO(
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
		catch (exception &ex)
		{
			LOG_ERROR(
				"Update IngestionJob failed"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", errorMessage: {}",
				_processorIdentifier, ingestionJobKey, ex.what()
			);
		}

		return;
	}
}
