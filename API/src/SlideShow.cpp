
#include "SlideShow.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


void SlideShow::encodeContent(
	Json::Value metadataRoot)
{
    string api = "slideShow";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
		// + ", requestBody: " + requestBody already logged
    );

    try
    {
        // Json::Value metadataRoot = JSONUtils::toJson(
		// 	-1, _encodingJobKey, requestBody);

		// int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);                 
		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);                  
		Json::Value ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];                       
		Json::Value encodingParametersRoot = metadataRoot["encodingParametersRoot"];                       

		float durationOfEachSlideInSeconds = 2.0;
		string field = "durationOfEachSlideInSeconds";
		durationOfEachSlideInSeconds = JSONUtils::asDouble(ingestedParametersRoot, field, 2.0);

		float shortestAudioDurationInSeconds = -1.0;
		field = "shortestAudioDurationInSeconds";
		shortestAudioDurationInSeconds = JSONUtils::asDouble(encodingParametersRoot, field, -1.0);

		string frameRateMode = "vfr";
		field = "frameRateMode";
		frameRateMode = JSONUtils::asString(ingestedParametersRoot, field, "vfr");

		vector<string> imagesPathNames;
		{
			Json::Value imagesRoot = encodingParametersRoot["imagesRoot"];
			for(int index = 0; index < imagesRoot.size(); index++)
			{
				Json::Value imageRoot = imagesRoot[index];

				if (externalEncoder)
				{
					field = "sourceFileExtension";
					if (!JSONUtils::isMetadataPresent(imageRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourceFileExtension = JSONUtils::asString(imageRoot, field, "");

					field = "sourcePhysicalDeliveryURL";
					if (!JSONUtils::isMetadataPresent(imageRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourcePhysicalDeliveryURL = JSONUtils::asString(imageRoot, field, "");

					field = "sourceTranscoderStagingAssetPathName";
					if (!JSONUtils::isMetadataPresent(imageRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourceTranscoderStagingAssetPathName = JSONUtils::asString(imageRoot, field, "");

					{
						size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							string directoryPathName = sourceTranscoderStagingAssetPathName.substr(
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

					imagesPathNames.push_back(
						downloadMediaFromMMS(
							_ingestionJobKey,
							_encodingJobKey,
							_encoding->_ffmpeg,
							sourceFileExtension,
							sourcePhysicalDeliveryURL,
							sourceTranscoderStagingAssetPathName));
				}
				else
				{
					imagesPathNames.push_back(JSONUtils::asString(imageRoot, "sourceAssetPathName", ""));
				}
			}
		}

		vector<string> audiosPathNames;
		{
			Json::Value audiosRoot = encodingParametersRoot["audiosRoot"];
			for(int index = 0; index < audiosRoot.size(); index++)
			{
				Json::Value audioRoot = audiosRoot[index];

				if (externalEncoder)
				{
					field = "sourceFileExtension";
					if (!JSONUtils::isMetadataPresent(audioRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourceFileExtension = JSONUtils::asString(audioRoot, field, "");

					field = "sourcePhysicalDeliveryURL";
					if (!JSONUtils::isMetadataPresent(audioRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourcePhysicalDeliveryURL = JSONUtils::asString(audioRoot, field, "");

					field = "sourceTranscoderStagingAssetPathName";
					if (!JSONUtils::isMetadataPresent(audioRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string sourceTranscoderStagingAssetPathName = JSONUtils::asString(audioRoot, field, "");

					{
						size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							string directoryPathName = sourceTranscoderStagingAssetPathName.substr(
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

					audiosPathNames.push_back(
						downloadMediaFromMMS(
							_ingestionJobKey,
							_encodingJobKey,
							_encoding->_ffmpeg,
							sourceFileExtension,
							sourcePhysicalDeliveryURL,
							sourceTranscoderStagingAssetPathName));
				}
				else
				{
					audiosPathNames.push_back(JSONUtils::asString(audioRoot, "sourceAssetPathName", ""));
				}
			}
		}

		string encodedStagingAssetPathName;
		if (externalEncoder)
		{
			field = "encodedTranscoderStagingAssetPathName";
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

		Json::Value encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetailsRoot"];                       

		_encoding->_ffmpeg->slideShow(_ingestionJobKey, _encodingJobKey,
			durationOfEachSlideInSeconds, frameRateMode,
			encodingProfileDetailsRoot,
			imagesPathNames, audiosPathNames, shortestAudioDurationInSeconds,
			encodedStagingAssetPathName, &(_encoding->_childPid));

        _logger->info(__FILEREF__ + "slideShow finished"
            + ", _ingestionJobKey: " + to_string(_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
        );

		if (externalEncoder)
		{
			for (string imagePathName: imagesPathNames)
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", imagePathName: " + imagePathName
				);

				fs::remove_all(imagePathName);
			}

			for (string audioPathName: audiosPathNames)
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", audioPathName: " + audioPathName
				);

				fs::remove_all(audioPathName);
			}

			field = "targetFileFormat";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string targetFileFormat = JSONUtils::asString(encodingParametersRoot, field, "");

			string workflowLabel =
				JSONUtils::asString(ingestedParametersRoot, "title", "")
				+ " (add slideShow from external transcoder)"
			;

			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot,
				"encodingProfileKey", -1);

			uploadLocalMediaToMMS(
				_ingestionJobKey,
				_encodingJobKey,
				ingestedParametersRoot,
				encodingProfileDetailsRoot,
				encodingParametersRoot,
				targetFileFormat,	// sourceFileExtension,
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

