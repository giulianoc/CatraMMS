
#include "FFMPEGEncoderTask.h"
#include "CurlWrapper.h"
#include "Encrypt.h"
#include "FFMpegWrapper.h"
#include "JSONUtils.h"
#include "MMSStorage.h"
#include "ProcessUtility.h"
#include "SafeFileSystem.h"
#include "StringUtils.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

using namespace std;
using json = nlohmann::json;

FFMPEGEncoderTask::FFMPEGEncoderTask(
	const shared_ptr<Encoding> &encoding, const json& configurationRoot,
	mutex *encodingCompletedMutex, map<int64_t, shared_ptr<EncodingCompleted>> *encodingCompletedMap
)
	: FFMPEGEncoderBase(configurationRoot)
{
	try
	{
		_encoding = encoding;
		_encodingCompletedMutex = encodingCompletedMutex;
		_encodingCompletedMap = encodingCompletedMap;

		_completedWithError = false;
		_killedByUser = false;
		// _urlForbidden = false;
		// _urlNotFound = false;

		_tvChannelConfigurationDirectory = JSONUtils::as<string>(configurationRoot["ffmpeg"], "tvChannelConfigurationDirectory", "");
		LOG_INFO(
			"Configuration item"
			", ffmpeg->tvChannelConfigurationDirectory: {}",
			_tvChannelConfigurationDirectory
		);

		_tvChannelPort_Start = 8000;
		_tvChannelPort_MaxNumberOfOffsets = 100;

		removeEncodingCompletedIfPresent();
	}
	catch (exception &e)
	{
		// error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

FFMPEGEncoderTask::~FFMPEGEncoderTask()
{
	try
	{
		addEncodingCompleted();
	}
	catch (exception &e)
	{
		// error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

void FFMPEGEncoderTask::uploadLocalMediaToMMS(
	int64_t ingestionJobKey, int64_t encodingJobKey, json ingestedParametersRoot, const json& encodingProfileDetailsRoot,
	const json& encodingParametersRoot,
	string sourceFileExtension, const string& encodedStagingAssetPathName, const string& workflowLabel, const string& ingester,
	int64_t encodingProfileKey,
	int64_t variantOfMediaItemKey // in case Media is a variant of a MediaItem already present
)
{
	string field;

	int64_t userKey;
	string apiKey;
	{
		field = "internalMMS";
		if (JSONUtils::isPresent(ingestedParametersRoot, field))
		{
			json internalMMSRoot = ingestedParametersRoot[field];

			field = "credentials";
			if (JSONUtils::isPresent(internalMMSRoot, field))
			{
				json credentialsRoot = internalMMSRoot[field];

				field = "userKey";
				userKey = JSONUtils::as<int64_t>(credentialsRoot, field, -1);

				field = "apiKey";
				string apiKeyEncrypted = JSONUtils::as<string>(credentialsRoot, field, "");
				apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
			}
		}
	}

	string fileFormat;
	if (encodingProfileDetailsRoot != nullptr)
	{
		field = "fileFormat";
		if (!JSONUtils::isPresent(encodingProfileDetailsRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", Field: {}",
				ingestionJobKey, encodingJobKey, field
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		fileFormat = JSONUtils::as<string>(encodingProfileDetailsRoot, field, "");
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

	/*
	field = "mmsWorkflowIngestionURL";
	if (!JSONUtils::isPresent(encodingParametersRoot, field))
	{
		string errorMessage = std::format(
			"Field is not present or it is null"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", Field: {}",
			ingestionJobKey, encodingJobKey, field
		);
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	string mmsWorkflowIngestionURL = JSONUtils::as<string>(encodingParametersRoot, field, "");

	field = "mmsBinaryIngestionURL";
	if (!JSONUtils::isPresent(encodingParametersRoot, field))
	{
		string errorMessage = std::format(
			"Field is not present or it is null"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", Field: {}",
			ingestionJobKey, encodingJobKey, field
		);
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	string mmsBinaryIngestionURL = JSONUtils::as<string>(encodingParametersRoot, field, "");
	*/

	int64_t fileSizeInBytes = 0;
	if (fileFormat != "hls")
	{
#ifdef SAFEFILESYSTEMTHREAD
		fileSizeInBytes = SafeFileSystem::fileSizeThread(encodedStagingAssetPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
		fileSizeInBytes = SafeFileSystem::fileSizeProcess(encodedStagingAssetPathName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#else
		fileSizeInBytes = fs::file_size(encodedStagingAssetPathName);
#endif
	}

	json userDataRoot;
	{
		if (JSONUtils::isPresent(ingestedParametersRoot, "userData"))
			userDataRoot = ingestedParametersRoot["userData"];

		json mmsDataRoot;

		json externalTranscoderRoot;

		externalTranscoderRoot["ingestionJobKey"] = ingestionJobKey;

		mmsDataRoot["externalTranscoder"] = externalTranscoderRoot;

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
				"", // sourceURL
				"", // title
				userDataRoot, nullptr, encodingProfileKey, variantOfMediaItemKey
			);
		}
		else
		{
			workflowMetadata = buildAddContentIngestionWorkflow(
				ingestionJobKey, workflowLabel, localFileFormat, ingester,
				"", // sourceURL
				"", // title
				userDataRoot, ingestedParametersRoot, encodingProfileKey
			);
		}
	}

	int64_t addContentIngestionJobKey = ingestContentByPushingBinary(
		ingestionJobKey, workflowMetadata, fileFormat, encodedStagingAssetPathName, fileSizeInBytes, userKey, apiKey,
		_mmsWorkflowIngestionURL, _mmsBinaryIngestionURL
	);

	/*
	{
		info(__FILEREF__ + "Remove file"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", sourceAssetPathName: " + sourceAssetPathName
		);

		bool exceptionInCaseOfError = false;
		fs::remove_all(sourceAssetPathName, exceptionInCaseOfError);
	}
	*/

	{
		size_t endOfDirectoryIndex = encodedStagingAssetPathName.find_last_of('/');
		if (endOfDirectoryIndex != string::npos)
		{
			string directoryPathName = encodedStagingAssetPathName.substr(0, endOfDirectoryIndex);

			LOG_INFO(
				"removeDirectory"
				", directoryPathName: {}",
				directoryPathName
			);
			fs::remove_all(directoryPathName);
		}
	}

	// wait the addContent to be executed
	try
	{
		/*
		string field = "mmsIngestionURL";
		if (!JSONUtils::isPresent(encodingParametersRoot, field))
		{
			string errorMessage = std::format(
				"Field is not present or it is null"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", Field: {}",
				ingestionJobKey, encodingJobKey, field
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		string mmsIngestionURL = JSONUtils::as<string>(encodingParametersRoot, field, "");
		*/

		chrono::system_clock::time_point startWaiting = chrono::system_clock::now();
		long maxSecondsWaiting = 5 * 60;
		long addContentFinished = 0;

		while (addContentFinished == 0 &&
			   chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startWaiting).count() < maxSecondsWaiting)
		{
			string mmsIngestionJobURL = std::format("{}/{}?ingestionJobOutputs=false", _mmsIngestionURL, addContentIngestionJobKey);

			vector<string> otherHeaders;
			json ingestionRoot = CurlWrapper::httpGetJson(
				mmsIngestionJobURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey), otherHeaders,
				std::format(", ingestionJobKey: {}", ingestionJobKey),
				3 // maxRetryNumber
			);

			string field = "response";
			if (!JSONUtils::isPresent(ingestionRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", Field: {}",
					ingestionJobKey, encodingJobKey, field
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json responseRoot = ingestionRoot[field];

			field = "ingestionJobs";
			if (!JSONUtils::isPresent(responseRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", Field: {}",
					ingestionJobKey, encodingJobKey, field
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json ingestionJobsRoot = responseRoot[field];

			if (ingestionJobsRoot.size() != 1)
			{
				string errorMessage = std::format(
					"Wrong ingestionJobs number"
					", ingestionJobKey: {}"
					", encodingJobKey: {}",
					ingestionJobKey, encodingJobKey
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			json ingestionJobRoot = ingestionJobsRoot[0];

			field = "status";
			if (!JSONUtils::isPresent(ingestionJobRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", Field: {}",
					ingestionJobKey, encodingJobKey, field
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			string ingestionJobStatus = JSONUtils::as<string>(ingestionJobRoot, field, "");

			string prefix = "End_";
			if (ingestionJobStatus.size() >= prefix.size() && 0 == ingestionJobStatus.compare(0, prefix.size(), prefix))
			{
				LOG_INFO(
					"addContentIngestionJobKey finished"
					", ingestionJobKey: {}"
					", addContentIngestionJobKey: {}"
					", ingestionJobStatus: {}",
					ingestionJobKey, addContentIngestionJobKey, ingestionJobStatus
				);

				addContentFinished++;
			}
			else
			{
				int secondsToSleep = 5;

				LOG_INFO(
					"addContentIngestionJobKey not finished, sleeping..."
					", ingestionJobKey: {}"
					", addContentIngestionJobKey: {}"
					", ingestionJobStatus: {}"
					", secondsToSleep: {}",
					ingestionJobKey, addContentIngestionJobKey, ingestionJobStatus, secondsToSleep
				);

				this_thread::sleep_for(chrono::seconds(secondsToSleep));
			}
		}

		LOG_INFO(
			"Waiting result..."
			", ingestionJobKey: {}"
			", addContentFinished: {}"
			", maxSecondsWaiting: {}"
			", elapsedInSeconds: {}",
			ingestionJobKey, addContentFinished, maxSecondsWaiting,
			chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startWaiting).count()
		);
	}
	catch (runtime_error &e)
	{
		string errorMessage = std::format(
			"waiting addContent ingestion failed"
			", ingestionJobKey: {}"
			", encodingJobKey: {}",
			ingestionJobKey, encodingJobKey
		);
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

int64_t FFMPEGEncoderTask::ingestContentByPushingBinary(
	int64_t ingestionJobKey, string workflowMetadata, string fileFormat, string binaryPathFileName, int64_t binaryFileSizeInBytes, int64_t userKey,
	const string& apiKey, string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL
) const
{
	LOG_INFO(
		"Received ingestContentByPushingBinary"
		", ingestionJobKey: {}"
		", fileFormat: {}"
		", binaryPathFileName: {}"
		", binaryFileSizeInBytes: {}"
		", userKey: {}"
		", mmsWorkflowIngestionURL: {}"
		", mmsBinaryIngestionURL: {}",
		ingestionJobKey, fileFormat, binaryPathFileName, binaryFileSizeInBytes, userKey, mmsWorkflowIngestionURL, mmsBinaryIngestionURL
	);

	int64_t addContentIngestionJobKey = -1;
	try
	{
		vector<string> otherHeaders;
		string sResponse =
			CurlWrapper::httpPostString(
				mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey), workflowMetadata,
				"application/json", // contentType
				otherHeaders, std::format(", ingestionJobKey: {}", ingestionJobKey),
				3 // maxRetryNumber
			)
				.second;

		addContentIngestionJobKey = getAddContentIngestionJobKey(ingestionJobKey, sResponse);
	}
	catch (exception& e)
	{
		LOG_ERROR(
			"Ingestion workflow failed (runtime_error)"
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
		string errorMessage = std::format(
			"Ingested URL failed, addContentIngestionJobKey is not valid"
			", ingestionJobKey: {}",
			ingestionJobKey
		);
		LOG_ERROR(errorMessage);

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

					size_t endOfPathIndex = localBinaryPathFileName.find_last_of('/');
					if (endOfPathIndex == string::npos)
					{
						string errorMessage = std::format(
							"No localVariantPathDirectory found"
							", ingestionJobKey: {}"
							", localBinaryPathFileName: {}",
							ingestionJobKey, localBinaryPathFileName
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					string localVariantPathDirectory = localBinaryPathFileName.substr(0, endOfPathIndex);

					executeCommand = std::format("tar cfz {} -C {} content", localBinaryPathFileName, localVariantPathDirectory);
					LOG_INFO(
						"Start tar command "
						", executeCommand: {}",
						executeCommand
					);
					chrono::system_clock::time_point startTar = chrono::system_clock::now();
					int executeCommandStatus = ProcessUtility::execute(executeCommand);
					chrono::system_clock::time_point endTar = chrono::system_clock::now();
					LOG_INFO(
						"End tar command "
						", executeCommand: {}"
						", @MMS statistics@ - tarDuration (millisecs): @{}@",
						executeCommand, chrono::duration_cast<chrono::milliseconds>(endTar - startTar).count()
					);
					if (executeCommandStatus != 0)
					{
						string errorMessage = std::format(
							"ProcessUtility::execute failed"
							", ingestionJobKey: {}"
							", executeCommandStatus: {}"
							", executeCommand: {}",
							ingestionJobKey, executeCommandStatus, executeCommand
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}

#ifdef SAFEFILESYSTEMTHREAD
					localBinaryFileSizeInBytes =
						SafeFileSystem::fileSizeThread(localBinaryPathFileName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#elif SAFEFILESYSTEMPROCESS
					localBinaryFileSizeInBytes =
						SafeFileSystem::fileSizeProcess(localBinaryPathFileName, 10, std::format(", ingestionJobKey: {}", ingestionJobKey));
#else
					localBinaryFileSizeInBytes = fs::file_size(localBinaryPathFileName);
#endif
				}
				catch (runtime_error &e)
				{
					string errorMessage = std::format(
						"tar command failed"
						", ingestionJobKey: {}"
						", executeCommand: {}"
						", exception: {}",
						ingestionJobKey, executeCommand, e.what()
					);
					LOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}

		mmsBinaryURL = mmsBinaryIngestionURL + "/" + to_string(addContentIngestionJobKey);

		string sResponse = CurlWrapper::httpPostFileSplittingInChunks(
			mmsBinaryURL, _mmsBinaryTimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey), localBinaryPathFileName,
			[](int, int) { return true; }, std::format(", ingestionJobKey: {}", ingestionJobKey),
			3 // maxRetryNumber
		);

		if (fileFormat == "hls")
		{
			LOG_INFO(
				"remove"
				", localBinaryPathFileName: {}",
				localBinaryPathFileName
			);
			fs::remove_all(localBinaryPathFileName);
		}
	}
	catch (exception& e)
	{
		LOG_ERROR(
			"Ingestion binary failed"
			", ingestionJobKey: {}"
			", mmsBinaryURL: {}"
			", workflowMetadata: {}"
			", exception: {}",
			ingestionJobKey, mmsBinaryURL, workflowMetadata, e.what()
		);

		if (fileFormat == "hls")
		{
			// it is useless to remove the generated tar.gz file because the parent staging directory
			// will be removed. Also here we should add a bool above to be sure the tar was successful
			/*
			info(__FILEREF__ + "remove"
				+ ", localBinaryPathFileName: " + localBinaryPathFileName
			);
			bool exceptionInCaseOfError = false;
			fs::remove_all(localBinaryPathFileName, exceptionInCaseOfError);
			*/
		}

		throw;
	}

	return addContentIngestionJobKey;
}

string FFMPEGEncoderTask::buildAddContentIngestionWorkflow(
	int64_t ingestionJobKey, string label, string fileFormat, string ingester,

	// in case of a new content
	string sourceURL, // if empty it means binary is ingested later (PUSH)
	string title, const json& userDataRoot,
	const json& ingestedParametersRoot, // it could be also nullValue
	int64_t encodingProfileKey,

	int64_t variantOfMediaItemKey // in case of a variant, otherwise -1
)
{
	string workflowMetadata;
	try
	{
		json addContentRoot;

		string field = "label";
		addContentRoot[field] = label;

		field = "type";
		addContentRoot[field] = "Add-Content";

		json addContentParametersRoot;

		if (variantOfMediaItemKey != -1)
		{
			// it is a Variant

			field = "fileFormat";
			addContentParametersRoot[field] = fileFormat;

			field = "ingester";
			addContentParametersRoot[field] = ingester;

			field = "variantOfMediaItemKey";
			addContentParametersRoot[field] = variantOfMediaItemKey;

			field = "encodingProfileKey";
			addContentParametersRoot[field] = encodingProfileKey;

			if (userDataRoot != nullptr)
			{
				field = "userData";
				addContentParametersRoot[field] = userDataRoot;
			}
		}
		else
		{
			// it is a new content

			if (ingestedParametersRoot != nullptr)
			{
				addContentParametersRoot = ingestedParametersRoot;

				field = "internalMMS";
				if (JSONUtils::isPresent(addContentParametersRoot, field))
					addContentParametersRoot.erase(field);
			}

			field = "fileFormat";
			addContentParametersRoot[field] = fileFormat;

			field = "ingester";
			addContentParametersRoot[field] = ingester;

			if (!sourceURL.empty())
			{
				// string sourceURL = string("move") + "://" + imagesDirectory + "/" + generatedFrameFileName;
				field = "sourceURL";
				addContentParametersRoot[field] = sourceURL;
			}

			if (!title.empty())
			{
				field = "title";
				addContentParametersRoot[field] = title;
			}

			if (userDataRoot != nullptr)
			{
				field = "userData";
				addContentParametersRoot[field] = userDataRoot;
			}

			if (encodingProfileKey != -1)
			{
				field = "encodingProfileKey";
				addContentParametersRoot[field] = encodingProfileKey;
			}
		}

		field = "parameters";
		addContentRoot[field] = addContentParametersRoot;

		json workflowRoot;

		field = "label";
		workflowRoot[field] = label;

		field = "type";
		workflowRoot[field] = "Workflow";

		field = "task";
		workflowRoot[field] = addContentRoot;

		{
			workflowMetadata = JSONUtils::toString(workflowRoot);
		}

		LOG_INFO(
			"buildAddContentIngestionWorkflow"
			", ingestionJobKey: {}"
			", workflowMetadata: {}",
			ingestionJobKey, workflowMetadata
		);

		return workflowMetadata;
	}
	catch (exception& e)
	{
		LOG_ERROR(
			"buildAddContentIngestionWorkflow failed"
			", ingestionJobKey: {}"
			", workflowMetadata: {}"
			", exception: {}",
			ingestionJobKey, workflowMetadata, e.what()
		);

		throw;
	}
}

struct FFMpegProgressData
{
	int64_t _ingestionJobKey{};
	chrono::system_clock::time_point _lastTimeProgressUpdate;
	double _lastPercentageUpdated{};
};
static int progressDownloadCallback2(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{

	auto *progressData = static_cast<FFMpegProgressData *>(clientp);

	int progressUpdatePeriodInSeconds = 15;

	chrono::system_clock::time_point now = chrono::system_clock::now();

	if (dltotal != 0 && (dltotal == dlnow || now - progressData->_lastTimeProgressUpdate >= chrono::seconds(progressUpdatePeriodInSeconds)))
	{
		double progress = dltotal == 0 ? 0 : (dlnow / dltotal) * 100;
		// int downloadingPercentage = floorf(progress * 100) / 100;
		// this is to have one decimal in the percentage
		double downloadingPercentage = ((double)((int)(progress * 10))) / 10;

		LOG_INFO(
			"progressDownloadCallback. Download still running"
			", ingestionJobKey: {}"
			", downloadingPercentage: {}"
			", dltotal: {}"
			", dlnow: {}"
			", ultotal: {}"
			", ulnow: {}",
			progressData->_ingestionJobKey, downloadingPercentage, dltotal, dlnow, ultotal, ulnow
		);

		progressData->_lastTimeProgressUpdate = now;

		if (progressData->_lastPercentageUpdated != downloadingPercentage)
		{
			/*
			LOG_INFO(
				"progressDownloadCallback. Update IngestionJob"
				", ingestionJobKey: {}"
				", downloadingPercentage: {}",
				progressData->_ingestionJobKey, downloadingPercentage
			);
			downloadingStoppedByUser = _mmsEngineDBFacade->updateIngestionJobSourceDownloadingInProgress (
				ingestionJobKey, downloadingPercentage);
			*/

			progressData->_lastPercentageUpdated = downloadingPercentage;
		}

		// if (downloadingStoppedByUser)
		//     return 1;   // stop downloading
	}

	return 0;
}

string FFMPEGEncoderTask::downloadMediaFromMMS(
	int64_t ingestionJobKey, int64_t encodingJobKey, const shared_ptr<FFMpegWrapper> &ffmpeg, const string &sourceFileExtension,
	const string& sourcePhysicalDeliveryURL, const string &destAssetPathName
)
{
	string localDestAssetPathName = destAssetPathName;

	bool isSourceStreaming = false;
	if (sourceFileExtension == ".m3u8")
		isSourceStreaming = true;

	LOG_INFO(
		"downloading source content"
		", ingestionJobKey: {}"
		", sourcePhysicalDeliveryURL: {}"
		", localDestAssetPathName: {}"
		", isSourceStreaming: {}",
		ingestionJobKey, sourcePhysicalDeliveryURL, localDestAssetPathName, isSourceStreaming
	);

	if (isSourceStreaming)
	{
		// regenerateTimestamps: see docs/TASK_01_Add_Content_JSON_Format.txt
		bool regenerateTimestamps = false;

		localDestAssetPathName = localDestAssetPathName + ".mp4";

		ffmpeg->streamingToFile(ingestionJobKey, regenerateTimestamps, sourcePhysicalDeliveryURL, localDestAssetPathName);
	}
	else
	{
		FFMpegProgressData progressData;
		progressData._ingestionJobKey = ingestionJobKey;
		progressData._lastTimeProgressUpdate = chrono::system_clock::now();
		progressData._lastPercentageUpdated = -1.0;

		constexpr long timeoutInSeconds = 960;
		CurlWrapper::downloadFile(
			sourcePhysicalDeliveryURL, localDestAssetPathName, progressDownloadCallback2, &progressData, 500,
			std::format(", ingestionJobKey: {}", ingestionJobKey), timeoutInSeconds, 3
		);
	}

	LOG_INFO(
		"downloaded source content"
		", ingestionJobKey: {}"
		", sourcePhysicalDeliveryURL: {}"
		", localDestAssetPathName: {}"
		", isSourceStreaming: {}",
		ingestionJobKey, sourcePhysicalDeliveryURL, localDestAssetPathName, isSourceStreaming
	);

	return localDestAssetPathName;
}

long FFMPEGEncoderTask::getFreeTvChannelPortOffset(mutex *tvChannelsPortsMutex, long tvChannelPort_CurrentOffset) const
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
			const filesystem::path tvChannelConfigurationDirectory(_tvChannelConfigurationDirectory);
			if (filesystem::exists(tvChannelConfigurationDirectory) && filesystem::is_directory(tvChannelConfigurationDirectory))
			{
				// check if this port is already used

				for (const auto &directoryEntry : filesystem::directory_iterator(tvChannelConfigurationDirectory))
				{
					auto fileName = directoryEntry.path().filename();

					LOG_INFO(
						"read directory"
						", fileName: {}",
						fileName.string()
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
						LOG_INFO(
							"getFreeTvChannelPortOffset. Port is already used"
							", portToLookFor: {}",
							portToLookFor
						);
						portAlreadyUsed = true;

						break;
					}
				}
			}
		}
		catch (filesystem::filesystem_error &e)
		{
			string errorMessage = std::format(
				"getFreeTvChannelPortOffset. File system error"
				", e.what(): {}",
				e.what()
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	} while (portAlreadyUsed && attemptNumber < _tvChannelPort_MaxNumberOfOffsets);

	LOG_INFO(
		"getFreeTvChannelPortOffset"
		", portAlreadyUsed: {}"
		", tvChannelPort_CurrentOffset: {}"
		", localTvChannelPort_CurrentOffset: {}",
		portAlreadyUsed, tvChannelPort_CurrentOffset, localTvChannelPort_CurrentOffset
	);

	return localTvChannelPort_CurrentOffset;
}

void FFMPEGEncoderTask::createOrUpdateTVDvbLastConfigurationFile(
	int64_t ingestionJobKey, int64_t encodingJobKey, string multicastIP, string multicastPort, string tvType, int64_t tvServiceId,
	int64_t tvFrequency, int64_t tvSymbolRate, int64_t tvBandwidthInMhz, string tvModulation, int tvVideoPid, int tvAudioItalianPid, bool toBeAdded
)
{
	try
	{
		LOG_INFO(
			"Received createOrUpdateTVDvbLastConfigurationFile"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", multicastIP: {}"
			", multicastPort: {}"
			", tvType: {}"
			", tvServiceId: {}"
			", tvFrequency: {}"
			", tvSymbolRate: {}"
			", tvBandwidthInMhz: {}"
			", tvModulation: {}"
			", tvVideoPid: {}"
			", tvAudioItalianPid: {}"
			", toBeAdded: {}",
			ingestionJobKey, encodingJobKey, multicastIP, multicastPort, tvType, tvServiceId, tvFrequency, tvSymbolRate, tvBandwidthInMhz,
			tvModulation, tvVideoPid, tvAudioItalianPid, toBeAdded
		);

		string localModulation;

		// dvblast modulation: qpsk|psk_8|apsk_16|apsk_32
		if (!tvModulation.empty())
		{
			if (tvModulation == "PSK/8")
				localModulation = "psk_8";
			else if (tvModulation == "QAM/64")
				localModulation = "QAM_64";
			else if (tvModulation == "QPSK")
				localModulation = "qpsk";
			else
			{
				string errorMessage = std::format(
					"createOrUpdateTVDvbLastConfigurationFile. Unknown modulation"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", tvModulation: {}",
					ingestionJobKey, encodingJobKey, tvModulation
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (!fs::exists(_tvChannelConfigurationDirectory))
		{
			LOG_INFO(
				"createOrUpdateTVDvbLastConfigurationFile. Create directory"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", _tvChannelConfigurationDirectory: {}",
				ingestionJobKey, encodingJobKey, _tvChannelConfigurationDirectory
			);

			fs::create_directories(_tvChannelConfigurationDirectory);
			fs::permissions(
				_tvChannelConfigurationDirectory,
				fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec | fs::perms::group_read | fs::perms::group_write |
					fs::perms::group_exec | fs::perms::others_read | fs::perms::others_write | fs::perms::others_exec,
				fs::perm_options::replace
			);
		}

		string dvblastConfigurationPathName = _tvChannelConfigurationDirectory + "/" + to_string(tvFrequency);
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
			while (getline(ifConfigurationFile, configuration))
			{
				string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

				if (trimmedConfiguration.size() > 10)
					vConfiguration.push_back(trimmedConfiguration);
			}
			ifConfigurationFile.close();
			if (!changedFileFound) // .txt found
			{
				LOG_INFO(
					"createOrUpdateTVDvbLastConfigurationFile. Remove dvblast configuration file to create the new one"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", dvblastConfigurationPathName: {}",
					ingestionJobKey, encodingJobKey, std::format("{}.txt", dvblastConfigurationPathName)
				);

				fs::remove_all(dvblastConfigurationPathName + ".txt");
			}
		}

		string newConfiguration = std::format("{}:{} 1 {} {},{}", multicastIP, multicastPort, tvServiceId, tvVideoPid, tvVideoPid, tvAudioItalianPid);

		LOG_INFO(
			"createOrUpdateTVDvbLastConfigurationFile. Creation dvblast configuration file"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", dvblastConfigurationPathName: {}"
			", newConfiguration: {}",
			ingestionJobKey, encodingJobKey, std::format("{}.changed", dvblastConfigurationPathName), newConfiguration
		);

		ofstream ofConfigurationFile(dvblastConfigurationPathName + ".changed", ofstream::trunc);
		if (!ofConfigurationFile)
		{
			string errorMessage = std::format(
				"createOrUpdateTVDvbLastConfigurationFile. Creation dvblast configuration file failed"
				", ingestionJobKey: {}"
				", encodingJobKey: "
				", dvblastConfigurationPathName: {}.changed",
				ingestionJobKey, encodingJobKey, dvblastConfigurationPathName
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		bool configurationAlreadyPresent = false;
		bool wroteFirstLine = false;
		for (const string& configuration : vConfiguration)
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
		string errorMessage = std::format(
			"createOrUpdateTVDvbLastConfigurationFile failed"
			", ingestionJobKey: {}"
			", encodingJobKey: {}",
			ingestionJobKey, encodingJobKey
		);
		LOG_ERROR(errorMessage);
	}
}

pair<string, string> FFMPEGEncoderTask::getTVMulticastFromDvblastConfigurationFile(
	int64_t ingestionJobKey, int64_t encodingJobKey, string tvType, int64_t tvServiceId, int64_t tvFrequency, int64_t tvSymbolRate,
	int64_t tvBandwidthInMhz, string tvModulation
) const
{
	string multicastIP;
	string multicastPort;

	try
	{
		LOG_INFO(
			"Received getTVMulticastFromDvblastConfigurationFile"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", tvType: {}"
			", tvServiceId: {}"
			", tvFrequency: {}"
			", tvSymbolRate: {}"
			", tvBandwidthInMhz: {}"
			", tvModulation: {}",
			ingestionJobKey, encodingJobKey, tvType, tvServiceId, tvFrequency, tvSymbolRate, tvBandwidthInMhz, tvModulation
		);

		string localModulation;

		// dvblast modulation: qpsk|psk_8|apsk_16|apsk_32
		if (!tvModulation.empty())
		{
			if (tvModulation == "PSK/8")
				localModulation = "psk_8";
			else if (tvModulation == "QAM/64")
				localModulation = "QAM_64";
			else if (tvModulation == "QPSK")
				localModulation = "qpsk";
			else
			{
				string errorMessage = std::format(
					"unknown modulation"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", tvModulation: {}",
					ingestionJobKey, encodingJobKey, tvModulation
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string dvblastConfigurationPathName = _tvChannelConfigurationDirectory + "/" + to_string(tvFrequency);
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
			while (getline(configurationFile, configuration))
			{
				string trimmedConfiguration = StringUtils::trimNewLineAndTabToo(configuration);

				// configuration is like: 239.255.1.1:8008 1 3401 501,601
				istringstream iss(trimmedConfiguration);
				vector<string> configurationPieces;
				copy(istream_iterator<std::string>(iss), istream_iterator<std::string>(), back_inserter(configurationPieces));
				if (configurationPieces.size() < 3)
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

		LOG_INFO(
			"Received getTVMulticastFromDvblastConfigurationFile"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", tvType: {}"
			", tvServiceId: {}"
			", tvFrequency: {}"
			", tvSymbolRate: {}"
			", tvBandwidthInMhz: {}"
			", tvModulation: {}"
			", multicastIP: {}"
			", multicastPort: {}",
			ingestionJobKey, encodingJobKey, tvType, tvServiceId, tvFrequency, tvSymbolRate, tvBandwidthInMhz, tvModulation, multicastIP,
			multicastPort
		);
	}
	catch (...)
	{
		// make sure do not raise an exception to the calling method to avoid
		// to interrupt "closure" encoding procedure
		string errorMessage = std::format(
			"getTVMulticastFromDvblastConfigurationFile failed"
			", ingestionJobKey: {}"
			", encodingJobKey: {}",
			ingestionJobKey, encodingJobKey
		);
		LOG_ERROR(errorMessage);
	}

	return make_pair(multicastIP, multicastPort);
}

void FFMPEGEncoderTask::addEncodingCompleted() const
{
	lock_guard<mutex> locker(*_encodingCompletedMutex);

	auto encodingCompleted = make_shared<EncodingCompleted>();

	encodingCompleted->_encodingJobKey = _encoding->_encodingJobKey;
	encodingCompleted->_completedWithError = _completedWithError;
	// encodingCompleted->_errorMessage = _encoding->_lastErrorMessage;
	encodingCompleted->_killedByUser = _killedByUser;
	encodingCompleted->_killTypeReceived = _encoding->_killTypeReceived;
	// encodingCompleted->_urlForbidden = _urlForbidden;
	// encodingCompleted->_urlNotFound = _urlNotFound;
	encodingCompleted->_timestamp = chrono::system_clock::now();

	{
		encodingCompleted->_callbackData = _encoding->_callbackData->clone();
		/*
		if (encodingCompleted->_callbackData->getFinished())
		{
			encodingCompleted->_urlForbidden = encodingCompleted->_callbackData->getUrlForbidden();
			encodingCompleted->_urlNotFound = encodingCompleted->_callbackData->getUrlNotFound();
		}
		*/
	}

	_encodingCompletedMap->insert(make_pair(encodingCompleted->_encodingJobKey, encodingCompleted));

	LOG_INFO(
		"addEncodingCompleted"
		", ingestionJobKey: {}"
		", encodingJobKey: {}"
		", encodingCompletedMap size: {}",
		_encoding->_ingestionJobKey, _encoding->_encodingJobKey, _encodingCompletedMap->size()
	);
}

void FFMPEGEncoderTask::removeEncodingCompletedIfPresent() const
{

	lock_guard<mutex> locker(*_encodingCompletedMutex);

	auto it = _encodingCompletedMap->find(_encoding->_encodingJobKey);
	if (it != _encodingCompletedMap->end())
	{
		_encodingCompletedMap->erase(it);

		LOG_INFO(
			"removeEncodingCompletedIfPresent"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", encodingCompletedMap size: {}",
			_encoding->_ingestionJobKey, _encoding->_encodingJobKey, _encodingCompletedMap->size()
		);
	}
}
