
#include "FFMPEGEncoderTask.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include <sstream>
#include <filesystem>
#include <fstream>
#include "catralibraries/Encrypt.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"

FFMPEGEncoderTask::FFMPEGEncoderTask(
	shared_ptr<Encoding> encoding,                                                                        
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	Json::Value configuration,
	mutex* encodingCompletedMutex,                                                                        
	map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,                                    
	shared_ptr<spdlog::logger> logger):
	FFMPEGEncoderBase(configuration, logger)
{
	try
	{
		_encoding = encoding;
		_ingestionJobKey = ingestionJobKey;
		_encodingJobKey = encodingJobKey;
		_encodingCompletedMutex = encodingCompletedMutex;                                                         
		_encodingCompletedMap = encodingCompletedMap;                                                             

		_completedWithError		= false;
		_killedByUser			= false;
		_urlForbidden			= false;
		_urlNotFound			= false;

		_tvChannelConfigurationDirectory = JSONUtils::asString(configuration["ffmpeg"],
			"tvChannelConfigurationDirectory", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", ffmpeg->tvChannelConfigurationDirectory: " + _tvChannelConfigurationDirectory
		);

		_tvChannelPort_Start = 8000;                                                                              
		_tvChannelPort_MaxNumberOfOffsets = 100;                                                                  


		_encoding->_errorMessage = "";
		removeEncodingCompletedIfPresent();
	}
	catch(runtime_error e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

FFMPEGEncoderTask::~FFMPEGEncoderTask()
{
	try
	{
		addEncodingCompleted();

		_encoding->_childPid = 0;	// set to 0 just to be sure because it is already set info the FFMpeg lib
		_encoding->_available = true;	// this is the last setting making the encoding available again
	}
	catch(runtime_error e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

void FFMPEGEncoderTask::addEncodingCompleted()
{
	lock_guard<mutex> locker(*_encodingCompletedMutex);

	shared_ptr<EncodingCompleted> encodingCompleted = make_shared<EncodingCompleted>();

	encodingCompleted->_encodingJobKey		= _encodingJobKey;
	encodingCompleted->_completedWithError	= _completedWithError;
	encodingCompleted->_errorMessage		= _encoding->_errorMessage;
	encodingCompleted->_killedByUser		= _killedByUser;
	encodingCompleted->_urlForbidden		= _urlForbidden;
	encodingCompleted->_urlNotFound			= _urlNotFound;
	encodingCompleted->_timestamp			= chrono::system_clock::now();

	_encodingCompletedMap->insert(make_pair(encodingCompleted->_encodingJobKey, encodingCompleted));

	_logger->info(__FILEREF__ + "addEncodingCompleted"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", encodingCompletedMap size: " + to_string(_encodingCompletedMap->size())
			);
}

void FFMPEGEncoderTask::removeEncodingCompletedIfPresent()
{

	lock_guard<mutex> locker(*_encodingCompletedMutex);

	map<int64_t, shared_ptr<EncodingCompleted>>::iterator it =
		_encodingCompletedMap->find(_encodingJobKey);
	if (it != _encodingCompletedMap->end())
	{
		_encodingCompletedMap->erase(it);

		_logger->info(__FILEREF__ + "removeEncodingCompletedIfPresent"
			+ ", _ingestionJobKey: " + to_string(_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingJobKey)
			+ ", encodingCompletedMap size: " + to_string(_encodingCompletedMap->size())
			);
	}
}

void FFMPEGEncoderTask::uploadLocalMediaToMMS(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	Json::Value ingestedParametersRoot,
	Json::Value encodingProfileDetailsRoot,
	Json::Value encodingParametersRoot,
	string sourceFileExtension,
	string encodedStagingAssetPathName,
	string workflowLabel,
	string ingester,
	int64_t encodingProfileKey,
	int64_t variantOfMediaItemKey	// in case Media is a variant of a MediaItem already present
)
{
	string field;

	int64_t userKey;
	string apiKey;
	{
		field = "internalMMS";
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

	string fileFormat;
	if (encodingProfileDetailsRoot != Json::nullValue)
	{
		field = "FileFormat";
		if (!JSONUtils::isMetadataPresent(encodingProfileDetailsRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		fileFormat = JSONUtils::asString(encodingProfileDetailsRoot, field, "");
	}
	else
	{
		if (sourceFileExtension == ".m3u8")
			fileFormat = "hls";
		else
		{
			if (sourceFileExtension.front() == '.')
				fileFormat = sourceFileExtension.substr(1);
			else
				fileFormat = sourceFileExtension;
		}
	}

	field = "mmsWorkflowIngestionURL";
	if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", Field: " + field;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	string mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");

	field = "mmsBinaryIngestionURL";
	if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
	{
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", Field: " + field;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	string mmsBinaryIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");

	int64_t fileSizeInBytes = 0;
	if (fileFormat != "hls")
	{
		fileSizeInBytes = fs::file_size(encodedStagingAssetPathName);                                                                          
	}

	Json::Value userDataRoot;
	{
		if (JSONUtils::isMetadataPresent(ingestedParametersRoot, "UserData"))
			userDataRoot = ingestedParametersRoot["UserData"];

		Json::Value mmsDataRoot;
		mmsDataRoot["dataType"] = "externalTranscoder";
		mmsDataRoot["ingestionJobKey"] = ingestionJobKey;

		userDataRoot["mmsData"] = mmsDataRoot;
	}

	string workflowMetadata;
	{
		string localFileFormat = fileFormat;
		if (fileFormat == "hls")
			localFileFormat = "m3u8-tar.gz";

		if (variantOfMediaItemKey != -1)
		{
			workflowMetadata = buildAddContentIngestionWorkflow(
				ingestionJobKey, workflowLabel, localFileFormat, ingester,
				"",	// sourceURL
				"",	// title
				userDataRoot,
				Json::nullValue,
				encodingProfileKey,
				variantOfMediaItemKey
			);
		}
		else
		{
			workflowMetadata = buildAddContentIngestionWorkflow(
				ingestionJobKey, workflowLabel, localFileFormat, ingester,
				"",	// sourceURL
				"",	// title
				userDataRoot,
				ingestedParametersRoot,
				encodingProfileKey
			);
		}
	}

	int64_t addContentIngestionJobKey = ingestContentByPushingBinary(
		ingestionJobKey,
		workflowMetadata,
		fileFormat,
		encodedStagingAssetPathName,
		fileSizeInBytes,
		userKey,
		apiKey,
		mmsWorkflowIngestionURL,
		mmsBinaryIngestionURL
	);

	/*
	{
		_logger->info(__FILEREF__ + "Remove file"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", sourceAssetPathName: " + sourceAssetPathName
		);

		bool exceptionInCaseOfError = false;
		fs::remove_all(sourceAssetPathName, exceptionInCaseOfError);
	}
	*/

	{
		size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of("/");
		if (endOfDirectoryIndex != string::npos)
		{
			string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

			_logger->info(__FILEREF__ + "removeDirectory"
				+ ", directoryPathName: " + directoryPathName
			);
			fs::remove_all(directoryPathName);
		}
	}

	// wait the addContent to be executed
	try
	{
		string field = "mmsIngestionURL";
		if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				// + ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string mmsIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");

		chrono::system_clock::time_point startWaiting = chrono::system_clock::now();
		long maxSecondsWaiting = 5 * 60;
		long addContentFinished = 0;

		while ( addContentFinished == 0
			&& chrono::duration_cast<chrono::seconds>(
				chrono::system_clock::now() - startWaiting).count() < maxSecondsWaiting)
		{
			string mmsIngestionJobURL =
				mmsIngestionURL
				+ "/" + to_string(addContentIngestionJobKey)
				+ "?ingestionJobOutputs=false"
			;

			vector<string> otherHeaders;
			Json::Value ingestionRoot = MMSCURL::httpGetJson(
				_logger,
				ingestionJobKey,
				mmsIngestionJobURL,
				_mmsAPITimeoutInSeconds,
				to_string(userKey),
				apiKey,
				otherHeaders,
				3 // maxRetryNumber
			);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(ingestionRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
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
					// + ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value ingestionJobsRoot = responseRoot[field];

			if (ingestionJobsRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong ingestionJobs number"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", encodingJobKey: " + to_string(encodingJobKey)
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
					// + ", encodingJobKey: " + to_string(encodingJobKey)
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
			// + ", encodingJobKey: " + to_string(encodingJobKey)
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
}

int64_t FFMPEGEncoderTask::ingestContentByPushingBinary(
	int64_t ingestionJobKey,
	string workflowMetadata,
	string fileFormat,
	string binaryPathFileName,
	int64_t binaryFileSizeInBytes,
	int64_t userKey,
	string apiKey,
	string mmsWorkflowIngestionURL,
	string mmsBinaryIngestionURL
)
{
	_logger->info(__FILEREF__ + "Received ingestContentByPushingBinary"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
		+ ", fileFormat: " + fileFormat
		+ ", binaryPathFileName: " + binaryPathFileName
		+ ", binaryFileSizeInBytes: " + to_string(binaryFileSizeInBytes)
		+ ", userKey: " + to_string(userKey)
		+ ", mmsWorkflowIngestionURL: " + mmsWorkflowIngestionURL
		+ ", mmsBinaryIngestionURL: " + mmsBinaryIngestionURL
	);

	int64_t addContentIngestionJobKey = -1;
	try
	{
		vector<string> otherHeaders;
		string sResponse = MMSCURL::httpPostString(
			_logger,
			ingestionJobKey,
			mmsWorkflowIngestionURL,
			_mmsAPITimeoutInSeconds,
			to_string(userKey),
			apiKey,
			workflowMetadata,
			"application/json",	// contentType
			otherHeaders,
			3 // maxRetryNumber
		).second;

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
		string localBinaryPathFileName = binaryPathFileName;
		int64_t localBinaryFileSizeInBytes = binaryFileSizeInBytes;
		if (fileFormat == "hls")
		{
			// binaryPathFileName is a dir like
			// /var/catramms/storage/MMSTranscoderWorkingAreaRepository/Staging/1_1607526_2022_11_09_09_11_04_0431/content
			// terminating with 'content' as built in MMSEngineProcessor.cpp

			{
				string executeCommand;
				try
				{
					localBinaryPathFileName = binaryPathFileName + ".tar.gz";

					size_t endOfPathIndex = localBinaryPathFileName.find_last_of("/");
					if (endOfPathIndex == string::npos)
					{
						string errorMessage = string("No localVariantPathDirectory found")
							+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
							+ ", localBinaryPathFileName: " + localBinaryPathFileName 
						;
						_logger->error(__FILEREF__ + errorMessage);
          
						throw runtime_error(errorMessage);
					}
					string localVariantPathDirectory =
						localBinaryPathFileName.substr(0, endOfPathIndex);

					executeCommand =
						"tar cfz " + localBinaryPathFileName
						+ " -C " + localVariantPathDirectory
						+ " " + "content";
					_logger->info(__FILEREF__ + "Start tar command "
						+ ", executeCommand: " + executeCommand
					);
					chrono::system_clock::time_point startTar = chrono::system_clock::now();
					int executeCommandStatus = ProcessUtility::execute(executeCommand);
					chrono::system_clock::time_point endTar = chrono::system_clock::now();
					_logger->info(__FILEREF__ + "End tar command "
						+ ", executeCommand: " + executeCommand
						+ ", @MMS statistics@ - tarDuration (millisecs): @"
							+ to_string(chrono::duration_cast<chrono::milliseconds>(endTar - startTar).count()) + "@"
					);
					if (executeCommandStatus != 0)
					{
						string errorMessage = string("ProcessUtility::execute failed")
							+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
							+ ", executeCommandStatus: " + to_string(executeCommandStatus) 
							+ ", executeCommand: " + executeCommand 
						;
						_logger->error(__FILEREF__ + errorMessage);
          
						throw runtime_error(errorMessage);
					}

					localBinaryFileSizeInBytes = fs::file_size(localBinaryPathFileName);                                                                          
				}
				catch(runtime_error e)
				{
					string errorMessage = string("tar command failed")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
						+ ", executeCommand: " + executeCommand 
					;
					_logger->error(__FILEREF__ + errorMessage);
         
					throw runtime_error(errorMessage);
				}
			}
		}

		mmsBinaryURL =
			mmsBinaryIngestionURL
			+ "/" + to_string(addContentIngestionJobKey)
		;

		string sResponse = MMSCURL::httpPostFileSplittingInChunks(
			_logger,
			ingestionJobKey,
			mmsBinaryURL,
			_mmsBinaryTimeoutInSeconds,
			to_string(userKey),
			apiKey,
			localBinaryPathFileName,
			localBinaryFileSizeInBytes,
			3 // maxRetryNumber
		);

		if (fileFormat == "hls")
		{
			_logger->info(__FILEREF__ + "remove"
				+ ", localBinaryPathFileName: " + localBinaryPathFileName
			);
			fs::remove_all(localBinaryPathFileName);
		}
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Ingestion binary failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", mmsBinaryURL: " + mmsBinaryURL
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		if (fileFormat == "hls")
		{
			// it is useless to remove the generated tar.gz file because the parent staging directory
			// will be removed. Also here we should add a bool above to be sure the tar was successful
			/*
			_logger->info(__FILEREF__ + "remove"
				+ ", localBinaryPathFileName: " + localBinaryPathFileName
			);
			bool exceptionInCaseOfError = false;
			fs::remove_all(localBinaryPathFileName, exceptionInCaseOfError);
			*/
		}

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

		if (fileFormat == "hls")
		{
			// it is useless to remove the generated tar.gz file because the parent staging directory
			// will be removed. Also here we should add a bool above to be sure the tar was successful
			/*
			_logger->info(__FILEREF__ + "remove"
				+ ", localBinaryPathFileName: " + localBinaryPathFileName
			);
			bool exceptionInCaseOfError = false;
			fs::remove_all(localBinaryPathFileName, exceptionInCaseOfError);
			*/
		}

		throw e;
	}

	return addContentIngestionJobKey;
}

string FFMPEGEncoderTask::buildAddContentIngestionWorkflow(
	int64_t ingestionJobKey,
	string label,
	string fileFormat,
	string ingester,

	// in case of a new content
	string sourceURL,	// if empty it means binary is ingested later (PUSH)
	string title,
	Json::Value userDataRoot,
	Json::Value ingestedParametersRoot,	// it could be also nullValue
	int64_t encodingProfileKey,

	int64_t variantOfMediaItemKey		// in case of a variant, otherwise -1
)
{
	string workflowMetadata;
	try
	{
		Json::Value addContentRoot;

		string field = "Label";
		addContentRoot[field] = label;

		field = "Type";
		addContentRoot[field] = "Add-Content";

		Json::Value addContentParametersRoot;

		if (variantOfMediaItemKey != -1)
		{
			// it is a Variant

			field = "FileFormat";
			addContentParametersRoot[field] = fileFormat;

			field = "Ingester";
			addContentParametersRoot[field] = ingester;

			field = "variantOfMediaItemKey";
			addContentParametersRoot[field] = variantOfMediaItemKey;

			field = "encodingProfileKey";
			addContentParametersRoot[field] = encodingProfileKey;

			if (userDataRoot != Json::nullValue)
			{
				field = "UserData";
				addContentParametersRoot[field] = userDataRoot;
			}
		}
		else
		{
			// it is a new content

			if (ingestedParametersRoot != Json::nullValue)
			{
				addContentParametersRoot = ingestedParametersRoot;

				field = "internalMMS";
				if (JSONUtils::isMetadataPresent(addContentParametersRoot, field))
				{
					Json::Value removed;
					addContentParametersRoot.removeMember(field, &removed);
				}
			}

			field = "FileFormat";
			addContentParametersRoot[field] = fileFormat;

			field = "Ingester";
			addContentParametersRoot[field] = ingester;

			if (sourceURL != "")
			{
				// string sourceURL = string("move") + "://" + imagesDirectory + "/" + generatedFrameFileName;
				field = "SourceURL";
				addContentParametersRoot[field] = sourceURL;
			}

			if (title != "")
			{
				field = "Title";
				addContentParametersRoot[field] = title;
			}

			if (userDataRoot != Json::nullValue)
			{
				field = "UserData";
				addContentParametersRoot[field] = userDataRoot;
			}

			if (encodingProfileKey != -1)
			{
				field = "encodingProfileKey";
				addContentParametersRoot[field] = encodingProfileKey;
			}
		}

		field = "Parameters";
		addContentRoot[field] = addContentParametersRoot;


		Json::Value workflowRoot;

		field = "Label";
		workflowRoot[field] = label;

		field = "Type";
		workflowRoot[field] = "Workflow";

		field = "Task";
		workflowRoot[field] = addContentRoot;

   		{
       		workflowMetadata = JSONUtils::toString(workflowRoot);
   		}

		_logger->info(__FILEREF__ + "buildAddContentIngestionWorkflow"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", workflowMetadata: " + workflowMetadata
		);

		return workflowMetadata;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "buildAddContentIngestionWorkflow failed (runtime_error)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "buildAddContentIngestionWorkflow failed (exception)"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey) 
			+ ", workflowMetadata: " + workflowMetadata
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

string FFMPEGEncoderTask::downloadMediaFromMMS(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	shared_ptr<FFMpeg> ffmpeg,
	string sourceFileExtension,
	string sourcePhysicalDeliveryURL,
	string destAssetPathName
)
{
	string localDestAssetPathName = destAssetPathName;


	bool isSourceStreaming = false;
	if (sourceFileExtension == ".m3u8")
		isSourceStreaming = true;

	_logger->info(__FILEREF__ + "downloading source content"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", sourcePhysicalDeliveryURL: " + sourcePhysicalDeliveryURL
		+ ", localDestAssetPathName: " + localDestAssetPathName
		+ ", isSourceStreaming: " + to_string(isSourceStreaming)
	);

	if (isSourceStreaming)
	{
		// regenerateTimestamps: see docs/TASK_01_Add_Content_JSON_Format.txt
		bool regenerateTimestamps = false;

		localDestAssetPathName = localDestAssetPathName + ".mp4";

		ffmpeg->streamingToFile(
			ingestionJobKey,
			regenerateTimestamps,
			sourcePhysicalDeliveryURL,
			localDestAssetPathName);
	}
	else
	{
		chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
		double lastPercentageUpdated = -1.0;
		curlpp::types::ProgressFunctionFunctor functor = bind(&FFMPEGEncoderTask::progressDownloadCallback, this,
			ingestionJobKey, lastProgressUpdate, lastPercentageUpdated,
			placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
		MMSCURL::downloadFile(
			_logger,
			ingestionJobKey,
			sourcePhysicalDeliveryURL,
			localDestAssetPathName,
			functor,
			3 // maxRetryNumber
		);
	}

	_logger->info(__FILEREF__ + "downloaded source content"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", sourcePhysicalDeliveryURL: " + sourcePhysicalDeliveryURL
		+ ", localDestAssetPathName: " + localDestAssetPathName
		+ ", isSourceStreaming: " + to_string(isSourceStreaming)
	);

	return localDestAssetPathName;
}

long FFMPEGEncoderTask::getFreeTvChannelPortOffset(
	mutex* tvChannelsPortsMutex,
	long tvChannelPort_CurrentOffset
)
{
	lock_guard<mutex> locker(*tvChannelsPortsMutex);

	long localTvChannelPort_CurrentOffset = tvChannelPort_CurrentOffset;
	bool portAlreadyUsed;
	long attemptNumber = 0;
	do
	{
		attemptNumber++;

		localTvChannelPort_CurrentOffset = (localTvChannelPort_CurrentOffset + 1) % _tvChannelPort_MaxNumberOfOffsets;

		long freeTvChannelPort = localTvChannelPort_CurrentOffset + _tvChannelPort_Start;

		portAlreadyUsed = false;

		try
		{
			const filesystem::path tvChannelConfigurationDirectory (_tvChannelConfigurationDirectory);
			if (filesystem::exists(tvChannelConfigurationDirectory)
				&& filesystem::is_directory(tvChannelConfigurationDirectory))
			{
				// check if this port is already used

				for(const auto& directoryEntry: filesystem::directory_iterator(tvChannelConfigurationDirectory))
				{
					auto fileName = directoryEntry.path().filename();

					_logger->info(__FILEREF__ + "read directory"
						+ ", fileName: " + fileName.string()
					);

					if (!filesystem::is_regular_file(directoryEntry.status()))
						break;

					string sFile;
					{
						ifstream medatataFile(tvChannelConfigurationDirectory / fileName);                                                         
						std::stringstream buffer;                                                                             
						buffer << medatataFile.rdbuf();                                                                       
						sFile = buffer.str();
					}

					// example of line inside the file: 239.255.1.1:8025 1 3006 1059,1159
					string portToLookFor = ":" + to_string(freeTvChannelPort);
					if (sFile.find(portToLookFor) != string::npos)
					{
						_logger->info(__FILEREF__ + "getFreeTvChannelPortOffset. Port is already used"
							+ ", portToLookFor: " + portToLookFor
						);
						portAlreadyUsed = true;

						break;
					}
				}
			}
		}
		catch(filesystem::filesystem_error& e)
		{
			string errorMessage = __FILEREF__ + "getFreeTvChannelPortOffset. File system error"
				+ ", e.what(): " + e.what()
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	while(portAlreadyUsed && attemptNumber < _tvChannelPort_MaxNumberOfOffsets);

	_logger->info(__FILEREF__ + "getFreeTvChannelPortOffset"
		+ ", portAlreadyUsed: " + to_string(portAlreadyUsed)
		+ ", tvChannelPort_CurrentOffset: " + to_string(tvChannelPort_CurrentOffset)
		+ ", localTvChannelPort_CurrentOffset: " + to_string(localTvChannelPort_CurrentOffset)
	);

	return localTvChannelPort_CurrentOffset;
}

void FFMPEGEncoderTask::createOrUpdateTVDvbLastConfigurationFile(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string multicastIP,
	string multicastPort,
	string tvType,
	int64_t tvServiceId,
	int64_t tvFrequency,
	int64_t tvSymbolRate,
	int64_t tvBandwidthInMhz,
	string tvModulation,
	int tvVideoPid,
	int tvAudioItalianPid,
	bool toBeAdded
)
{
	try
	{
		_logger->info(__FILEREF__ + "Received createOrUpdateTVDvbLastConfigurationFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", multicastIP: " + multicastIP
			+ ", multicastPort: " + multicastPort
			+ ", tvType: " + tvType
			+ ", tvServiceId: " + to_string(tvServiceId)
			+ ", tvFrequency: " + to_string(tvFrequency)
			+ ", tvSymbolRate: " + to_string(tvSymbolRate)
			+ ", tvBandwidthInMhz: " + to_string(tvBandwidthInMhz)
			+ ", tvModulation: " + tvModulation
			+ ", tvVideoPid: " + to_string(tvVideoPid)
			+ ", tvAudioItalianPid: " + to_string(tvAudioItalianPid)
			+ ", toBeAdded: " + to_string(toBeAdded)
		);

		string localModulation;

		// dvblast modulation: qpsk|psk_8|apsk_16|apsk_32
		if (tvModulation != "")
		{
			if (tvModulation == "PSK/8")
				localModulation = "psk_8";
			else if (tvModulation == "QAM/64")
				localModulation = "QAM_64";
			else if (tvModulation == "QPSK")
				localModulation = "qpsk";
			else
			{
				string errorMessage = __FILEREF__ + "unknown modulation"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", tvModulation: " + tvModulation
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (!fs::exists(_tvChannelConfigurationDirectory))
		{
			_logger->info(__FILEREF__ + "Create directory"
				+ ", _ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(encodingJobKey)
				+ ", _tvChannelConfigurationDirectory: " + _tvChannelConfigurationDirectory
			);

			fs::create_directories(_tvChannelConfigurationDirectory);
			fs::permissions(_tvChannelConfigurationDirectory,
				fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec
				| fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec
				| fs::perms::others_read | fs::perms::others_write | fs::perms::others_exec,
				fs::perm_options::replace);
		}

		string dvblastConfigurationPathName =
			_tvChannelConfigurationDirectory
			+ "/" + to_string(tvFrequency)
		;
		if (tvSymbolRate < 0)
			dvblastConfigurationPathName += "-";
		else
			dvblastConfigurationPathName += (string("-") + to_string(tvSymbolRate));
		if (tvBandwidthInMhz < 0)
			dvblastConfigurationPathName += "-";
		else
			dvblastConfigurationPathName += (string("-") + to_string(tvBandwidthInMhz));
		dvblastConfigurationPathName += (string("-") + localModulation);

		ifstream ifConfigurationFile;
		bool changedFileFound = false;
		if (fs::exists(dvblastConfigurationPathName + ".txt"))
			ifConfigurationFile.open(dvblastConfigurationPathName + ".txt", ios::in);
		else if (fs::exists(dvblastConfigurationPathName + ".changed"))
		{
			changedFileFound = true;
			ifConfigurationFile.open(dvblastConfigurationPathName + ".changed", ios::in);
		}

		vector<string> vConfiguration;
		if (ifConfigurationFile.is_open())
        {
			string configuration;
            while(getline(ifConfigurationFile, configuration))
			{
				string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

				if (trimmedConfiguration.size() > 10)
					vConfiguration.push_back(trimmedConfiguration);
			}
            ifConfigurationFile.close();
			if (!changedFileFound)	// .txt found
			{
				_logger->info(__FILEREF__ + "Remove dvblast configuration file to create the new one"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", dvblastConfigurationPathName: " + dvblastConfigurationPathName + ".txt"
				);

				fs::remove_all(dvblastConfigurationPathName + ".txt");
			}
		}

		string newConfiguration =
			multicastIP + ":" + multicastPort 
			+ " 1 "
			+ to_string(tvServiceId)
			+ " "
			+ to_string(tvVideoPid) + "," + to_string(tvAudioItalianPid)
		;

		_logger->info(__FILEREF__ + "Creation dvblast configuration file"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", dvblastConfigurationPathName: " + dvblastConfigurationPathName + ".changed"
		);

		ofstream ofConfigurationFile(dvblastConfigurationPathName + ".changed", ofstream::trunc);
		if (!ofConfigurationFile)
		{
			string errorMessage = __FILEREF__ + "Creation dvblast configuration file failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", dvblastConfigurationPathName: " + dvblastConfigurationPathName + ".changed"
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		bool configurationAlreadyPresent = false;
		bool wroteFirstLine = false;
		for(string configuration: vConfiguration)
		{
			if (toBeAdded)
			{
				if (newConfiguration == configuration)
					configurationAlreadyPresent = true;

				if (wroteFirstLine)
					ofConfigurationFile << endl;
				ofConfigurationFile << configuration;
				wroteFirstLine = true;
			}
			else
			{
				if (newConfiguration != configuration)
				{
					if (wroteFirstLine)
						ofConfigurationFile << endl;
					ofConfigurationFile << configuration;
					wroteFirstLine = true;
				}
			}
		}

		if (toBeAdded)
		{
			// added only if not already present
			if (!configurationAlreadyPresent)
			{
				if (wroteFirstLine)
					ofConfigurationFile << endl;
				ofConfigurationFile << newConfiguration;
				wroteFirstLine = true;
			}
		}

		ofConfigurationFile << endl;
	}
	catch (...)
	{
		// make sure do not raise an exception to the calling method to avoid
		// to interrupt "closure" encoding procedure
		string errorMessage = __FILEREF__ + "createOrUpdateTVDvbLastConfigurationFile failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		;
		_logger->error(errorMessage);
	}
}

pair<string, string> FFMPEGEncoderTask::getTVMulticastFromDvblastConfigurationFile(
	int64_t ingestionJobKey,
	int64_t encodingJobKey,
	string tvType,
	int64_t tvServiceId,
	int64_t tvFrequency,
	int64_t tvSymbolRate,
	int64_t tvBandwidthInMhz,
	string tvModulation
)
{
	string multicastIP;
	string multicastPort;

	try
	{
		_logger->info(__FILEREF__ + "Received getTVMulticastFromDvblastConfigurationFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", tvType: " + tvType
			+ ", tvServiceId: " + to_string(tvServiceId)
			+ ", tvFrequency: " + to_string(tvFrequency)
			+ ", tvSymbolRate: " + to_string(tvSymbolRate)
			+ ", tvBandwidthInMhz: " + to_string(tvBandwidthInMhz)
			+ ", tvModulation: " + tvModulation
		);

		string localModulation;

		// dvblast modulation: qpsk|psk_8|apsk_16|apsk_32
		if (tvModulation != "")
		{
			if (tvModulation == "PSK/8")
				localModulation = "psk_8";
			else if (tvModulation == "QAM/64")
				localModulation = "QAM_64";
			else if (tvModulation == "QPSK")
				localModulation = "qpsk";
			else
			{
				string errorMessage = __FILEREF__ + "unknown modulation"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", tvModulation: " + tvModulation
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string dvblastConfigurationPathName =
			_tvChannelConfigurationDirectory
			+ "/" + to_string(tvFrequency)
		;
		if (tvSymbolRate < 0)
			dvblastConfigurationPathName += "-";
		else
			dvblastConfigurationPathName += (string("-") + to_string(tvSymbolRate));
		if (tvBandwidthInMhz < 0)
			dvblastConfigurationPathName += "-";
		else
			dvblastConfigurationPathName += (string("-") + to_string(tvBandwidthInMhz));
		dvblastConfigurationPathName += (string("-") + localModulation);


		ifstream configurationFile;
		if (fs::exists(dvblastConfigurationPathName + ".txt"))
			configurationFile.open(dvblastConfigurationPathName + ".txt", ios::in);
		else if (fs::exists(dvblastConfigurationPathName + ".changed"))
			configurationFile.open(dvblastConfigurationPathName + ".changed", ios::in);

		if (configurationFile.is_open())
		{
			string configuration;
            while(getline(configurationFile, configuration))
			{
				string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

				// configuration is like: 239.255.1.1:8008 1 3401 501,601
				istringstream iss(trimmedConfiguration);
				vector<string> configurationPieces;
				copy(
					istream_iterator<std::string>(iss),
					istream_iterator<std::string>(),
					back_inserter(configurationPieces)
				);
				if(configurationPieces.size() < 3)
					continue;

				if (configurationPieces[2] == to_string(tvServiceId))
				{
					size_t ipSeparator = (configurationPieces[0]).find(":");
					if (ipSeparator != string::npos)
					{
						multicastIP = (configurationPieces[0]).substr(0, ipSeparator);
						multicastPort = (configurationPieces[0]).substr(ipSeparator + 1);

						break;
					}
				}
			}
            configurationFile.close();
        }

		_logger->info(__FILEREF__ + "Received getTVMulticastFromDvblastConfigurationFile"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", tvType: " + tvType
			+ ", tvServiceId: " + to_string(tvServiceId)
			+ ", tvFrequency: " + to_string(tvFrequency)
			+ ", tvSymbolRate: " + to_string(tvSymbolRate)
			+ ", tvBandwidthInMhz: " + to_string(tvBandwidthInMhz)
			+ ", tvModulation: " + tvModulation
			+ ", multicastIP: " + multicastIP
			+ ", multicastPort: " + multicastPort
		);
	}
	catch (...)
	{
		// make sure do not raise an exception to the calling method to avoid
		// to interrupt "closure" encoding procedure
		string errorMessage = __FILEREF__ + "getTVMulticastFromDvblastConfigurationFile failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
		;
		_logger->error(errorMessage);
	}

	return make_pair(multicastIP, multicastPort);
}

int FFMPEGEncoderTask::progressDownloadCallback(
	int64_t ingestionJobKey,
	chrono::system_clock::time_point& lastTimeProgressUpdate, 
	double& lastPercentageUpdated,
	double dltotal, double dlnow,
	double ultotal, double ulnow)
{

	int progressUpdatePeriodInSeconds = 15;

    chrono::system_clock::time_point now = chrono::system_clock::now();
            
    if (dltotal != 0 &&
            (dltotal == dlnow 
            || now - lastTimeProgressUpdate >= chrono::seconds(progressUpdatePeriodInSeconds)))
    {
        double progress = (dlnow / dltotal) * 100;
        // int downloadingPercentage = floorf(progress * 100) / 100;
        // this is to have one decimal in the percentage
        double downloadingPercentage = ((double) ((int) (progress * 10))) / 10;

        _logger->info(__FILEREF__ + "Download still running"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", downloadingPercentage: " + to_string(downloadingPercentage)
            + ", dltotal: " + to_string(dltotal)
            + ", dlnow: " + to_string(dlnow)
            + ", ultotal: " + to_string(ultotal)
            + ", ulnow: " + to_string(ulnow)
        );
        
        lastTimeProgressUpdate = now;

        if (lastPercentageUpdated != downloadingPercentage)
        {
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", downloadingPercentage: " + to_string(downloadingPercentage)
            );                            
            // downloadingStoppedByUser = _mmsEngineDBFacade->updateIngestionJobSourceDownloadingInProgress (
            //     ingestionJobKey, downloadingPercentage);

            lastPercentageUpdated = downloadingPercentage;
        }

        // if (downloadingStoppedByUser)
        //     return 1;   // stop downloading
    }

    return 0;
}

