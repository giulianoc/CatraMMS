
#include "SlideShow.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/FileIO.h"                                                                            


void SlideShow::encodeContent(
	string requestBody)
{
    string api = "slideShow";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
		// + ", requestBody: " + requestBody already logged
    );

    try
    {
        Json::Value metadataRoot = JSONUtils::toJson(
			-1, _encodingJobKey, requestBody);

		int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);                 
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
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

							bool noErrorIfExists = true;
							bool recursive = true;
							_logger->info(__FILEREF__ + "Creating directory"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingJobKey)
								+ ", directoryPathName: " + directoryPathName
							);
							FileIO::createDirectory(directoryPathName,
								S_IRUSR | S_IWUSR | S_IXUSR |
								S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
						}
					}

					imagesPathNames.push_back(
						downloadMediaFromMMS(
							ingestionJobKey,
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
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

							bool noErrorIfExists = true;
							bool recursive = true;
							_logger->info(__FILEREF__ + "Creating directory"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingJobKey)
								+ ", directoryPathName: " + directoryPathName
							);
							FileIO::createDirectory(directoryPathName,
								S_IRUSR | S_IWUSR | S_IXUSR |
								S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
						}
					}

					audiosPathNames.push_back(
						downloadMediaFromMMS(
							ingestionJobKey,
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

					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(__FILEREF__ + "Creating directory"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", directoryPathName: " + directoryPathName
					);
					FileIO::createDirectory(directoryPathName,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, noErrorIfExists, recursive);
				}
			}
		}
		else
		{
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

		Json::Value encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetailsRoot"];                       

		_encoding->_ffmpeg->slideShow(ingestionJobKey, _encodingJobKey,
			durationOfEachSlideInSeconds, frameRateMode,
			encodingProfileDetailsRoot,
			imagesPathNames, audiosPathNames, shortestAudioDurationInSeconds,
			encodedStagingAssetPathName, &(_encoding->_childPid));

        _logger->info(__FILEREF__ + "slideShow finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
        );

		if (externalEncoder)
		{
			for (string imagePathName: imagesPathNames)
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", imagePathName: " + imagePathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(imagePathName, exceptionInCaseOfError);
			}

			for (string audioPathName: audiosPathNames)
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", audioPathName: " + audioPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(audioPathName, exceptionInCaseOfError);
			}

			field = "targetFileFormat";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string targetFileFormat = JSONUtils::asString(encodingParametersRoot, field, "");

			string workflowLabel =
				JSONUtils::asString(ingestedParametersRoot, "Title", "")
				+ " (add slideShow from external transcoder)"
			;

			uploadLocalMediaToMMS(
				ingestionJobKey,
				_encodingJobKey,
				ingestedParametersRoot,
				encodingProfileDetailsRoot,
				encodingParametersRoot,
				targetFileFormat,	// sourceFileExtension,
				encodedStagingAssetPathName,
				workflowLabel,
				"External Transcoder"	// ingester
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

