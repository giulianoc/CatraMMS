
#include "EncodeContent.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/FileIO.h"                                                                            


void EncodeContent::encodeContent(
	string requestBody)
{
    string api = "encodeContent";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
		+ ", requestBody: " + requestBody
    );

	bool externalEncoder = false;
	string sourceAssetPathName;
	string encodedStagingAssetPathName;
	int64_t ingestionJobKey = 1;
    try
    {
        Json::Value metadataRoot = JSONUtils::toJson(
			-1, _encodingJobKey, requestBody);

		ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);

		externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);

		Json::Value ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];
		Json::Value encodingParametersRoot = metadataRoot["encodingParametersRoot"];

        int videoTrackIndexToBeUsed = JSONUtils::asInt(ingestedParametersRoot,
			"VideoTrackIndex", -1);
        int audioTrackIndexToBeUsed = JSONUtils::asInt(ingestedParametersRoot,
			"AudioTrackIndex", -1);

		Json::Value sourcesToBeEncodedRoot = encodingParametersRoot["sourcesToBeEncoded"];
		Json::Value sourceToBeEncodedRoot = sourcesToBeEncodedRoot[0];
		Json::Value encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

        int64_t durationInMilliSeconds = JSONUtils::asInt64(sourceToBeEncodedRoot,
				"sourceDurationInMilliSecs", -1);
        MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(
				JSONUtils::asString(encodingParametersRoot, "contentType", ""));
        int64_t physicalPathKey = JSONUtils::asInt64(sourceToBeEncodedRoot, "sourcePhysicalPathKey", -1);

		Json::Value videoTracksRoot;
		string field = "videoTracks";
        if (JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			videoTracksRoot = sourceToBeEncodedRoot[field];
		Json::Value audioTracksRoot;
		field = "audioTracks";
        if (JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			audioTracksRoot = sourceToBeEncodedRoot[field];

		field = "sourceFileExtension";
		if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingJobKey)
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileExtension = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

		if (externalEncoder)
		{
			field = "sourceTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

			{
				size_t endOfDirectoryIndex = sourceAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = sourceAssetPathName.substr(
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

			field = "sourcePhysicalDeliveryURL";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string sourcePhysicalDeliveryURL = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

			field = "encodedTranscoderStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

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

			sourceAssetPathName = downloadMediaFromMMS(
				ingestionJobKey,
				_encodingJobKey,
				_encoding->_ffmpeg,
				sourceFileExtension,
				sourcePhysicalDeliveryURL,
				sourceAssetPathName);
		}
		else
		{
			field = "mmsSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

			field = "encodedNFSStagingAssetPathName";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedStagingAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, field, "");
		}

        _logger->info(__FILEREF__ + "encoding content..."
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", sourceAssetPathName: " + sourceAssetPathName
            + ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
        );

		// chrono::system_clock::time_point startEncoding = chrono::system_clock::now();
        _encoding->_ffmpeg->encodeContent(
			sourceAssetPathName,
			durationInMilliSeconds,
			encodedStagingAssetPathName,
			encodingProfileDetailsRoot,
			contentType == MMSEngineDBFacade::ContentType::Video,
			videoTracksRoot,
			audioTracksRoot,
			videoTrackIndexToBeUsed, audioTrackIndexToBeUsed,
			physicalPathKey,
			_encodingJobKey,
			ingestionJobKey,
			&(_encoding->_childPid)
		);
		// chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "encoded content"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", sourceAssetPathName: " + sourceAssetPathName
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

				bool exceptionInCaseOfError = false;
				FileIO::remove(sourceAssetPathName, exceptionInCaseOfError);
			}

			field = "sourceMediaItemKey";
			if (!JSONUtils::isMetadataPresent(sourceToBeEncodedRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			int64_t sourceMediaItemKey = JSONUtils::asInt64(sourceToBeEncodedRoot, field, -1);

			field = "encodingProfileKey";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot, field, -1);

			string workflowLabel = "Add Variant " + to_string(sourceMediaItemKey)
				+ " - " + to_string(encodingProfileKey)
				+ " (encoding from external transcoder)"
			;
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
				encodingProfileKey,
				sourceMediaItemKey
			);
		}
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		if (externalEncoder)
		{
			if (sourceAssetPathName != "" && FileIO::fileExisting(sourceAssetPathName))
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(sourceAssetPathName, exceptionInCaseOfError);
			}

			if (encodedStagingAssetPathName != "")
			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
		}

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
		_killedByUser = true;

		throw e;
    }
    catch(runtime_error e)
    {
		if (externalEncoder)
		{
			if (sourceAssetPathName != "" && FileIO::fileExisting(sourceAssetPathName))
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(sourceAssetPathName, exceptionInCaseOfError);
			}

			if (encodedStagingAssetPathName != "")
			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
		}

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
		_encoding->_errorMessage = e.what();
		_completedWithError			= true;

		throw e;
    }
    catch(exception e)
    {
		if (externalEncoder)
		{
			if (sourceAssetPathName != "" && FileIO::fileExisting(sourceAssetPathName))
			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", sourceAssetPathName: " + sourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(sourceAssetPathName, exceptionInCaseOfError);
			}

			if (encodedStagingAssetPathName != "")
			{
				size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
		}

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

