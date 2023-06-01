
#include "IntroOutroOverlay.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


void IntroOutroOverlay::encodeContent(
	string requestBody)
{
    string api = "introOutroOverlay";

	_logger->info(__FILEREF__ + "Received " + api
		+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
		+ ", requestBody: " + requestBody
	);

    try
    {
        Json::Value metadataRoot = JSONUtils::toJson(
			-1, _encodingJobKey, requestBody);

		// int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);                 
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
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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

				field = "introSourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string introSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				introSourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey,
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
				string field = "outroSourceTranscoderStagingAssetPathName";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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

				field = "outroSourcePhysicalDeliveryURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string outroSourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, field, "");

				outroSourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey,
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
			string field = "introSourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
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

		bool splitMain = false;
		if (splitMain)
		{
			string stagingPath = "/var/catramms/storage/MMSTranscoderWorkingAreaRepository/ffmpeg/";

			string main_Begin_PathName = stagingPath + "main_Bagin.avi";
			if (fs::exists(main_Begin_PathName))
				fs::remove_all(main_Begin_PathName);
			double startTimeInSeconds = 0.0;
			double endTimeInSeconds = 60.0;
			_encoding->_ffmpeg->cutWithoutEncoding(
				_ingestionJobKey,
				mainSourceAssetPathName,
				"KeyFrameSeeking",
				true,
				startTimeInSeconds,
				endTimeInSeconds,
				-1,
				main_Begin_PathName);

			string main_End_PathName = stagingPath + "main_End.avi";
			if (fs::exists(main_End_PathName))
				fs::remove_all(main_End_PathName);
			startTimeInSeconds = (mainSourceDurationInMilliSeconds / 1000) - 60.0;
			endTimeInSeconds = mainSourceDurationInMilliSeconds / 1000;
			_encoding->_ffmpeg->cutWithoutEncoding(
				_ingestionJobKey,
				mainSourceAssetPathName,
				"KeyFrameSeeking",
				true,
				startTimeInSeconds,
				endTimeInSeconds,
				-1,
				main_End_PathName);

			string main_Center_PathName = stagingPath + "main_Center.avi";
			if (fs::exists(main_Center_PathName))
				fs::remove_all(main_Center_PathName);
			startTimeInSeconds = 60.0;
			endTimeInSeconds = (mainSourceDurationInMilliSeconds / 1000) - 60.0;
			int64_t mainCenterDurationInMilliSeconds = mainSourceDurationInMilliSeconds - 60000 - 60000;
			_encoding->_ffmpeg->cutWithoutEncoding(
				_ingestionJobKey,
				mainSourceAssetPathName,
				"KeyFrameSeeking",
				true,
				startTimeInSeconds,
				endTimeInSeconds,
				-1,
				main_Center_PathName);

			string main_Intro_PathName = stagingPath + "main_Intro.mp4";
			if (fs::exists(main_Intro_PathName))
				fs::remove_all(main_Intro_PathName);
			_encoding->_ffmpeg->introOverlay(
				introSourceAssetPathName, introSourceDurationInMilliSeconds,
				main_Begin_PathName, 60000,

				introOverlayDurationInSeconds,
				muteIntroOverlay,

				encodingProfileDetailsRoot,

				main_Intro_PathName,

				_encodingJobKey,
				_ingestionJobKey,
				&(_encoding->_childPid));

			string main_Outro_PathName = stagingPath + "main_Outro.mp4";
			if (fs::exists(main_Outro_PathName))
				fs::remove_all(main_Outro_PathName);
			_encoding->_ffmpeg->outroOverlay(
				main_End_PathName, 60000,
				outroSourceAssetPathName, outroSourceDurationInMilliSeconds,

				outroOverlayDurationInSeconds,
				muteOutroOverlay,

				encodingProfileDetailsRoot,

				main_Outro_PathName,

				_encodingJobKey,
				_ingestionJobKey,
				&(_encoding->_childPid));

			string main_Center_Encoded_PathName = stagingPath + "main_Center_Encoded.mp4";
			if (fs::exists(main_Center_Encoded_PathName))
				fs::remove_all(main_Center_Encoded_PathName);
			_encoding->_ffmpeg->encodeContent(
				main_Center_PathName, mainCenterDurationInMilliSeconds,
				main_Center_Encoded_PathName,
				encodingProfileDetailsRoot,
				true,
				Json::nullValue, Json::nullValue,
				-1, -1,
				-1, _encodingJobKey, _ingestionJobKey,
				&(_encoding->_childPid));


			vector<string> sourcePhysicalPaths;
			sourcePhysicalPaths.push_back(main_Intro_PathName);
			sourcePhysicalPaths.push_back(main_Center_Encoded_PathName);
			sourcePhysicalPaths.push_back(main_Outro_PathName);
			_encoding->_ffmpeg->concat(
				_ingestionJobKey,
				true,
				sourcePhysicalPaths,
				encodedStagingAssetPathName);

			// fs::remove_all(main_Begin_PathName);
			// fs::remove_all(main_End_PathName);
			// fs::remove_all(main_Center_PathName);
			// fs::remove_all(main_Intro_PathName);
			// fs::remove_all(main_Outro_PathName);
			// fs::remove_all(main_Center_Encoded_PathName);
		}
		else
		{
			_encoding->_ffmpeg->introOutroOverlay(
				introSourceAssetPathName, introSourceDurationInMilliSeconds,
				mainSourceAssetPathName, mainSourceDurationInMilliSeconds,
				outroSourceAssetPathName, outroSourceDurationInMilliSeconds,

				introOverlayDurationInSeconds, outroOverlayDurationInSeconds,
				muteIntroOverlay, muteOutroOverlay,

				encodingProfileDetailsRoot,

				encodedStagingAssetPathName,

				_encodingJobKey,
				_ingestionJobKey,
				&(_encoding->_childPid));
		}

        _logger->info(__FILEREF__ + "introOutroOverlay encoding content finished"
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
					+ ", introSourceAssetPathName: " + introSourceAssetPathName
				);

				fs::remove_all(introSourceAssetPathName);
			}

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
					+ ", outroSourceAssetPathName: " + outroSourceAssetPathName
				);

				fs::remove_all(outroSourceAssetPathName);
			}

			string workflowLabel =
				JSONUtils::asString(ingestedParametersRoot, "Title", "")
				+ " (add introOutroOverlay from external transcoder)"
			;

			int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot,
				"encodingProfileKey", -1);

			// string encodingProfileFileFormat =
			// 	JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat", "");

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
				// 2023-04-21: Il file generato da introOutroOverlay ha un payload generato dall'encodingProfileKey,
				//	pero', nello stesso tempo, ha un fileFormat dato dal mainSource (vedi come viene costruito
				//	l'encodedFileName in MMSEngineService.cpp).
				//	Per cui possiamo indicare encodingProfileKey quando il file encodato viene aggiunto in MMS
				//	solo se mainSourceFileExtension coincide con il fileFormat dell'encodingProfileKey
				//	fileFormat è ad esempio "mp4", mainSourceFileExtension è per esempio ".ts"
				// 2023-04-22: Penso che l'encodingProfileKey debba essere considerato indipendentemente
				//	se il fileFormat sia uguale o diverso
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
