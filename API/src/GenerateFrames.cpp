
#include "GenerateFrames.h"

#include "JSONUtils.h"
#include "MMSCURL.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include "spdlog/spdlog.h"

void GenerateFrames::encodeContent(json metadataRoot)
{
	string api = "generateFrames";

	_logger->info(
		__FILEREF__ + "Received " + api + ", _ingestionJobKey: " + to_string(_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingJobKey) +
		", requestBody: " + JSONUtils::toString(metadataRoot)
	);

	bool externalEncoder = false;
	string imagesDirectory;
	try
	{
		// json metadataRoot = JSONUtils::toJson(
		// 	-1, _encodingJobKey, requestBody);

		bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);
		// int64_t ingestionJobKey = JSONUtils::asInt64(metadataRoot, "ingestionJobKey", -1);
		json encodingParametersRoot = metadataRoot["encodingParametersRoot"];
		json ingestedParametersRoot = metadataRoot["ingestedParametersRoot"];

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
			string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			imagesDirectory = JSONUtils::asString(encodingParametersRoot, field, "");

			string sourcePhysicalDeliveryURL = JSONUtils::asString(encodingParametersRoot, "sourcePhysicalDeliveryURL", "");
			string sourceTranscoderStagingAssetPathName = JSONUtils::asString(encodingParametersRoot, "sourceTranscoderStagingAssetPathName", "");

			{
				string sourceTranscoderStagingAssetDirectory;
				{
					size_t endOfDirectoryIndex = sourceTranscoderStagingAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex == string::npos)
					{
						string errorMessage = __FILEREF__ + "No directory find in the asset file name" +
											  ", sourceTranscoderStagingAssetPathName: " + sourceTranscoderStagingAssetPathName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					sourceTranscoderStagingAssetDirectory = sourceTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);
				}

				if (!fs::exists(sourceTranscoderStagingAssetDirectory))
				{
					bool noErrorIfExists = true;
					bool recursive = true;
					_logger->info(
						__FILEREF__ + "Creating directory" + ", sourceTranscoderStagingAssetDirectory: " + sourceTranscoderStagingAssetDirectory
					);
					fs::create_directories(sourceTranscoderStagingAssetDirectory);
					fs::permissions(
						sourceTranscoderStagingAssetDirectory,
						fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_exec |
							fs::perms::others_read | fs::perms::others_exec,
						fs::perm_options::replace
					);
				}
			}

			sourceAssetPathName = downloadMediaFromMMS(
				_ingestionJobKey, _encodingJobKey, _encoding->_ffmpeg, sourceFileExtension, sourcePhysicalDeliveryURL, sourceAssetPathName
			);
		}
		else
		{
			field = "sourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "nfsImagesDirectory";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			imagesDirectory = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string imageBaseFileName = to_string(_ingestionJobKey);

		_encoding->_ffmpeg->generateFramesToIngest(
			_ingestionJobKey, _encodingJobKey, imagesDirectory, imageBaseFileName, startTimeInSeconds, maxFramesNumber, videoFilter, periodInSeconds,
			mjpeg, imageWidth, imageHeight, sourceAssetPathName, videoDurationInMilliSeconds, &(_encoding->_childPid)
		);

		_encoding->_ffmpegTerminatedSuccessful = true;

		if (externalEncoder)
		{
			vector<int64_t> addContentIngestionJobKeys;

			int generatedFrameIndex = 0;
			for (fs::directory_entry const &entry : fs::directory_iterator(imagesDirectory))
			{
				try
				{
					if (!entry.is_regular_file())
						continue;

					if (!(entry.path().filename().string().size() >= imageBaseFileName.size() &&
						  0 == entry.path().filename().string().compare(0, imageBaseFileName.size(), imageBaseFileName)))
						continue;

					string generateFrameTitle = JSONUtils::asString(ingestedParametersRoot, "title", "");

					string ingestionJobLabel = generateFrameTitle + " (" + to_string(generatedFrameIndex) + ")";

					json userDataRoot;
					{
						if (JSONUtils::isMetadataPresent(ingestedParametersRoot, "userData"))
							userDataRoot = ingestedParametersRoot["userData"];

						json mmsDataRoot;

						json generatedFrameRoot;
						generatedFrameRoot["ingestionJobLabel"] = ingestionJobLabel;
						generatedFrameRoot["ingestionJobKey"] = _ingestionJobKey;
						generatedFrameRoot["generatedFrameIndex"] = generatedFrameIndex;

						mmsDataRoot["generatedFrame"] = generatedFrameRoot;

						userDataRoot["mmsData"] = mmsDataRoot;
					}

					// Title
					string addContentTitle = ingestionJobLabel;

					string outputFileFormat;
					try
					{
						{
							size_t extensionIndex = entry.path().filename().string().find_last_of(".");
							if (extensionIndex == string::npos)
							{
								string errorMessage = __FILEREF__ + "No extension find in the asset file name" +
													  ", entry.path().filename().string(): " + entry.path().filename().string();
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
							outputFileFormat = entry.path().filename().string().substr(extensionIndex + 1);
						}

						_logger->info(
							__FILEREF__ + "ingest Frame" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
							", _encodingJobKey: " + to_string(_encodingJobKey) + ", externalEncoder: " + to_string(externalEncoder) +
							", imagesDirectory: " + imagesDirectory + ", generatedFrameFileName: " + entry.path().filename().string() +
							", addContentTitle: " + addContentTitle + ", outputFileFormat: " + outputFileFormat
						);

						addContentIngestionJobKeys.push_back(generateFrames_ingestFrame(
							_ingestionJobKey, externalEncoder, imagesDirectory, entry.path().filename().string(), addContentTitle, userDataRoot,
							outputFileFormat, ingestedParametersRoot, encodingParametersRoot
						));
					}
					catch (runtime_error &e)
					{
						_logger->error(
							__FILEREF__ + "generateFrames_ingestFrame failed" + ", _encodingJobKey: " + to_string(_encodingJobKey) +
							", _ingestionJobKey: " + to_string(_ingestionJobKey) + ", externalEncoder: " + to_string(externalEncoder) +
							", imagesDirectory: " + imagesDirectory + ", generatedFrameFileName: " + entry.path().filename().string() +
							", addContentTitle: " + addContentTitle + ", outputFileFormat: " + outputFileFormat + ", e.what(): " + e.what()
						);

						throw e;
					}
					catch (exception &e)
					{
						_logger->error(
							__FILEREF__ + "generateFrames_ingestFrame failed" + ", _encodingJobKey: " + to_string(_encodingJobKey) +
							", _ingestionJobKey: " + to_string(_ingestionJobKey) + ", externalEncoder: " + to_string(externalEncoder) +
							", imagesDirectory: " + imagesDirectory + ", generatedFrameFileName: " + entry.path().filename().string() +
							", addContentTitle: " + addContentTitle + ", outputFileFormat: " + outputFileFormat + ", e.what(): " + e.what()
						);

						throw e;
					}

					{
						_logger->info(__FILEREF__ + "remove" + ", framePathName: " + entry.path().string());
						fs::remove_all(entry.path());
					}

					generatedFrameIndex++;
				}
				catch (runtime_error &e)
				{
					string errorMessage = __FILEREF__ + "listing directory failed" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
										  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", e.what(): " + e.what();
					_logger->error(errorMessage);

					_completedWithError = true;
					_encoding->_errorMessage = errorMessage;

					// throw e;
				}
				catch (exception &e)
				{
					string errorMessage = __FILEREF__ + "listing directory failed" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
										  ", _encodingJobKey: " + to_string(_encodingJobKey) + ", e.what(): " + e.what();
					_logger->error(errorMessage);

					_completedWithError = true;
					_encoding->_errorMessage = errorMessage;

					// throw e;
				}
			}

			if (fs::exists(imagesDirectory))
			{
				_logger->info(
					__FILEREF__ + "Remove" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) + ", imagesDirectory: " + imagesDirectory
				);
				fs::remove_all(imagesDirectory);
			}

			// wait the addContent to be executed
			try
			{
				string field = "mmsIngestionURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " +
										  to_string(_ingestionJobKey)
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
						json internalMMSRoot = ingestedParametersRoot[field];

						field = "credentials";
						if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
						{
							json credentialsRoot = internalMMSRoot[field];

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

				while (addContentIngestionJobKeys.size() > 0 &&
					   chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startWaiting).count() < maxSecondsWaiting)
				{
					int64_t addContentIngestionJobKey = *(addContentIngestionJobKeys.begin());

					string mmsIngestionJobURL = mmsIngestionURL + "/" + to_string(addContentIngestionJobKey) + "?ingestionJobOutputs=false";

					vector<string> otherHeaders;
					json ingestionRoot = MMSCURL::httpGetJson(
						_logger, _ingestionJobKey, mmsIngestionJobURL, _mmsAPITimeoutInSeconds, to_string(userKey), apiKey, otherHeaders,
						3 // maxRetryNumber
					);

					string field = "response";
					if (!JSONUtils::isMetadataPresent(ingestionRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " +
											  to_string(_ingestionJobKey)
											  // + ", _encodingJobKey: " + to_string(_encodingJobKey)
											  + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					json responseRoot = ingestionRoot[field];

					field = "ingestionJobs";
					if (!JSONUtils::isMetadataPresent(responseRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " +
											  to_string(_ingestionJobKey)
											  // + ", _encodingJobKey: " + to_string(_encodingJobKey)
											  + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					json ingestionJobsRoot = responseRoot[field];

					if (ingestionJobsRoot.size() != 1)
					{
						string errorMessage = __FILEREF__ + "Wrong ingestionJobs number" + ", _ingestionJobKey: " + to_string(_ingestionJobKey)
							// + ", _encodingJobKey: " + to_string(_encodingJobKey)
							;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					json ingestionJobRoot = ingestionJobsRoot[0];

					field = "status";
					if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", _ingestionJobKey: " +
											  to_string(_ingestionJobKey)
											  // + ", _encodingJobKey: " + to_string(_encodingJobKey)
											  + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					string ingestionJobStatus = JSONUtils::asString(ingestionJobRoot, field, "");

					string prefix = "End_";
					if (ingestionJobStatus.size() >= prefix.size() && 0 == ingestionJobStatus.compare(0, prefix.size(), prefix))
					{
						_logger->info(
							__FILEREF__ + "addContentIngestionJobKey finished" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
							", addContentIngestionJobKey: " + to_string(addContentIngestionJobKey) + ", ingestionJobStatus: " + ingestionJobStatus
						);

						addContentIngestionJobKeys.erase(addContentIngestionJobKeys.begin());
						addContentFinished++;
					}
					else
					{
						int secondsToSleep = 5;

						_logger->info(
							__FILEREF__ + "addContentIngestionJobKey not finished, sleeping..." + ", _ingestionJobKey: " +
							to_string(_ingestionJobKey) + ", addContentIngestionJobKey: " + to_string(addContentIngestionJobKey) +
							", ingestionJobStatus: " + ingestionJobStatus + ", secondsToSleep: " + to_string(secondsToSleep)
						);

						this_thread::sleep_for(chrono::seconds(secondsToSleep));
					}
				}

				_logger->info(
					__FILEREF__ + "Waiting result..." + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
					", addContentToBeWaited: " + to_string(addContentToBeWaited) + ", addContentFinished: " + to_string(addContentFinished) +
					", maxSecondsWaiting: " + to_string(maxSecondsWaiting) +
					", elapsedInSeconds: " + to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startWaiting).count())
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = __FILEREF__ + "waiting addContent ingestion failed" + ", _ingestionJobKey: " + to_string(_ingestionJobKey)
					// + ", _encodingJobKey: " + to_string(_encodingJobKey)
					;
				_logger->error(errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "generateFrames finished" + ", _ingestionJobKey: " + to_string(_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingJobKey) + ", _completedWithError: " + to_string(_completedWithError)
		);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		if (externalEncoder)
		{
			if (imagesDirectory != "" && fs::exists(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove" + ", imagesDirectory: " + imagesDirectory);
				fs::remove_all(imagesDirectory);
			}
		}

		string eWhat = e.what();
		SPDLOG_ERROR(
			"{} API failed (EncodingKilledByUser)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);

		// used by FFMPEGEncoderTask
		_killedByUser = true;

		throw e;
	}
	catch (runtime_error &e)
	{
		if (externalEncoder)
		{
			if (imagesDirectory != "" && fs::exists(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove" + ", imagesDirectory: " + imagesDirectory);
				fs::remove_all(imagesDirectory);
			}
		}

		string eWhat = e.what();
		string errorMessage = fmt::format(
			"{} API failed (runtime_error)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_errorMessage = errorMessage;
		_completedWithError = true;

		throw e;
	}
	catch (exception &e)
	{
		if (externalEncoder)
		{
			if (imagesDirectory != "" && fs::exists(imagesDirectory))
			{
				_logger->info(__FILEREF__ + "Remove" + ", imagesDirectory: " + imagesDirectory);
				fs::remove_all(imagesDirectory);
			}
		}

		string eWhat = e.what();
		string errorMessage = fmt::format(
			"{} API failed (exception)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())), _ingestionJobKey, _encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_errorMessage = errorMessage;
		_completedWithError = true;

		throw e;
	}
}

int64_t GenerateFrames::generateFrames_ingestFrame(
	int64_t ingestionJobKey, bool externalEncoder, string imagesDirectory, string generatedFrameFileName, string addContentTitle, json userDataRoot,
	string outputFileFormat, json ingestedParametersRoot, json encodingParametersRoot
)
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
			ingestionJobKey, addContentTitle, outputFileFormat, "Transcoder -> Generator Frames", sourceURL, addContentTitle, userDataRoot,
			ingestedParametersRoot, encodingProfileKey
		);

		{
			string field = "internalMMS";
			if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				json internalMMSRoot = ingestedParametersRoot[field];

				field = "credentials";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					json credentialsRoot = internalMMSRoot[field];

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
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " +
									  to_string(ingestionJobKey)
									  // + ", encodingJobKey: " + to_string(encodingJobKey)
									  + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		vector<string> otherHeaders;
		string sResponse =
			MMSCURL::httpPostString(
				_logger, ingestionJobKey, mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, to_string(userKey), apiKey, workflowMetadata,
				"application/json", // contentType
				otherHeaders,
				3 // maxRetries
			)
				.second;

		addContentIngestionJobKey = getAddContentIngestionJobKey(ingestionJobKey, sResponse);
	}
	catch (runtime_error e)
	{
		_logger->error(
			__FILEREF__ + "Ingestion workflow failed (runtime_error)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL + ", workflowMetadata: " + workflowMetadata + ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(
			__FILEREF__ + "Ingestion workflow failed (exception)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL + ", workflowMetadata: " + workflowMetadata + ", exception: " + e.what()
		);

		throw e;
	}

	if (addContentIngestionJobKey == -1)
	{
		string errorMessage =
			string("Ingested URL failed, addContentIngestionJobKey is not valid") + ", ingestionJobKey: " + to_string(ingestionJobKey);
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	string mmsBinaryURL;
	// ingest binary
	try
	{
		int64_t frameFileSize = fs::file_size(imagesDirectory + "/" + generatedFrameFileName);

		string mmsBinaryIngestionURL;
		{
			string field = "mmsBinaryIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " +
									  to_string(ingestionJobKey)
									  // + ", encodingJobKey: " + to_string(encodingJobKey)
									  + ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsBinaryIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		mmsBinaryURL = mmsBinaryIngestionURL + "/" + to_string(addContentIngestionJobKey);

		string sResponse = MMSCURL::httpPostFile(
			_logger, ingestionJobKey, mmsBinaryURL, _mmsBinaryTimeoutInSeconds, to_string(userKey), apiKey,
			imagesDirectory + "/" + generatedFrameFileName, frameFileSize,
			3 // maxRetryNumber
		);
	}
	catch (runtime_error e)
	{
		_logger->error(
			__FILEREF__ + "Ingestion binary failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsBinaryURL: " + mmsBinaryURL +
			", workflowMetadata: " + workflowMetadata + ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(
			__FILEREF__ + "Ingestion binary failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsBinaryURL: " + mmsBinaryURL +
			", workflowMetadata: " + workflowMetadata + ", exception: " + e.what()
		);

		throw e;
	}

	return addContentIngestionJobKey;
}
