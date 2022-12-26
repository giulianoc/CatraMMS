
#include "FFMPEGEncoderTask.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include <sstream>
#include "catralibraries/FileIO.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/ProcessUtility.h"

FFMPEGEncoderTask::FFMPEGEncoderTask(
	shared_ptr<Encoding> encoding,                                                                        
	int64_t encodingJobKey,
	Json::Value configuration,
	mutex* encodingCompletedMutex,                                                                        
	map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,                                    
	shared_ptr<spdlog::logger> logger)
{
	try
	{
		_encoding = encoding;
		_encodingJobKey = encodingJobKey;
		_encodingCompletedMutex = encodingCompletedMutex;                                                         
		_encodingCompletedMap = encodingCompletedMap;                                                             
		_logger = logger;

		_completedWithError		= false;
		_killedByUser			= false;
		_urlForbidden			= false;
		_urlNotFound			= false;

		_mmsAPITimeoutInSeconds = JSONUtils::asInt(configuration["api"], "timeoutInSeconds", 120);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", api->timeoutInSeconds: " + to_string(_mmsAPITimeoutInSeconds)
		);
		_mmsBinaryTimeoutInSeconds = JSONUtils::asInt(configuration["api"]["binary"], "timeoutInSeconds", 120);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", api->binary->timeoutInSeconds: " + to_string(_mmsBinaryTimeoutInSeconds)
		);


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
	int64_t variantOfMediaItemKey,	// in case Media is a variant of a MediaItem already present
	int64_t variantEncodingProfileKey	// in case Media is a variant of a MediaItem already present
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
		bool inCaseOfLinkHasItToBeRead = false;
		fileSizeInBytes = FileIO::getFileSizeInBytes (encodedStagingAssetPathName,
			inCaseOfLinkHasItToBeRead);                                                                          
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

		if (variantOfMediaItemKey != -1 && variantEncodingProfileKey != -1)
		{
			workflowMetadata = buildAddContentIngestionWorkflow(
				ingestionJobKey, workflowLabel, localFileFormat, ingester,
				"",	// sourceURL
				"",	// title
				userDataRoot,
				Json::nullValue,
				variantOfMediaItemKey,
				variantEncodingProfileKey
			);
		}
		else
		{
			workflowMetadata = buildAddContentIngestionWorkflow(
				ingestionJobKey, workflowLabel, localFileFormat, ingester,
				"",	// sourceURL
				"",	// title
				userDataRoot,
				ingestedParametersRoot
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
		FileIO::remove(sourceAssetPathName, exceptionInCaseOfError);
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
			Boolean_t bRemoveRecursively = true;
			FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
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
		string sResponse = MMSCURL::httpPostString(
			_logger,
			ingestionJobKey,
			mmsWorkflowIngestionURL,
			_mmsAPITimeoutInSeconds,
			to_string(userKey),
			apiKey,
			workflowMetadata,
			"application/json",	// contentType
			3 // maxRetryNumber
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

					bool inCaseOfLinkHasItToBeRead = false;
					localBinaryFileSizeInBytes = FileIO::getFileSizeInBytes (localBinaryPathFileName,
						inCaseOfLinkHasItToBeRead);                                                                          
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
			bool exceptionInCaseOfError = false;
			FileIO::remove(localBinaryPathFileName, exceptionInCaseOfError);
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
			FileIO::remove(localBinaryPathFileName, exceptionInCaseOfError);
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
			FileIO::remove(localBinaryPathFileName, exceptionInCaseOfError);
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

	// in case of a Variant
	int64_t variantOfMediaItemKey,		// in case of a variant, otherwise -1
	int64_t variantEncodingProfileKey	// in case of a variant, otherwise -1
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

			field = "variantEncodingProfileKey";
			addContentParametersRoot[field] = variantEncodingProfileKey;

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

long FFMPEGEncoderTask::getAddContentIngestionJobKey(
	int64_t ingestionJobKey,
	string ingestionResponse
)
{
	try
	{
		int64_t addContentIngestionJobKey;

		/*
		{
			"tasks" :
			[
				{
					"ingestionJobKey" : 10793,
					"label" : "Add Content test",
					"type" : "Add-Content"
				},
				{
					"ingestionJobKey" : 10794,
					"label" : "Frame Containing Face: test",
					"type" : "Face-Recognition"
				},
				...
			],
			"workflow" :
			{
				"ingestionRootKey" : 831,
				"label" : "ingestContent test"
			}
		}
		*/
        Json::Value ingestionResponseRoot = JSONUtils::toJson(
			ingestionJobKey, -1, ingestionResponse);

		string field = "tasks";
		if (!JSONUtils::isMetadataPresent(ingestionResponseRoot, field))
		{
			string errorMessage = __FILEREF__
				"ingestion workflow. Response Body json is not well format"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", ingestionResponse: " + ingestionResponse
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		Json::Value tasksRoot = ingestionResponseRoot[field];

		for(int taskIndex = 0; taskIndex < tasksRoot.size(); taskIndex++)
		{
			Json::Value ingestionJobRoot = tasksRoot[taskIndex];

			field = "type";
			if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
			{
				string errorMessage = __FILEREF__
					"ingestion workflow. Response Body json is not well format"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", ingestionResponse: " + ingestionResponse
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			string type = JSONUtils::asString(ingestionJobRoot, field, "");

			if (type == "Add-Content")
			{
				field = "ingestionJobKey";
				if (!JSONUtils::isMetadataPresent(ingestionJobRoot, field))
				{
					string errorMessage = __FILEREF__
						"ingestion workflow. Response Body json is not well format"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", ingestionResponse: " + ingestionResponse
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				addContentIngestionJobKey = JSONUtils::asInt64(ingestionJobRoot, field, -1);

				break;
			}
		}

		return addContentIngestionJobKey;
	}
	catch(...)
	{
		string errorMessage =
			string("ingestion workflow. Response Body json is not well format")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", ingestionResponse: " + ingestionResponse
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
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
		MMSCURL::downloadFile(
			_logger,
			ingestionJobKey,
			sourcePhysicalDeliveryURL,
			localDestAssetPathName,
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

