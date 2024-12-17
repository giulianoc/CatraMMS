
#include "LiveGrid.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/DateTime.h"
#include "spdlog/spdlog.h"

LiveGrid::LiveGrid(
	shared_ptr<LiveProxyAndGrid> liveProxyData, int64_t ingestionJobKey, int64_t encodingJobKey, json configurationRoot,
	mutex *encodingCompletedMutex, map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap, shared_ptr<spdlog::logger> logger
)
	: FFMPEGEncoderTask(liveProxyData, ingestionJobKey, encodingJobKey, configurationRoot, encodingCompletedMutex, encodingCompletedMap, logger)
{
	_liveProxyData = liveProxyData;
};

LiveGrid::~LiveGrid()
{
	_liveProxyData->_method = "";
	_liveProxyData->_ingestionJobKey = 0;
	_liveProxyData->_killedBecauseOfNotWorking = false;
	// _liveProxyData->_liveGridOutputType = "";
	// _liveProxyData->_channelLabel = "";
}

void LiveGrid::encodeContent(string requestBody)
{
	string api = "liveGrid";

	_logger->info(__FILEREF__ + "Received " + api + ", _encodingJobKey: " + to_string(_encodingJobKey) + ", requestBody: " + requestBody);

	try
	{
		_liveProxyData->_killedBecauseOfNotWorking = false;
		json metadataRoot = JSONUtils::toJson(requestBody);

		_liveProxyData->_ingestionJobKey = _ingestionJobKey; // JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);

		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];
		json ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];

		json inputChannelsRoot = encodingParametersRoot["inputChannels"];

		string userAgent = JSONUtils::asString(ingestedParametersRoot, "userAgent", "");

		int gridColumns = JSONUtils::asInt(ingestedParametersRoot, "columns", 0);
		int gridWidth = JSONUtils::asInt(ingestedParametersRoot, "gridWidth", 0);
		int gridHeight = JSONUtils::asInt(ingestedParametersRoot, "gridHeight", 0);

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
							_logger->info(__FILEREF__ + "removeDirectory" + ", manifestDirectoryPath: " + manifestDirectoryPath);
							fs::remove_all(manifestDirectoryPath);
						}
						catch (runtime_error &e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed" +
												  ", ingestionJobKey: " + to_string(_liveProxyData->_ingestionJobKey) +
												  ", _encodingJobKey: " + to_string(_encodingJobKey) +
												  ", manifestDirectoryPath: " + manifestDirectoryPath + ", e.what(): " + e.what();
							_logger->error(errorMessage);

							// throw e;
						}
						catch (exception &e)
						{
							string errorMessage = __FILEREF__ + "remove directory failed" +
												  ", ingestionJobKey: " + to_string(_liveProxyData->_ingestionJobKey) +
												  ", _encodingJobKey: " + to_string(_encodingJobKey) +
												  ", manifestDirectoryPath: " + manifestDirectoryPath + ", e.what(): " + e.what();
							_logger->error(errorMessage);

							// throw e;
						}
					}
				}
			}
		}

		{
			_liveProxyData->_proxyStart = chrono::system_clock::now();

			_liveProxyData->_ffmpeg->liveGrid(
				_liveProxyData->_ingestionJobKey, _encodingJobKey, externalEncoder, userAgent, inputChannelsRoot, gridColumns, gridWidth, gridHeight,
				_liveProxyData->_outputsRoot, &(_liveProxyData->_childPid)
			);
		}

		_logger->info(
			__FILEREF__ + "_ffmpeg->liveGridBy... finished" + ", ingestionJobKey: " + to_string(_liveProxyData->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingJobKey)
			// + ", _liveProxyData->_channelLabel: " + _liveProxyData->_channelLabel
		);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		string eWhat = e.what();
		SPDLOG_ERROR(
			"{} API failed (EncodingKilledByUser)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
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
		string errorMessage = fmt::format(
			"{} API failed (URLForbidden)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage = errorMessage;
		_completedWithError = true;
		_urlForbidden = true;

		throw e;
	}
	catch (FFMpegURLNotFound &e)
	{
		string eWhat = e.what();
		string errorMessage = fmt::format(
			"{} API failed (URLNotFound)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage = errorMessage;
		_completedWithError = true;
		_urlNotFound = true;

		throw e;
	}
	catch (runtime_error &e)
	{
		string eWhat = e.what();
		string errorMessage = fmt::format(
			"{} API failed (runtime_error)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage = errorMessage;
		_completedWithError = true;

		throw e;
	}
	catch (exception &e)
	{
		string eWhat = e.what();
		string errorMessage = fmt::format(
			"{} API failed (exception)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			requestBody, (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage = errorMessage;
		_completedWithError = true;

		throw e;
	}
}
