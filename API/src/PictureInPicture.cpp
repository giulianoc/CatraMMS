
#include "PictureInPicture.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


void PictureInPicture::encodeContent(
	Json::Value metadataRoot)
{
    string api = "pictureInPicture";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
        + ", requestBody: " + JSONUtils::toString(metadataRoot)
    );

    try
    {
        // Json::Value metadataRoot = JSONUtils::toJson(
		// 	-1, _encodingJobKey, requestBody);

		// int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);                 
		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);                  
		Json::Value ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];                       
		Json::Value encodingParametersRoot = metadataRoot["encodingParametersRoot"];                       

		Json::Value encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		string mainSourceFileExtension;
		{
			string field = "mainSourceFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mainSourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string overlaySourceFileExtension;
		{
			string field = "overlaySourceFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			overlaySourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string mainSourceAssetPathName;
		string overlaySourceAssetPathName;
		string encodedStagingAssetPathName;

		if (externalEncoder)
		{
			{
				string field = "mainSourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				mainSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = mainSourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = mainSourceAssetPathName.substr(
							0, endOfDirectoryIndex);

						_logger->info(__FILEREF__ + "Creating directory"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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

				field = "mainSourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string mainSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				mainSourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					mainSourceFileExtension,
					mainSourcePhysicalDeliveryURL,
					mainSourceAssetPathName);
			}

			{
				string field = "overlaySourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				overlaySourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = overlaySourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = overlaySourceAssetPathName.substr(
							0, endOfDirectoryIndex);

						_logger->info(__FILEREF__ + "Creating directory"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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

				field = "overlaySourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string overlaySourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				overlaySourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					overlaySourceFileExtension,
					overlaySourcePhysicalDeliveryURL,
					overlaySourceAssetPathName);
			}

			string field = "encodedTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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
			string field = "mainSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mainSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "overlaySourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			overlaySourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "encodedNFSStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		int64_t mainSourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot,
			"mainSourceDurationInMilliSeconds", 0);
		int64_t overlaySourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot,
			"overlaySourceDurationInMilliSeconds", 0);
		bool soundOfMain = JSONUtils::asBool(encodingParametersRoot, "soundOfMain", false);

        string overlayPosition_X_InPixel = JSONUtils::asString(ingestedParametersRoot,
			"overlayPosition_X_InPixel", "0");
        string overlayPosition_Y_InPixel = JSONUtils::asString(ingestedParametersRoot,
			"overlayPosition_Y_InPixel", "0");
        string overlay_Width_InPixel = JSONUtils::asString(ingestedParametersRoot,
			"overlay_Width_InPixel", "100");
        string overlay_Height_InPixel = JSONUtils::asString(ingestedParametersRoot,
			"overlay_Height_InPixel", "100");

		_encoding->_ffmpeg->pictureInPicture(
			mainSourceAssetPathName,
			mainSourceDurationInMilliSeconds,

			overlaySourceAssetPathName,
			overlaySourceDurationInMilliSeconds,

			soundOfMain,

			overlayPosition_X_InPixel,
			overlayPosition_Y_InPixel,
			overlay_Width_InPixel,
			overlay_Height_InPixel,

			encodingProfileDetailsRoot,

			encodedStagingAssetPathName,
			_encodingJobKey,
			_ingestionJobKey,
			&(_encoding->_childPid));

        _logger->info(__FILEREF__ + "PictureInPicture encoding content finished"
            + ", _ingestionJobKey: " + to_string(_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
        );

		if (externalEncoder)
		{
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", mainSourceAssetPathName: " + mainSourceAssetPathName
				);

				fs::remove_all(mainSourceAssetPathName);
			}

			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", overlaySourceAssetPathName: " + overlaySourceAssetPathName
				);

				fs::remove_all(overlaySourceAssetPathName);
			}

			string workflowLabel =
				JSONUtils::asString(ingestedParametersRoot, "Title", "")
				+ " (add pictureInPicture from external transcoder)"
			;

			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot,
				"encodingProfileKey", -1);

			uploadLocalMediaToMMS(
				_ingestionJobKey,
				_encodingJobKey,
				ingestedParametersRoot,
				encodingProfileDetailsRoot,
				encodingParametersRoot,
				mainSourceFileExtension,
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
            + ", requestBody: " + JSONUtils::toString(metadataRoot)
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
            + ", requestBody: " + JSONUtils::toString(metadataRoot)
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
            + ", requestBody: " + JSONUtils::toString(metadataRoot)
            + ", e.what(): " + (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
        ;
        _logger->error(__FILEREF__ + errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
}

