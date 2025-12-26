
#include "GenerateFrames.h"

#include "CurlWrapper.h"
#include "Datetime.h"
#include "Encrypt.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "SafeFileSystem.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"

void GenerateFrames::encodeContent(json metadataRoot)
{
	string api = "generateFrames";

	SPDLOG_INFO(
		"Received {}"
		", _ingestionJobKey: {}"
		", _encodingJobKey: {}"
		", requestBody: {}",
		api, _encoding->_ingestionJobKey, _encoding->_encodingJobKey, JSONUtils::toString(metadataRoot)
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
		int maxFramesNumber = JSONUtils::asInt32(encodingParametersRoot, "maxFramesNumber", -1);
		string videoFilter = JSONUtils::asString(encodingParametersRoot, "videoFilter", "");
		int periodInSeconds = JSONUtils::asInt32(encodingParametersRoot, "periodInSeconds", -1);
		bool mjpeg = JSONUtils::asBool(encodingParametersRoot, "mjpeg", false);
		int imageWidth = JSONUtils::asInt32(encodingParametersRoot, "imageWidth", -1);
		int imageHeight = JSONUtils::asInt32(encodingParametersRoot, "imageHeight", -1);
		int64_t videoDurationInMilliSeconds = JSONUtils::asInt64(encodingParametersRoot, "videoDurationInMilliSeconds", -1);

		string field = "sourceFileExtension";
		if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", Field: {}",
				_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string sourceFileExtension = JSONUtils::asString(encodingParametersRoot, field, "");

		string sourceAssetPathName;

		if (externalEncoder)
		{
			field = "transcoderStagingImagesDirectory";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

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
						string errorMessage = fmt::format(
							"No directory find in the asset file name"
							", sourceTranscoderStagingAssetPathName: {}",
							sourceTranscoderStagingAssetPathName
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					sourceTranscoderStagingAssetDirectory = sourceTranscoderStagingAssetPathName.substr(0, endOfDirectoryIndex);
				}

				if (!fs::exists(sourceTranscoderStagingAssetDirectory))
				{
					bool noErrorIfExists = true;
					bool recursive = true;
					SPDLOG_INFO(
						"Creating directory"
						", sourceTranscoderStagingAssetDirectory: {}",
						sourceTranscoderStagingAssetDirectory
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
				_encoding->_ingestionJobKey, _encoding->_encodingJobKey, _encoding->_ffmpeg, sourceFileExtension, sourcePhysicalDeliveryURL, sourceAssetPathName
			);
		}
		else
		{
			field = "sourceAssetPathName";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceAssetPathName = JSONUtils::asString(encodingParametersRoot, field, "");

			field = "nfsImagesDirectory";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			imagesDirectory = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		string imageBaseFileName = to_string(_encoding->_ingestionJobKey);

		_encoding->_encodingStart = chrono::system_clock::now();
		_encoding->_ffmpeg->generateFramesToIngest(
			_encoding->_ingestionJobKey, _encoding->_encodingJobKey, imagesDirectory, imageBaseFileName, startTimeInSeconds, maxFramesNumber,
			videoFilter, periodInSeconds, mjpeg, imageWidth, imageHeight, sourceAssetPathName, videoDurationInMilliSeconds,
			_encoding->_childProcessId, _encoding->_callbackData
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
						generatedFrameRoot["ingestionJobKey"] = _encoding->_ingestionJobKey;
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
								string errorMessage = fmt::format(
									"No extension find in the asset file name"
									", entry.path().filename().string(): {}",
									entry.path().filename().string()
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
							outputFileFormat = entry.path().filename().string().substr(extensionIndex + 1);
						}

						SPDLOG_INFO(
							"ingest Frame"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", externalEncoder: {}"
							", imagesDirectory: {}"
							", generatedFrameFileName: {}"
							", addContentTitle: {}"
							", outputFileFormat: {}",
							_encoding->_ingestionJobKey, _encoding->_encodingJobKey, externalEncoder, imagesDirectory, entry.path().filename().string(), addContentTitle,
							outputFileFormat
						);

						addContentIngestionJobKeys.push_back(generateFrames_ingestFrame(
							_encoding->_ingestionJobKey, externalEncoder, imagesDirectory, entry.path().filename().string(), addContentTitle,
							userDataRoot, outputFileFormat, ingestedParametersRoot, encodingParametersRoot
						));
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							"generateFrames_ingestFrame failed"
							", _encodingJobKey: {}"
							", _ingestionJobKey: {}"
							", externalEncoder: {}"
							", imagesDirectory: {}"
							", generatedFrameFileName: {}"
							", addContentTitle: {}"
							", outputFileFormat: {}"
							", e.what(): {}",
							_encoding->_encodingJobKey, _encoding->_ingestionJobKey, externalEncoder, imagesDirectory, entry.path().filename().string(), addContentTitle,
							outputFileFormat, e.what()
						);

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"generateFrames_ingestFrame failed"
							", _encodingJobKey: {}"
							", _ingestionJobKey: {}"
							", externalEncoder: {}"
							", imagesDirectory: {}"
							", generatedFrameFileName: {}"
							", addContentTitle: {}"
							", outputFileFormat: {}"
							", e.what(): {}",
							_encoding->_encodingJobKey, _encoding->_ingestionJobKey, externalEncoder, imagesDirectory, entry.path().filename().string(), addContentTitle,
							outputFileFormat, e.what()
						);

						throw e;
					}

					{
						SPDLOG_INFO(
							"remove"
							", framePathName: {}",
							entry.path().string()
						);
						fs::remove_all(entry.path());
					}

					generatedFrameIndex++;
				}
				catch (runtime_error &e)
				{
					string errorMessage = fmt::format(
						"listing directory failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, e.what()
					);
					SPDLOG_ERROR(errorMessage);

					_completedWithError = true;
					_encoding->_callbackData->pushErrorMessage(errorMessage);

					// throw e;
				}
				catch (exception &e)
				{
					string errorMessage = fmt::format(
						"listing directory failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, e.what()
					);
					SPDLOG_ERROR(errorMessage);

					_completedWithError = true;
					_encoding->_callbackData->pushErrorMessage(errorMessage);

					// throw e;
				}
			}

			if (fs::exists(imagesDirectory))
			{
				SPDLOG_INFO(
					"Remove"
					", _ingestionJobKey: {}"
					", imagesDirectory: {}",
					_encoding->_ingestionJobKey, imagesDirectory
				);
				fs::remove_all(imagesDirectory);
			}

			// wait the addContent to be executed
			try
			{
				string field = "mmsIngestionURL";
				if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
				{
					string errorMessage = std::format(
						"Field is not present or it is null"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", Field: {}",
						_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
					);
					SPDLOG_ERROR(errorMessage);

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

				while (!addContentIngestionJobKeys.empty() &&
					   chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startWaiting).count() < maxSecondsWaiting)
				{
					int64_t addContentIngestionJobKey = *(addContentIngestionJobKeys.begin());

					string mmsIngestionJobURL = mmsIngestionURL + "/" + to_string(addContentIngestionJobKey) + "?ingestionJobOutputs=false";

					vector<string> otherHeaders;
					json ingestionRoot = CurlWrapper::httpGetJson(
						mmsIngestionJobURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey), otherHeaders,
						std::format(", ingestionJobKey: {}", _encoding->_ingestionJobKey),
						3 // maxRetryNumber
					);

					string field = "response";
					if (!JSONUtils::isMetadataPresent(ingestionRoot, field))
					{
						string errorMessage = std::format(
							"Field is not present or it is null"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", Field: {}",
							_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					json responseRoot = ingestionRoot[field];

					field = "ingestionJobs";
					if (!JSONUtils::isMetadataPresent(responseRoot, field))
					{
						string errorMessage = std::format(
							"Field is not present or it is null"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", Field: {}",
							_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					json ingestionJobsRoot = responseRoot[field];

					if (ingestionJobsRoot.size() != 1)
					{
						string errorMessage = fmt::format(
							"Wrong ingestionJobs number"
							", _ingestionJobKey: {}",
							_encoding->_ingestionJobKey
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

					json ingestionJobRoot = ingestionJobsRoot[0];

					field = "status";
					if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
					{
						string errorMessage = std::format(
							"Field is not present or it is null"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", Field: {}",
							_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					string ingestionJobStatus = JSONUtils::asString(ingestionJobRoot, field, "");

					string prefix = "End_";
					if (ingestionJobStatus.size() >= prefix.size() && 0 == ingestionJobStatus.compare(0, prefix.size(), prefix))
					{
						SPDLOG_INFO(
							"addContentIngestionJobKey finished"
							", _ingestionJobKey: {}"
							", addContentIngestionJobKey: {}"
							", ingestionJobStatus: {}",
							_encoding->_ingestionJobKey, addContentIngestionJobKey, ingestionJobStatus
						);

						addContentIngestionJobKeys.erase(addContentIngestionJobKeys.begin());
						addContentFinished++;
					}
					else
					{
						int secondsToSleep = 5;

						SPDLOG_INFO(
							"addContentIngestionJobKey not finished, sleeping..."
							", _ingestionJobKey: {}"
							", addContentIngestionJobKey: {}"
							", ingestionJobStatus: {}"
							", secondsToSleep: {}",
							_encoding->_ingestionJobKey, addContentIngestionJobKey, ingestionJobStatus, secondsToSleep
						);

						this_thread::sleep_for(chrono::seconds(secondsToSleep));
					}
				}

				SPDLOG_INFO(
					"Waiting result..."
					", _ingestionJobKey: {}"
					", addContentToBeWaited: {}"
					", addContentFinished: {}"
					", maxSecondsWaiting: {}"
					", elapsedInSeconds: {}",
					_encoding->_ingestionJobKey, addContentToBeWaited, addContentFinished, maxSecondsWaiting,
					chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startWaiting).count()
				);
			}
			catch (runtime_error &e)
			{
				string errorMessage = fmt::format(
					"waiting addContent ingestion failed"
					", _ingestionJobKey: {}",
					_encoding->_ingestionJobKey
				);
				SPDLOG_ERROR(errorMessage);
			}
		}

		SPDLOG_INFO(
			"generateFrames finished"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", _completedWithError: {}",
			_encoding->_ingestionJobKey, _encoding->_encodingJobKey, _completedWithError
		);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		if (externalEncoder)
		{
			if (!imagesDirectory.empty() && fs::exists(imagesDirectory))
			{
				SPDLOG_INFO(
					"Remove"
					", imagesDirectory: {}",
					imagesDirectory
				);
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
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
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
			if (!imagesDirectory.empty() && fs::exists(imagesDirectory))
			{
				SPDLOG_INFO(
					"Remove"
					", imagesDirectory: {}",
					imagesDirectory
				);
				fs::remove_all(imagesDirectory);
			}
		}

		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (runtime_error)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_callbackData->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
	catch (exception &e)
	{
		if (externalEncoder)
		{
			if (!imagesDirectory.empty() && fs::exists(imagesDirectory))
			{
				SPDLOG_INFO(
					"Remove"
					", imagesDirectory: {}",
					imagesDirectory
				);
				fs::remove_all(imagesDirectory);
			}
		}

		string eWhat = e.what();
		string errorMessage = std::format(
			"{} API failed (exception)"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			Datetime::nowLocalTime(), _encoding->_ingestionJobKey, _encoding->_encodingJobKey, api,
			JSONUtils::toString(metadataRoot), (eWhat.size() > 130 ? eWhat.substr(0, 130) : eWhat)
		);
		SPDLOG_ERROR(errorMessage);

		// used by FFMPEGEncoderTask
		_encoding->_callbackData->pushErrorMessage(errorMessage);
		_completedWithError = true;

		throw e;
	}
}

int64_t GenerateFrames::generateFrames_ingestFrame(
	int64_t ingestionJobKey, bool externalEncoder, const string& imagesDirectory, const string& generatedFrameFileName,
	const string& addContentTitle, json userDataRoot,
	string outputFileFormat, json ingestedParametersRoot, const json& encodingParametersRoot
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
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		vector<string> otherHeaders;
		string sResponse =
			CurlWrapper::httpPostString(
				mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey), workflowMetadata,
				"application/json", // contentType
				otherHeaders, std::format(", ingestionJobKey: {}", ingestionJobKey),
				3 // maxRetries
			)
				.second;

		addContentIngestionJobKey = getAddContentIngestionJobKey(ingestionJobKey, sResponse);
	}
	catch (exception& e)
	{
		SPDLOG_ERROR(
			"Ingestion workflow failed"
			", ingestionJobKey: {}"
			", mmsWorkflowIngestionURL: {}"
			", workflowMetadata: {}"
			", exception: {}",
			ingestionJobKey, mmsWorkflowIngestionURL, workflowMetadata, e.what()
		);

		throw;
	}

	if (addContentIngestionJobKey == -1)
	{
		string errorMessage = fmt::format(
			"Ingested URL failed, addContentIngestionJobKey is not valid"
			", ingestionJobKey: {}",
			ingestionJobKey
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	string mmsBinaryURL;
	// ingest binary
	try
	{
#ifdef SAFEFILESYSTEMTHREAD
		int64_t frameFileSize =
			SafeFileSystem::fileSizeThread(imagesDirectory + "/" + generatedFrameFileName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
		int64_t frameFileSize = SafeFileSystem::fileSizeProcess(
			imagesDirectory + "/" + generatedFrameFileName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey)
		);
#else
		int64_t frameFileSize = fs::file_size(imagesDirectory + "/" + generatedFrameFileName);
#endif

		string mmsBinaryIngestionURL;
		{
			string field = "mmsBinaryIngestionURL";
			if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", Field: {}",
					_encoding->_ingestionJobKey, _encoding->_encodingJobKey, field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			mmsBinaryIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
		}

		mmsBinaryURL = mmsBinaryIngestionURL + "/" + to_string(addContentIngestionJobKey);

		string sResponse = CurlWrapper::httpPostFile(
			mmsBinaryURL, _mmsBinaryTimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey),
			imagesDirectory + "/" + generatedFrameFileName, frameFileSize, "", std::format(", ingestionJobKey: {}", ingestionJobKey),
			3 // maxRetryNumber
		);
	}
	catch (exception& e)
	{
		SPDLOG_ERROR(
			"Ingestion binary failed"
			", ingestionJobKey: {}"
			", mmsBinaryURL: {}"
			", workflowMetadata: {}"
			", exception: {}",
			ingestionJobKey, mmsBinaryURL, workflowMetadata, e.what()
		);

		throw;
	}

	return addContentIngestionJobKey;
}
