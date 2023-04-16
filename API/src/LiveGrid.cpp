
#include "LiveGrid.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


LiveGrid::LiveGrid(
	shared_ptr<LiveProxyAndGrid> liveProxyData,
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	Json::Value configuration,
	mutex* encodingCompletedMutex,
	map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,
	shared_ptr<spdlog::logger> logger):
	FFMPEGEncoderTask(liveProxyData, ingestionJobKey, encodingJobKey, configuration, encodingCompletedMutex,
		encodingCompletedMap, logger)
{
	_liveProxyData					= liveProxyData;

};

LiveGrid::~LiveGrid()
{
	_liveProxyData->_method = "";
	_liveProxyData->_killedBecauseOfNotWorking = false;
	_liveProxyData->_ingestionJobKey = 0;
	_liveProxyData->_liveGridOutputType = "";
	// _liveProxyData->_channelLabel = "";

}

void LiveGrid::encodeContent(
	string requestBody)
{
    string api = "liveGrid";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
        + ", requestBody: " + requestBody
    );

    try
    {
		_liveProxyData->_killedBecauseOfNotWorking = false;

        Json::Value metadataRoot = JSONUtils::toJson(
			-1, _encodingJobKey, requestBody);

		_liveProxyData->_ingestionJobKey = _ingestionJobKey;	// JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);

		Json::Value encodingParametersRoot = metadataRoot["encodingParametersRoot"];
        Json::Value ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];

		Json::Value inputChannelsRoot = encodingParametersRoot["inputChannels"];

		string userAgent = JSONUtils::asString(ingestedParametersRoot, "userAgent", "");
		Json::Value encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		int gridColumns = JSONUtils::asInt(ingestedParametersRoot, "Columns", 0);
		int gridWidth = JSONUtils::asInt(ingestedParametersRoot, "GridWidth", 0);
		int gridHeight = JSONUtils::asInt(ingestedParametersRoot, "GridHeight", 0);

		_liveProxyData->_liveGridOutputType = JSONUtils::asString(ingestedParametersRoot, "outputType",
			"HLS_Channel");

		// it is present only in case of outputType == "SRT"
		string srtURL = JSONUtils::asString(ingestedParametersRoot, "SRT_URL", "");

		// it is present only in case of outputType == "HLS"
		int segmentDurationInSeconds = JSONUtils::asInt(ingestedParametersRoot,
			"segmentDurationInSeconds", 10);

		// it is present only in case of outputType == "HLS"
		int playlistEntriesNumber = JSONUtils::asInt(ingestedParametersRoot,
			"playlistEntriesNumber", 6);

		string manifestDirectoryPath = JSONUtils::asString(encodingParametersRoot, "manifestDirectoryPath", "");
		string manifestFileName = JSONUtils::asString(encodingParametersRoot, "manifestFileName", "");

		// if (_liveProxyData->_outputType == "HLS") // || _liveProxyData->_outputType == "DASH")
		{
			if (_liveProxyData->_liveGridOutputType == "HLS_Channel"
				&& fs::exists(manifestDirectoryPath))
			{
				try
				{
					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", manifestDirectoryPath: " + manifestDirectoryPath
					);
					fs::remove_all(manifestDirectoryPath);
				}
				catch(runtime_error e)
				{
					string errorMessage = __FILEREF__ + "remove directory failed"
						+ ", ingestionJobKey: " + to_string(_liveProxyData->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", manifestDirectoryPath: " + manifestDirectoryPath
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					// throw e;
				}
				catch(exception e)
				{
					string errorMessage = __FILEREF__ + "remove directory failed"
						+ ", ingestionJobKey: " + to_string(_liveProxyData->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", manifestDirectoryPath: " + manifestDirectoryPath
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					// throw e;
				}
			}

			_liveProxyData->_proxyStart = chrono::system_clock::now();

			_liveProxyData->_ffmpeg->liveGrid(
				_liveProxyData->_ingestionJobKey,
				_encodingJobKey,
				encodingProfileDetailsRoot,
				userAgent,
				inputChannelsRoot,
				gridColumns,
				gridWidth,
				gridHeight,
				_liveProxyData->_liveGridOutputType,
				segmentDurationInSeconds,
				playlistEntriesNumber,
				manifestDirectoryPath,
				manifestFileName,
				srtURL,
				&(_liveProxyData->_childPid));
		}

        _logger->info(__FILEREF__ + "_ffmpeg->liveGridBy... finished"
			+ ", ingestionJobKey: " + to_string(_liveProxyData->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
            // + ", _liveProxyData->_channelLabel: " + _liveProxyData->_channelLabel
        );
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (EncodingKilledByUser)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;

        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		if (_liveProxyData->_killedBecauseOfNotWorking)
		{
			// it was killed just because it was not working and not because of user
			// In this case the process has to be restarted soon
			_completedWithError			= true;
			_liveProxyData->_killedBecauseOfNotWorking = false;
		}
		else
		{
			_killedByUser				= true;
		}

		throw e;
    }
    catch(FFMpegURLForbidden e)
    {
		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLForbidden)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage = errorMessage;
		_completedWithError			= true;
		_urlForbidden				= true;

		throw e;
    }
    catch(FFMpegURLNotFound e)
    {
		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (URLNotFound)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage = errorMessage;
		_completedWithError			= true;
		_urlNotFound				= true;

		throw e;
    }
    catch(runtime_error e)
    {
		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (runtime_error)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
    catch(exception e)
    {
		char strDateTime [64];
		{
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());
			tm tmDateTime;
			localtime_r (&utcTime, &tmDateTime);
			sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
				tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1, tmDateTime. tm_mday,
				tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
		}
		string eWhat = e.what();
        string errorMessage = string(strDateTime) + " API failed (exception)"
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", API: " + api
            + ", requestBody: " + requestBody
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_liveProxyData->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
}
