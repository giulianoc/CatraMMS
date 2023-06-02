
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

		// 2023-06-01: ho notato che il comando ffmpeg "accumula" lip-sync ed il problema è
		//	evidente con video di durata >= 10min. Dipende anche tanto dalla codifica del sorgente,
		//	un sorgente "non compresso" ritarda il problema del lip-sync
		// Per questo motivo, se la durata del sorgente è abbastanza lunga (vedi controllo sotto), dividiamo il sorgente in tre parti:
		//	la prima e l'ultima di 60 secondi e quella centrale rimanente
		// In questo modo applichiamo intro alla prima parte, l'outro all'ultima parte,
		// la parte centrale la codifichiamo utilizzando lo stesso profilo e poi concateniamo
		// le tre parti risultanti
		int introOutroDurationInSeconds = 60;
		if (mainSourceDurationInMilliSeconds >=
			(introOverlayDurationInSeconds + introOutroDurationInSeconds
			+ outroOverlayDurationInSeconds + introOutroDurationInSeconds) * 1000)
		{
			string stagingBasePath;

			// ci serve un path dello staging locale al transcoder.
			// Utilizziamo solo il path di encodedTranscoderStagingAssetPathName che 
			// sarebbe il path locale dell'asset path name in caso di transcoder remoto
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
			string encodedTranscoderStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, field);

			size_t endOfDirectoryIndex = encodedTranscoderStagingAssetPathName.find_last_of("/");
			if (endOfDirectoryIndex == string::npos)
			{
				string errorMessage = __FILEREF__ + "encodedTranscoderStagingAssetPathName is not well formed"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", encodedTranscoderStagingAssetPathName: " + encodedTranscoderStagingAssetPathName;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			stagingBasePath = encodedTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);
			if (!fs::exists(stagingBasePath))
			{
				_logger->info(__FILEREF__ + "Creating directory"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", stagingBasePath: " + stagingBasePath
				);
				fs::create_directories(stagingBasePath);
				fs::permissions(stagingBasePath,
					fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
					| fs::perms::group_read | fs::perms::group_exec
					| fs::perms::others_read | fs::perms::others_exec,
					fs::perm_options::replace);
			}

			stagingBasePath += ("/introOutroSplit_"
				+ to_string(_ingestionJobKey) + "_" + to_string(_encodingJobKey) + "_");

			string mainBeginPathName = stagingBasePath + "mainBegin" + mainSourceFileExtension;
			if (fs::exists(mainBeginPathName))
				fs::remove_all(mainBeginPathName);
			double startTimeInSeconds = 0.0;
			double endTimeInSeconds = introOverlayDurationInSeconds + introOutroDurationInSeconds;
			int64_t mainBeginDurationInMilliSeconds = (introOverlayDurationInSeconds + introOutroDurationInSeconds) * 1000;
			_encoding->_ffmpeg->cutWithoutEncoding(
				_ingestionJobKey,
				mainSourceAssetPathName,
				"KeyFrameSeeking",
				true,
				startTimeInSeconds,
				endTimeInSeconds,
				-1,
				mainBeginPathName);

			string mainEndPathName = stagingBasePath + "mainEnd" + mainSourceFileExtension;
			if (fs::exists(mainEndPathName))
				fs::remove_all(mainEndPathName);
			startTimeInSeconds = (mainSourceDurationInMilliSeconds / 1000) - (outroOverlayDurationInSeconds + introOutroDurationInSeconds);
			endTimeInSeconds = mainSourceDurationInMilliSeconds / 1000;
			int64_t mainEndDurationInMilliSeconds =
				(
					(mainSourceDurationInMilliSeconds / 1000)
					- ((mainSourceDurationInMilliSeconds / 1000) - (outroOverlayDurationInSeconds + introOutroDurationInSeconds))
				) * 1000;
			_encoding->_ffmpeg->cutWithoutEncoding(
				_ingestionJobKey,
				mainSourceAssetPathName,
				"KeyFrameSeeking",
				true,
				startTimeInSeconds,
				endTimeInSeconds,
				-1,
				mainEndPathName);

			string mainCenterPathName = stagingBasePath + "mainCenter" + mainSourceFileExtension;
			if (fs::exists(mainCenterPathName))
				fs::remove_all(mainCenterPathName);
			startTimeInSeconds = introOverlayDurationInSeconds + introOutroDurationInSeconds;
			endTimeInSeconds = (mainSourceDurationInMilliSeconds / 1000) - (outroOverlayDurationInSeconds + introOutroDurationInSeconds);
			int64_t mainCenterDurationInMilliSeconds =
				(
					((mainSourceDurationInMilliSeconds / 1000) - (outroOverlayDurationInSeconds + introOutroDurationInSeconds))
					- (introOverlayDurationInSeconds + introOutroDurationInSeconds)
				) * 1000;
			_encoding->_ffmpeg->cutWithoutEncoding(
				_ingestionJobKey,
				mainSourceAssetPathName,
				"KeyFrameSeeking",
				true,
				startTimeInSeconds,
				endTimeInSeconds,
				-1,
				mainCenterPathName);

			string destFileFormat = JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat", "");

			string mainIntroPathName = stagingBasePath + "mainIntro." + destFileFormat;
			if (fs::exists(mainIntroPathName))
				fs::remove_all(mainIntroPathName);
			_encoding->_ffmpeg->introOverlay(
				introSourceAssetPathName, introSourceDurationInMilliSeconds,
				mainBeginPathName, mainBeginDurationInMilliSeconds,

				introOverlayDurationInSeconds,
				muteIntroOverlay,

				encodingProfileDetailsRoot,

				mainIntroPathName,

				_encodingJobKey,
				_ingestionJobKey,
				&(_encoding->_childPid));

			string mainOutroPathName = stagingBasePath + "mainOutro." + destFileFormat;
			if (fs::exists(mainOutroPathName))
				fs::remove_all(mainOutroPathName);
			_encoding->_ffmpeg->outroOverlay(
				mainEndPathName, mainEndDurationInMilliSeconds,
				outroSourceAssetPathName, outroSourceDurationInMilliSeconds,

				outroOverlayDurationInSeconds,
				muteOutroOverlay,

				encodingProfileDetailsRoot,

				mainOutroPathName,

				_encodingJobKey,
				_ingestionJobKey,
				&(_encoding->_childPid));

			string mainCenterEncodedPathName = stagingBasePath + "mainCenterEncoded." + destFileFormat;
			if (fs::exists(mainCenterEncodedPathName))
				fs::remove_all(mainCenterEncodedPathName);
			_encoding->_ffmpeg->encodeContent(
				mainCenterPathName, mainCenterDurationInMilliSeconds,
				mainCenterEncodedPathName,
				encodingProfileDetailsRoot,
				true,
				Json::nullValue, Json::nullValue,
				-1, -1,
				-1, _encodingJobKey, _ingestionJobKey,
				&(_encoding->_childPid));


			vector<string> sourcePhysicalPaths;
			sourcePhysicalPaths.push_back(mainIntroPathName);
			sourcePhysicalPaths.push_back(mainCenterEncodedPathName);
			sourcePhysicalPaths.push_back(mainOutroPathName);
			_encoding->_ffmpeg->concat(
				_ingestionJobKey,
				true,
				sourcePhysicalPaths,
				encodedStagingAssetPathName);

			fs::remove_all(mainBeginPathName);
			fs::remove_all(mainEndPathName);
			fs::remove_all(mainCenterPathName);
			fs::remove_all(mainIntroPathName);
			fs::remove_all(mainOutroPathName);
			fs::remove_all(mainCenterEncodedPathName);
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
