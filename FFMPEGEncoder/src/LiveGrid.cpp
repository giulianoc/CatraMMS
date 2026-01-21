
#include "LiveGrid.h"

#include "Datetime.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;

LiveGrid::LiveGrid(
	const shared_ptr<LiveProxyAndGrid> &liveProxyData, const json &configurationRoot,
	mutex *encodingCompletedMutex, map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap
)
	: FFMPEGEncoderTask(liveProxyData, configurationRoot, encodingCompletedMutex, encodingCompletedMap)
{
	_liveProxyData = liveProxyData;
};

void LiveGrid::encodeContent(const string_view& requestBody)
{
	string api = "liveGrid";

	LOG_INFO(
		"Received {}"
		", _ingestionJobKey: {}"
		", _encodingJobKey: {}"
		", requestBody: {}",
		api, _encoding->_ingestionJobKey, _encoding->_encodingJobKey, requestBody
	);

	try
	{
		_liveProxyData->_killedBecauseOfNotWorking = false;
		json metadataRoot = JSONUtils::toJson<json>(requestBody);

		_liveProxyData->_ingestionJobKey = _encoding->_ingestionJobKey; // JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);

		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];
		json ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];

		json inputChannelsRoot = encodingParametersRoot["inputChannels"];

		string userAgent = JSONUtils::asString(ingestedParametersRoot, "userAgent", "");

		int gridColumns = JSONUtils::asInt32(ingestedParametersRoot, "columns", 0);
		int gridWidth = JSONUtils::asInt32(ingestedParametersRoot, "gridWidth", 0);
		int gridHeight = JSONUtils::asInt32(ingestedParametersRoot, "gridHeight", 0);

		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);

		_liveProxyData->_outputsRoot = encodingParametersRoot["outputsRoot"];
		{
			for (int outputIndex = 0; outputIndex < _liveProxyData->_outputsRoot.size(); outputIndex++)
			{
				json outputRoot = _liveProxyData->_outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				// if (outputType == "HLS" || outputType == "DASH")
				if (outputType == "HLS_Channel")
				{
					string manifestDirectoryPath = JSONUtils::asString(outputRoot, "manifestDirectoryPath", "");

					if (fs::exists(manifestDirectoryPath))
					{
						try
						{
							LOG_INFO(
								"removeDirectory"
								", manifestDirectoryPath: {}" +
								manifestDirectoryPath
							);
							fs::remove_all(manifestDirectoryPath);
						}
						catch (runtime_error &e)
						{
							string errorMessage = std::format(
								"remove directory failed"
								", ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", manifestDirectoryPath: {}"
								", e.what(): {}",
								_liveProxyData->_ingestionJobKey, _encoding->_encodingJobKey, manifestDirectoryPath, e.what()
							);
							LOG_ERROR(errorMessage);

							// throw e;
						}
						catch (exception &e)
						{
							string errorMessage = std::format(
								"remove directory failed"
								", ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", manifestDirectoryPath: {}"
								", e.what(): {}",
								_liveProxyData->_ingestionJobKey, _encoding->_encodingJobKey, manifestDirectoryPath, e.what()
							);
							LOG_ERROR(errorMessage);

							// throw e;
						}
					}
				}
			}
		}

		{
			_liveProxyData->_encodingStart = chrono::system_clock::now();

			_liveProxyData->_ffmpeg->liveGrid(
				_liveProxyData->_ingestionJobKey, _encoding->_encodingJobKey, externalEncoder, userAgent, inputChannelsRoot, gridColumns, gridWidth,
				gridHeight, _liveProxyData->_outputsRoot, _liveProxyData->_childProcessId, _liveProxyData->_callbackData
			);
		}

		LOG_INFO(
			"_ffmpeg->liveGridBy... finished"
			", ingestionJobKey: {}"
			", _encodingJobKey: {}",
			_liveProxyData->_ingestionJobKey, _encoding->_encodingJobKey
			// + ", _liveProxyData->_channelLabel: " + _liveProxyData->_channelLabel
		);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		string eWhat = e.what();
		LOG_ERROR(
			"{} API failed (EncodingKilledByUser)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);

		// used by FFMPEGEncoderTask
		if (_liveProxyData->_killedBecauseOfNotWorking)
		{
			// it was killed just because it was not working and not because of user
			// In this case the process has to be restarted soon
			_completedWithError = true;
			// _liveProxyData->_killedBecauseOfNotWorking = false;
		}
		else
		{
			_killedByUser = true;
		}

		throw e;
	}
	catch (FFMpegURLForbidden &e)
	{
		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (URLForbidden)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		LOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_callbackData->pushErrorMessage(errorMessage);
		_completedWithError = true;
		// _urlForbidden = true;

		throw e;
	}
	catch (FFMpegURLNotFound &e)
	{
		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (URLNotFound)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		LOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_callbackData->pushErrorMessage(errorMessage);
		_completedWithError = true;
		// _urlNotFound = true;

		throw e;
	}
	catch (runtime_error &e)
	{
		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (runtime_error)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		LOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_callbackData->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
	catch (exception &e)
	{
		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (exception)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		LOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_callbackData->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
}
