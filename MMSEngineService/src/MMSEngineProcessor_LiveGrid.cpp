
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSEngineProcessor.h"
#include "catralibraries/StringUtils.h"

void MMSEngineProcessor::manageLiveGrid(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot
)
{
	try
	{
		/*
		 * commented because it will be High by default
		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority =
				static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority =
				MMSEngineDBFacade::toEncodingPriority(parametersRoot.get(field,
		"XXX").asString());
		}
		*/

		json inputChannelsRoot = json::array();
		json outputsRoot;
		{
			string field = "inputConfigurationLabels";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json inputChannelsRoot = parametersRoot[field];
			for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsRoot.size(); inputChannelIndex++)
			{
				string configurationLabel = JSONUtils::asString(inputChannelsRoot[inputChannelIndex]);

				/*
				bool warningIfMissing = false;
				tuple<int64_t, string, string, string, string, int64_t, bool, int, string, int, int, string, int, int, int, int, int, int64_t>
					confKeyAndChannelURL = _mmsEngineDBFacade->getStreamDetails(workspace->_workspaceKey, configurationLabel, warningIfMissing);

				int64_t confKey;
				string streamSourceType;
				string liveURL;
				tie(confKey, streamSourceType, ignore, liveURL, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore,
					ignore, ignore, ignore, ignore) = confKeyAndChannelURL;
				*/

				auto [confKey, streamSourceType, liveURL] =
					_mmsEngineDBFacade->stream_confKeySourceTypeUrl(workspace->_workspaceKey, configurationLabel);

				// bisognerebbe verificare streamSourceType

				// string youTubePrefix1("https://www.youtube.com/");
				// string youTubePrefix2("https://youtu.be/");
				// if ((liveURL.size() >= youTubePrefix1.size() && 0 == liveURL.compare(0, youTubePrefix1.size(), youTubePrefix1)) ||
				// 	(liveURL.size() >= youTubePrefix2.size() && 0 == liveURL.compare(0, youTubePrefix2.size(), youTubePrefix2)))
				if (StringUtils::startWith(liveURL, "https://www.youtube.com/") || StringUtils::startWith(liveURL, "https://youtu.be/"))
				{
					liveURL = _mmsEngineDBFacade->getStreamingYouTubeLiveURL(workspace, ingestionJobKey, confKey, liveURL);
				}

				json inputChannelRoot;

				inputChannelRoot["confKey"] = confKey;
				inputChannelRoot["configurationLabel"] = configurationLabel;
				inputChannelRoot["liveURL"] = liveURL;

				inputChannelsRoot.push_back(inputChannelRoot);
			}

			field = "outputs";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				outputsRoot = parametersRoot[field];
		}

		json localOutputsRoot = getReviewedOutputsRoot(outputsRoot, workspace, ingestionJobKey, false);

		_mmsEngineDBFacade->addEncoding_LiveGridJob(
			workspace, ingestionJobKey, inputChannelsRoot,
			localOutputsRoot // used by FFMPEGEncoder
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"manageLiveGrid failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what(): {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"manageLiveGrid failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", e.what(): {}",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"manageLiveGrid failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}",
			_processorIdentifier, ingestionJobKey
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
