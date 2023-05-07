
#include "AddSilentAudio.h"

#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


void AddSilentAudio::encodeContent(
	string requestBody)
{
    string api = "addSilentAudio";

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

		string addType = JSONUtils::asString(ingestedParametersRoot, "addType", "entireTrack");                  
		int silentDurationInSeconds = JSONUtils::asInt(ingestedParametersRoot, "silentDurationInSeconds", 1);                  

		Json::Value encodingProfileDetailsRoot = encodingParametersRoot["encodingProfileDetails"];

		Json::Value sourcesRoot = encodingParametersRoot["sources"];

		for(int sourceIndex = 0; sourceIndex < sourcesRoot.size(); sourceIndex++)
		{
			Json::Value sourceRoot = sourcesRoot[sourceIndex];

			bool stopIfReferenceProcessingError = JSONUtils::asBool(sourceRoot, "stopIfReferenceProcessingError", false);
			int64_t sourceDurationInMilliSeconds = JSONUtils::asInt64(sourceRoot, "sourceDurationInMilliSeconds", 0);
			string sourceFileExtension = JSONUtils::asString(sourceRoot, "sourceFileExtension", "");


			string sourceAssetPathName;
			string encodedStagingAssetPathName;

			if (externalEncoder)
			{
				sourceAssetPathName = JSONUtils::asString(sourceRoot, "sourceTranscoderStagingAssetPathName", "");

				{
					size_t endOfDirectoryIndex = sourceAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						string directoryPathName = sourceAssetPathName.substr(
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

				encodedStagingAssetPathName = JSONUtils::asString(sourceRoot, "encodedTranscoderStagingAssetPathName", "");

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

				string sourcePhysicalDeliveryURL = JSONUtils::asString(sourceRoot, "sourcePhysicalDeliveryURL", "");

				sourceAssetPathName = downloadMediaFromMMS(
					_ingestionJobKey,
					_encodingJobKey,
					_encoding->_ffmpeg,
					sourceFileExtension,
					sourcePhysicalDeliveryURL,
					sourceAssetPathName);
			}
			else
			{
				sourceAssetPathName = JSONUtils::asString(sourceRoot, "sourceAssetPathName", "");
				encodedStagingAssetPathName = JSONUtils::asString(sourceRoot, "encodedNFSStagingAssetPathName", "");
			}

			try
			{
				_encoding->_ffmpeg->silentAudio(
					sourceAssetPathName,
					sourceDurationInMilliSeconds,

					addType,
					silentDurationInSeconds,

					encodingProfileDetailsRoot,

					encodedStagingAssetPathName,
					_encodingJobKey,
					_ingestionJobKey,
					&(_encoding->_childPid));

				_logger->info(__FILEREF__ + "Encode content finished"
					+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", encodedStagingAssetPathName: " + encodedStagingAssetPathName
				);
			}
			catch(FFMpegEncodingKilledByUser e)
			{
				throw e;
			}
			catch(runtime_error e)
			{
				if (stopIfReferenceProcessingError || sourceIndex + 1 == sourcesRoot.size())
					throw e;
				else
				{
					_logger->info(__FILEREF__ + "ffmpeg failed but we will continue with the next one"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", stopIfReferenceProcessingError: " + to_string(stopIfReferenceProcessingError)
					);

					continue;
				}
			}
			catch(exception e)
			{
				if (stopIfReferenceProcessingError || sourceIndex + 1 == sourcesRoot.size())
					throw e;
				else
				{
					_logger->info(__FILEREF__ + "ffmpeg failed but we will continue with the next one"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", stopIfReferenceProcessingError: " + to_string(stopIfReferenceProcessingError)
					);

					continue;
				}
			}

			if (externalEncoder)
			{
				{
					_logger->info(__FILEREF__ + "Remove file"
						+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", sourceAssetPathName: " + sourceAssetPathName
					);

					fs::remove_all(sourceAssetPathName);
				}

				string workflowLabel =
					JSONUtils::asString(ingestedParametersRoot, "Title", "")
					+ " (add videoSpeed from external transcoder)"
				;

				int64_t encodingProfileKey = JSONUtils::asInt64(encodingParametersRoot,
					"encodingProfileKey", -1);

				uploadLocalMediaToMMS(
					_ingestionJobKey,
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

