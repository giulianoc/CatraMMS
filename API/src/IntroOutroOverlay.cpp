
#include "IntroOutroOverlay.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/FileIO.h"                                                                            


void IntroOutroOverlay::encodeContent(
	string requestBody)
{
    string api = "introOutroOverlay";

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

		string introSourceFileExtension;
		{
			string field = "introSourceFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			introSourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string mainSourceFileExtension;
		{
			string field = "mainSourceFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mainSourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string outroSourceFileExtension;
		{
			string field = "outroSourceFileExtension";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			outroSourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string introSourceAssetPathName;
		string mainSourceAssetPathName;
		string outroSourceAssetPathName;
		string encodedStagingAssetPathName;

		if (externalEncoder)
		{
			{
				string field = "introSourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				introSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = introSourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = introSourceAssetPathName.substr(
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

				field = "introSourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string introSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				introSourceAssetPathName = downloadMediaFromMMS(
					ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					introSourceFileExtension,
					introSourcePhysicalDeliveryURL,
					introSourceAssetPathName);
			}

			{
				string field = "mainSourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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

				field = "mainSourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string mainSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				mainSourceAssetPathName = downloadMediaFromMMS(
					ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					mainSourceFileExtension,
					mainSourcePhysicalDeliveryURL,
					mainSourceAssetPathName);
			}

			{
				string field = "outroSourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				outroSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

				{
					size_t endOfDirectoryIndex = outroSourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = outroSourceAssetPathName.substr(
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

				field = "outroSourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string outroSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				outroSourceAssetPathName = downloadMediaFromMMS(
					ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					outroSourceFileExtension,
					outroSourcePhysicalDeliveryURL,
					outroSourceAssetPathName);
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
			string field = "introSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			introSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "mainSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mainSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "outroSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			outroSourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

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

		int64_t introSourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot,
			"introSourceDurationInMilliSeconds", -1);                 
		int64_t mainSourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot,
			"mainSourceDurationInMilliSeconds", -1);                 
		int64_t outroSourceDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot,
			"outroSourceDurationInMilliSeconds", -1);                 

		int introOverlayDurationInSeconds = JSONUtils::asInt(ingestedParametersRoot,
			"introOverlayDurationInSeconds", -1);                 
		int outroOverlayDurationInSeconds = JSONUtils::asInt(ingestedParametersRoot,
			"outroOverlayDurationInSeconds", -1);                 
		bool muteIntroOverlay = JSONUtils::asInt(ingestedParametersRoot, "muteIntroOverlay", true);                 
		bool muteOutroOverlay = JSONUtils::asInt(ingestedParametersRoot, "muteOutroOverlay", true);                 

		_encoding->_ffmpeg->introOutroOverlay(
			introSourceAssetPathName, introSourceDurationInMilliSeconds,
			mainSourceAssetPathName, mainSourceDurationInMilliSeconds,
			outroSourceAssetPathName, outroSourceDurationInMilliSeconds,

			introOverlayDurationInSeconds, outroOverlayDurationInSeconds,
			muteIntroOverlay, muteOutroOverlay,

			encodingProfileDetailsRoot,

			encodedStagingAssetPathName,

			_encodingJobKey,
			ingestionJobKey,
			&(_encoding->_childPid));

        _logger->info(__FILEREF__ + "introOutroOverlay encoding content finished"
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
					+ ", introSourceAssetPathName: " + introSourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(introSourceAssetPathName, exceptionInCaseOfError);
			}

			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", mainSourceAssetPathName: " + mainSourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(mainSourceAssetPathName, exceptionInCaseOfError);
			}

			{
				_logger->info(__FILEREF__ + "Remove file"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", outroSourceAssetPathName: " + outroSourceAssetPathName
				);

				bool exceptionInCaseOfError = false;
				FileIO::remove(outroSourceAssetPathName, exceptionInCaseOfError);
			}

			string workflowLabel =
				JSONUtils::asString(ingestedParametersRoot, "Title", "")
				+ " (add introOutroOverlay from external transcoder)"
			;

			uploadLocalMediaToMMS(
				ingestionJobKey,
				_encodingJobKey,
				ingestedParametersRoot,
				encodingProfileDetailsRoot,
				encodingParametersRoot,
				mainSourceFileExtension,
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
		_killedByUser			= true;

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