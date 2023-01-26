
#include "CutFrameAccurate.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


void CutFrameAccurate::encodeContent(
	string requestBody)
{
	string api = "cutFrameAccurate";

	_logger->info(__FILEREF__ + "Received " + api
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
		+ ", requestBody: " + requestBody
	);

    try
    {
        Json::Value metadataRoot = JSONUtils::toJson(
			-1, _encodingJobKey, requestBody);

		int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);                 
		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);                  
		Json::Value ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];                       
		Json::Value encodingParametersRoot = metadataRoot["encodingParametersRoot"];                       

		Json::Value encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		string sourceFileExtension;
		{
			string field = "sourceFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string sourceAssetPathName;
		string encodedStagingAssetPathName;

		if (externalEncoder)
		{
			{
				string field = "sourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				sourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = sourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = sourceAssetPathName.substr(
							0, endOfDirectoryIndex);

						_logger->info(__FILEREF__ + "Creating directory"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", directoryPathName: " + directoryPathName
						);
						fs::create_directories(directoryPathName);
						fs::permissions(directoryPathName,
							fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
							| fs::perms::group_read | fs::perms::group_exec
							| fs::perms::others_read | fs::perms::others_exec,
							fs::perm_options::replace);
					}
				}

				field = "sourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string sourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				sourceAssetPathName = downloadMediaFromMMS(
					ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					sourceFileExtension,
					sourcePhysicalDeliveryURL,
					sourceAssetPathName);
			}

			string field = "encodedTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(
						0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "Creating directory"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", directoryPathName: " + directoryPathName
					);
					fs::create_directories(directoryPathName);
					fs::permissions(directoryPathName,
						fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
						| fs::perms::group_read | fs::perms::group_exec
						| fs::perms::others_read | fs::perms::others_exec,
						fs::perm_options::replace);
				}
			}
		}
		else
		{
			string field = "sourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "encodedNFSStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		_encoding->_ffmpeg->cutFrameAccurateWithEncoding(
			ingestionJobKey,
			sourceAssetPathName,
			_encodingJobKey,
			encodingProfileDetailsRoot,
			JSONUtils::asDouble(ingestedParametersRoot, "StartTimeInSeconds", 0.0),
			JSONUtils::asDouble(encodingParametersRoot, "endTimeInSeconds", 0.0),
			JSONUtils::asInt(ingestedParametersRoot, "FramesNumber", -1),
			encodedStagingAssetPathName,

			&(_encoding->_childPid));

        _logger->info(__FILEREF__ + "cut encoding content finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
        );

		if (externalEncoder)
		{
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName
				);

				fs::remove_all(sourceAssetPathName);
			}

			string workflowLabel =
				JSONUtils::asString(ingestedParametersRoot, "Title", "")
				+ " (add cutFrameAccurate from external transcoder)"
			;

			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot,
				"encodingProfileKey", -1);

			uploadLocalMediaToMMS(
				ingestionJobKey,
				_encodingJobKey,
				ingestedParametersRoot,
				encodingProfileDetailsRoot,
				encodingParametersRoot,
				sourceFileExtension,
				encodedStagingAssetPathName,
				workflowLabel,
				"External Transcoder",	// ingester
				encodingProfileKey
			);
		}
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
		_killedByUser				= true;

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
		_encoding->_errorMessage = errorMessage;
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
		_encoding->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
}
