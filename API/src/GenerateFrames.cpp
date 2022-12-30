
#include "GenerateFrames.h"

#include "JSONUtils.h"
#include "MMSCURL.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/FileIO.h"                                                                            
#include "catralibraries/Encrypt.h"


void GenerateFrames::encodeContent(
	string requestBody)
{
    string api = "generateFrames";

    _logger->info(__FILEREF__ + "Received " + api
		+ ", _encodingJobKey: " + to_string(_encodingJobKey)
        + ", requestBody: " + requestBody
    );

	bool externalEncoder = false;
	string imagesDirectory;
    try
    {
        Json::Value metadataRoot = JSONUtils::toJson(
			-1, _encodingJobKey, requestBody);

        bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);
        int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);
		Json::Value encodingParametersRoot = metadataRoot["encodingParametersRoot"];
		Json::Value ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];

        double startTimeInSeconds = JSONUtils::asDouble(encodingParametersRoot, "startTimeInSeconds", 0);
        int maxFramesNumber = JSONUtils::asInt(encodingParametersRoot, "maxFramesNumber", -1);
        string videoFilter = JSONUtils::asString(encodingParametersRoot, "videoFilter", "");
        int periodInSeconds = JSONUtils::asInt(encodingParametersRoot, "periodInSeconds", -1);
        bool mjpeg = JSONUtils::asBool(encodingParametersRoot, "mjpeg", false);
        int imageWidth = JSONUtils::asInt(encodingParametersRoot, "imageWidth", -1);
        int imageHeight = JSONUtils::asInt(encodingParametersRoot, "imageHeight", -1);
        int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot, "videoDurationInMilliSeconds", -1);

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
		string sourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");

		string sourceAssetPathName;

		if (externalEncoder)
		{
			field = "transcoderStagingImagesDirectory";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			imagesDirectory = JSONUtils::asString(encodingParametersRoot, field, "");

			string sourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot,
				"sourcePhysicalDeliveryURL", "");
			string sourceTranscoderStagingAssetPathName = JSONUtils::asString(encodingParametersRoot,
				"sourceTranscoderStagingAssetPathName", "");

			{
				string sourceTranscoderStagingAssetDirectory;
				{
					size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");                             
					if (endOfDirectoryIndex == string::npos)                                                   
					{                                                                                     
						string errorMessage = __FILEREF__ + "No directory find in the asset file name"
							+ ", sourceTranscoderStagingAssetPathName: " + sourceTranscoderStagingAssetPathName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);                                                
					}                                                                                     
					sourceTranscoderStagingAssetDirectory = sourceTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);                          
				}

				if (!FileIO::directoryExisting(sourceTranscoderStagingAssetDirectory))
				{
					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(__FILEREF__ + "Creating directory"
						+ ", sourceTranscoderStagingAssetDirectory: " + sourceTranscoderStagingAssetDirectory
					);
					FileIO::createDirectory(sourceTranscoderStagingAssetDirectory,
						S_IRUSR | S_IWUSR | S_IXUSR |
						S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH,
						noErrorIfExists, recursive);
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
			field = "sourceAssetPathName";
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

			field = "nfsImagesDirectory";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			imagesDirectory = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string imageBaseFileName = to_string(ingestionJobKey);

		_encoding->_ffmpeg->generateFramesToIngest(
			ingestionJobKey,
			_encodingJobKey,
			imagesDirectory,
			imageBaseFileName,
			startTimeInSeconds,
			maxFramesNumber,
			videoFilter,
			periodInSeconds,
			mjpeg,
			imageWidth, 
			imageHeight,
			sourceAssetPathName,
			videoDurationInMilliSeconds,
			&(_encoding->_childPid)
		);

		if (externalEncoder)
		{
			FileIO::DirectoryEntryType_t detDirectoryEntryType;
			shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (imagesDirectory + "/");

			vector<int64_t> addContentIngestionJobKeys;

			bool scanDirectoryFinished = false;
			int generatedFrameIndex = 0;
			while (!scanDirectoryFinished)
			{
				try
				{
					string generatedFrameFileName = FileIO::readDirectory (directory, &detDirectoryEntryType);

					if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
						continue;

					if (!(generatedFrameFileName.size() >= imageBaseFileName.size()
						&& 0 == generatedFrameFileName.compare(0, imageBaseFileName.size(), imageBaseFileName)))
						continue;

					string generateFrameTitle = JSONUtils::asString(ingestedParametersRoot, "Title", "");

					string ingestionJobLabel = generateFrameTitle + " (" + to_string(generatedFrameIndex) + ")";

					Json::Value userDataRoot;
					{
						if (JSONUtils::isMetadataPresent(ingestedParametersRoot, "UserData"))
							userDataRoot = ingestedParametersRoot["UserData"];

						Json::Value mmsDataRoot;
						mmsDataRoot["dataType"] = "generatedFrame";
						mmsDataRoot["ingestionJobLabel"] = ingestionJobLabel;
						mmsDataRoot["ingestionJobKey"] = ingestionJobKey;
						mmsDataRoot["generatedFrameIndex"] = generatedFrameIndex;

						userDataRoot["mmsData"] = mmsDataRoot;
					}

					// Title
					string addContentTitle = ingestionJobLabel;

					string outputFileFormat;                                                               
					try
					{
						{
							size_t extensionIndex = generatedFrameFileName.find_last_of(".");                             
							if (extensionIndex == string::npos)                                                   
							{                                                                                     
								string errorMessage = __FILEREF__ + "No extension find in the asset file name"
									+ ", generatedFrameFileName: " + generatedFrameFileName;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);                                                
							}                                                                                     
							outputFileFormat = generatedFrameFileName.substr(extensionIndex + 1);                          
						}

						_logger->info(__FILEREF__ + "ingest Frame"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", externalEncoder: " + to_string(externalEncoder)
							+ ", imagesDirectory: " + imagesDirectory
							+ ", generatedFrameFileName: " + generatedFrameFileName
							+ ", addContentTitle: " + addContentTitle
							+ ", outputFileFormat: " + outputFileFormat
						);

						addContentIngestionJobKeys.push_back(
							generateFrames_ingestFrame(
								ingestionJobKey, externalEncoder,
								imagesDirectory, generatedFrameFileName,
								addContentTitle, userDataRoot, outputFileFormat,
								ingestedParametersRoot, encodingParametersRoot)
						);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "generateFrames_ingestFrame failed"
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", externalEncoder: " + to_string(externalEncoder)
							+ ", imagesDirectory: " + imagesDirectory
							+ ", generatedFrameFileName: " + generatedFrameFileName
							+ ", addContentTitle: " + addContentTitle
							+ ", outputFileFormat: " + outputFileFormat
							+ ", e.what(): " + e.what()
						);

						throw e;
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "generateFrames_ingestFrame failed"
							+ ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", externalEncoder: " + to_string(externalEncoder)
							+ ", imagesDirectory: " + imagesDirectory
							+ ", generatedFrameFileName: " + generatedFrameFileName
							+ ", addContentTitle: " + addContentTitle
							+ ", outputFileFormat: " + outputFileFormat
							+ ", e.what(): " + e.what()
						);

						throw e;
					}

					{
						_logger->info(__FILEREF__ + "remove"
							+ ", framePathName: " + imagesDirectory + "/" + generatedFrameFileName
						);
						bool exceptionInCaseOfError = false;
						FileIO::remove(imagesDirectory + "/" + generatedFrameFileName,
							exceptionInCaseOfError);
					}

					generatedFrameIndex++;
				}
				catch(DirectoryListFinished e)
				{
					scanDirectoryFinished = true;
				}
				catch(runtime_error e)
				{
					string errorMessage = __FILEREF__ + "listing directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					scanDirectoryFinished = true;

					_completedWithError		= true;
					_encoding->_errorMessage = errorMessage;

					// throw e;
				}
				catch(exception e)
				{
					string errorMessage = __FILEREF__ + "listing directory failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", e.what(): " + e.what()
					;
					_logger->error(errorMessage);

					scanDirectoryFinished = true;

					_completedWithError		= true;
					_encoding->_errorMessage = errorMessage;

					// throw e;
				}
			}

			FileIO::closeDirectory (directory);

			if (FileIO::directoryExisting(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", imagesDirectory: " + imagesDirectory);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
			}

			// wait the addContent to be executed
			try
			{
				string field = "mmsIngestionURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						// + ", _encodingJobKey: " + to_string(_encodingJobKey)
						+ ", Field: " + field;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				string mmsIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");

				int64_t userKey;
				string apiKey;
				{
					string field = "internalMMS";
					if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
					{
						Json::Value internalMMSRoot = ingestedParametersRoot[field];

						field = "credentials";
						if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
						{
							Json::Value credentialsRoot = internalMMSRoot[field];

							field = "userKey";
							userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

							field = "apiKey";
							string apiKeyEncrypted = JSONUtils::asString(credentialsRoot, field, "");
							apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
						}
					}
				}

				chrono::system_clock::time_point startWaiting = chrono::system_clock::now();
				long maxSecondsWaiting = 5 * 60;
				long addContentFinished = 0;
				long addContentToBeWaited = addContentIngestionJobKeys.size();

				while (addContentIngestionJobKeys.size() > 0
					&& chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startWaiting).count() < maxSecondsWaiting)
				{
					int64_t addContentIngestionJobKey = *(addContentIngestionJobKeys.begin());

					string mmsIngestionJobURL =
						mmsIngestionURL
						+ "/" + to_string(addContentIngestionJobKey)
						+ "?ingestionJobOutputs=false"
					;

					Json::Value ingestionRoot = MMSCURL::httpGetJson(
						_logger,
						ingestionJobKey,
						mmsIngestionJobURL,
						_mmsAPITimeoutInSeconds,
						to_string(userKey),
						apiKey,
						3 // maxRetryNumber
					);

					string field = "response";
					if (!JSONUtils::isMetadataPresent(ingestionRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							// + ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					Json::Value responseRoot = ingestionRoot[field];

					field = "ingestionJobs";
					if (!JSONUtils::isMetadataPresent(responseRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							// + ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					Json::Value ingestionJobsRoot = responseRoot[field];

					if (ingestionJobsRoot.size() != 1)
					{
						string errorMessage = __FILEREF__ + "Wrong ingestionJobs number"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							// + ", _encodingJobKey: " + to_string(_encodingJobKey)
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					Json::Value ingestionJobRoot = ingestionJobsRoot[0];

					field = "status";
					if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							// + ", _encodingJobKey: " + to_string(_encodingJobKey)
							+ ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string ingestionJobStatus = JSONUtils::asString(ingestionJobRoot, field, "");

					string prefix = "End_";
					if (ingestionJobStatus.size() >= prefix.size()
						&& 0 == ingestionJobStatus.compare(0, prefix.size(), prefix))
					{
						_logger->info(__FILEREF__ + "addContentIngestionJobKey finished"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", addContentIngestionJobKey: " + to_string(addContentIngestionJobKey)
							+ ", ingestionJobStatus: " + ingestionJobStatus);

						addContentIngestionJobKeys.erase(addContentIngestionJobKeys.begin());
						addContentFinished++;
					}
					else
					{
						int secondsToSleep = 5;

						_logger->info(__FILEREF__ + "addContentIngestionJobKey not finished, sleeping..."
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", addContentIngestionJobKey: " + to_string(addContentIngestionJobKey)
							+ ", ingestionJobStatus: " + ingestionJobStatus
							+ ", secondsToSleep: " + to_string(secondsToSleep)
						);

						this_thread::sleep_for(chrono::seconds(secondsToSleep));
					}
				}

				_logger->info(__FILEREF__ + "Waiting result..."
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", addContentToBeWaited: " + to_string(addContentToBeWaited)
					+ ", addContentFinished: " + to_string(addContentFinished)
					+ ", maxSecondsWaiting: " + to_string(maxSecondsWaiting)
					+ ", elapsedInSeconds: " + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startWaiting).count())
				);
			}
			catch(runtime_error e)
			{
				string errorMessage = __FILEREF__ + "waiting addContent ingestion failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", _encodingJobKey: " + to_string(_encodingJobKey)
				;
				_logger->error(errorMessage);
			}
		}

        _logger->info(__FILEREF__ + "generateFrames finished"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingJobKey)
            + ", _completedWithError: " + to_string(_completedWithError)
        );
    }
	catch(FFMpegEncodingKilledByUser e)
	{
		if (externalEncoder)
		{
			if (imagesDirectory != "" && FileIO::directoryExisting(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", imagesDirectory: " + imagesDirectory);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
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
		_killedByUser				= true;

		throw e;
    }
    catch(runtime_error e)
    {
		if (externalEncoder)
		{
			if (imagesDirectory != "" && FileIO::directoryExisting(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", imagesDirectory: " + imagesDirectory);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
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
		_encoding->_errorMessage = errorMessage;
		_completedWithError			= true;

		throw e;
    }
    catch(exception e)
    {
		if (externalEncoder)
		{
			if (imagesDirectory != "" && FileIO::directoryExisting(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove"
					+ ", imagesDirectory: " + imagesDirectory);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(imagesDirectory, bRemoveRecursively);
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

int64_t GenerateFrames::generateFrames_ingestFrame(
	int64_t ingestionJobKey,
	bool externalEncoder,
	string imagesDirectory, string generatedFrameFileName,
	string addContentTitle,
	Json::Value userDataRoot,
	string outputFileFormat,
	Json::Value ingestedParametersRoot,
	Json::Value encodingParametersRoot)
{
	string workflowMetadata;
	int64_t userKey;
	string apiKey;
	int64_t addContentIngestionJobKey = -1;
	string mmsWorkflowIngestionURL;
	// create the workflow and ingest it
	try
	{
		string sourceURL = "move://" + imagesDirectory + "/" + generatedFrameFileName;
		int64_t encodingProfileKey = -1;
		workflowMetadata = buildAddContentIngestionWorkflow(
			ingestionJobKey, addContentTitle,
			outputFileFormat,
			"Transcoder -> Generator Frames",
			sourceURL,
			addContentTitle,
			userDataRoot,
			ingestedParametersRoot,
			encodingProfileKey
		);

		{
			string field = "internalMMS";
    		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				Json::Value internalMMSRoot = ingestedParametersRoot[field];

				field = "credentials";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					Json::Value credentialsRoot = internalMMSRoot[field];

					field = "userKey";
					userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

					field = "apiKey";
					string apiKeyEncrypted = JSONUtils::asString(credentialsRoot, field, "");
					apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
				}
			}
		}

		{
			string field = "mmsWorkflowIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string sResponse = MMSCURL::httpPostString(
			_logger,
			ingestionJobKey,
			mmsWorkflowIngestionURL,
			_mmsAPITimeoutInSeconds,
			to_string(userKey),
			apiKey,
			workflowMetadata,
			"application/json",	// contentType
			3	// maxRetries
		);

		addContentIngestionJobKey = getAddContentIngestionJobKey(ingestionJobKey, sResponse);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion workflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}

	if (addContentIngestionJobKey == -1)
	{
		string errorMessage =
			string("Ingested URL failed, addContentIngestionJobKey is not valid")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	string mmsBinaryURL;
	// ingest binary
	try
	{
		bool inCaseOfLinkHasItToBeRead = false;
		int64_t frameFileSize = FileIO::getFileSizeInBytes(
			imagesDirectory + "/" + generatedFrameFileName,
			inCaseOfLinkHasItToBeRead);

		string mmsBinaryIngestionURL;
		{
			string field = "mmsBinaryIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsBinaryIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		mmsBinaryURL =
			mmsBinaryIngestionURL
			+ "/" + to_string(addContentIngestionJobKey)
		;

		string sResponse = MMSCURL::httpPostFile(
			_logger,
			ingestionJobKey,
			mmsBinaryURL,
			_mmsBinaryTimeoutInSeconds,
			to_string(userKey),
			apiKey,
			imagesDirectory + "/" + generatedFrameFileName,
			frameFileSize,
			3 // maxRetryNumber
		);
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}

	return addContentIngestionJobKey;
}

