
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"

void MMSEngineProcessor::manageIntroOutroOverlayTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 3)
		{
			string errorMessage = string() + "Wrong number of dependencies" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		int64_t introSourceMediaItemKey;
		int64_t introSourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType introReferenceContentType;
		string introSourceAssetPathName;
		string introSourceRelativePath;
		string introSourceFileName;
		string introSourceFileExtension;
		int64_t introSourceDurationInMilliSeconds;
		string introSourcePhysicalDeliveryURL;
		string introSourceTranscoderStagingAssetPathName;
		bool introStopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> introDependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(introSourceMediaItemKey, introSourcePhysicalPathKey, introReferenceContentType, introSourceAssetPathName, introSourceRelativePath,
			introSourceFileName, introSourceFileExtension, introSourceDurationInMilliSeconds, introSourcePhysicalDeliveryURL,
			introSourceTranscoderStagingAssetPathName, introStopIfReferenceProcessingError) = introDependencyInfo;

		int64_t mainSourceMediaItemKey;
		int64_t mainSourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType mainReferenceContentType;
		string mainSourceAssetPathName;
		string mainSourceRelativePath;
		string mainSourceFileName;
		string mainSourceFileExtension;
		int64_t mainSourceDurationInMilliSeconds;
		string mainSourcePhysicalDeliveryURL;
		string mainSourceTranscoderStagingAssetPathName;
		bool mainStopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> mainDependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[1]);
		tie(mainSourceMediaItemKey, mainSourcePhysicalPathKey, mainReferenceContentType, mainSourceAssetPathName, mainSourceRelativePath,
			mainSourceFileName, mainSourceFileExtension, mainSourceDurationInMilliSeconds, mainSourcePhysicalDeliveryURL,
			mainSourceTranscoderStagingAssetPathName, mainStopIfReferenceProcessingError) = mainDependencyInfo;

		int64_t outroSourceMediaItemKey;
		int64_t outroSourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType outroReferenceContentType;
		string outroSourceAssetPathName;
		string outroSourceRelativePath;
		string outroSourceFileName;
		string outroSourceFileExtension;
		int64_t outroSourceDurationInMilliSeconds;
		string outroSourcePhysicalDeliveryURL;
		string outroSourceTranscoderStagingAssetPathName;
		bool outroStopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> outroDependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[2]);
		tie(outroSourceMediaItemKey, outroSourcePhysicalPathKey, outroReferenceContentType, outroSourceAssetPathName, outroSourceRelativePath,
			outroSourceFileName, outroSourceFileExtension, outroSourceDurationInMilliSeconds, outroSourcePhysicalDeliveryURL,
			outroSourceTranscoderStagingAssetPathName, outroStopIfReferenceProcessingError) = outroDependencyInfo;

		int64_t encodingProfileKey;
		json encodingProfileDetailsRoot;
		{
			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				MMSEngineDBFacade::ContentType videoContentType = MMSEngineDBFacade::ContentType::Video;
				encodingProfileKey =
					_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, videoContentType, encodingProfileLabel);
			}
			else
			{
				string errorMessage = string() + "Both fields are not present or it is null" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", Field: " + keyField +
									  ", Field: " + labelField;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		string encodedFileName = to_string(ingestionJobKey) + "_introOutroOverlay" +
								 getEncodedFileExtensionByEncodingProfile(encodingProfileDetailsRoot); // mainSourceFileExtension;

		string encodedTranscoderStagingAssetPathName; // used in case of
													  // external encoder
		fs::path encodedNFSStagingAssetPathName;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				// as specified by doc
				// (TASK_01_Add_Content_JSON_Format.txt), in case of hls and
				// external encoder (binary is ingested through PUSH), the
				// directory inside the tar.gz has to be 'content'
				encodedFileName, // content
				-1,				 // _encodingItem->_mediaItemKey, not used because
								 // encodedFileName is not ""
				-1,				 // _encodingItem->_physicalPathKey, not used because
								 // encodedFileName is not ""
				removeLinuxPathIfExist
			);

			encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
		}

		_mmsEngineDBFacade->addEncoding_IntroOutroOverlayJob(
			workspace, ingestionJobKey, encodingProfileKey, encodingProfileDetailsRoot,

			introSourcePhysicalPathKey, introSourceAssetPathName, introSourceFileExtension, introSourceDurationInMilliSeconds,
			introSourcePhysicalDeliveryURL, introSourceTranscoderStagingAssetPathName,

			mainSourcePhysicalPathKey, mainSourceAssetPathName, mainSourceFileExtension, mainSourceDurationInMilliSeconds,
			mainSourcePhysicalDeliveryURL, mainSourceTranscoderStagingAssetPathName,

			outroSourcePhysicalPathKey, outroSourceAssetPathName, outroSourceFileExtension, outroSourceDurationInMilliSeconds,
			outroSourcePhysicalDeliveryURL, outroSourceTranscoderStagingAssetPathName,

			encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL, _mmsBinaryIngestionURL,
			_mmsIngestionURL,

			encodingPriority
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageIntroOutroOverlayTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageIntroOutroOverlayTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageOverlayImageOnVideoTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 2)
		{
			string errorMessage = string() + "Wrong number of dependencies" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		int64_t sourceMediaItemKey_1;
		int64_t sourcePhysicalPathKey_1;
		MMSEngineDBFacade::ContentType referenceContentType_1;
		string mmsSourceAssetPathName_1;
		string sourceFileName_1;
		string sourceFileExtension_1;
		int64_t sourceDurationInMilliSecs_1;
		string sourcePhysicalDeliveryURL_1;
		string sourceTranscoderStagingAssetPathName_1;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo_1 =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey_1, sourcePhysicalPathKey_1, referenceContentType_1, mmsSourceAssetPathName_1, ignore, sourceFileName_1,
			sourceFileExtension_1, sourceDurationInMilliSecs_1, sourcePhysicalDeliveryURL_1, sourceTranscoderStagingAssetPathName_1, ignore) =
			dependencyInfo_1;

		int64_t sourceMediaItemKey_2;
		int64_t sourcePhysicalPathKey_2;
		MMSEngineDBFacade::ContentType referenceContentType_2;
		string mmsSourceAssetPathName_2;
		string sourceFileName_2;
		string sourceFileExtension_2;
		int64_t sourceDurationInMilliSecs_2;
		string sourcePhysicalDeliveryURL_2;
		string sourceTranscoderStagingAssetPathName_2;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo_2 =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[1]);
		tie(sourceMediaItemKey_2, sourcePhysicalPathKey_2, referenceContentType_2, mmsSourceAssetPathName_2, ignore, sourceFileName_2,
			sourceFileExtension_2, sourceDurationInMilliSecs_2, sourcePhysicalDeliveryURL_2, sourceTranscoderStagingAssetPathName_2, ignore) =
			dependencyInfo_2;

		int64_t sourceVideoMediaItemKey;
		int64_t sourceVideoPhysicalPathKey;
		string mmsSourceVideoAssetPathName;
		string sourceVideoPhysicalDeliveryURL;
		string sourceVideoFileName;
		string sourceVideoFileExtension;
		string sourceVideoTranscoderStagingAssetPathName; // used in case of
														  // external encoder
		int64_t videoDurationInMilliSeconds;

		int64_t sourceImageMediaItemKey;
		int64_t sourceImagePhysicalPathKey;
		string mmsSourceImageAssetPathName;
		string sourceImagePhysicalDeliveryURL;

		if (referenceContentType_1 == MMSEngineDBFacade::ContentType::Video && referenceContentType_2 == MMSEngineDBFacade::ContentType::Image)
		{
			sourceVideoMediaItemKey = sourceMediaItemKey_1;
			sourceVideoPhysicalPathKey = sourcePhysicalPathKey_1;
			mmsSourceVideoAssetPathName = mmsSourceAssetPathName_1;
			sourceVideoPhysicalDeliveryURL = sourcePhysicalDeliveryURL_1;
			sourceVideoFileName = sourceFileName_1;
			sourceVideoFileExtension = sourceFileExtension_1;
			sourceVideoTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_1;
			videoDurationInMilliSeconds = sourceDurationInMilliSecs_1;

			sourceImageMediaItemKey = sourceMediaItemKey_2;
			sourceImagePhysicalPathKey = sourcePhysicalPathKey_2;
			mmsSourceImageAssetPathName = mmsSourceAssetPathName_2;
			sourceImagePhysicalDeliveryURL = sourcePhysicalDeliveryURL_2;
		}
		else if (referenceContentType_1 == MMSEngineDBFacade::ContentType::Image && referenceContentType_2 == MMSEngineDBFacade::ContentType::Video)
		{
			sourceVideoMediaItemKey = sourceMediaItemKey_2;
			sourceVideoPhysicalPathKey = sourcePhysicalPathKey_2;
			mmsSourceVideoAssetPathName = mmsSourceAssetPathName_2;
			sourceVideoPhysicalDeliveryURL = sourcePhysicalDeliveryURL_2;
			sourceVideoFileName = sourceFileName_2;
			sourceVideoFileExtension = sourceFileExtension_2;
			sourceVideoTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_2;
			videoDurationInMilliSeconds = sourceDurationInMilliSecs_2;

			sourceImageMediaItemKey = sourceMediaItemKey_1;
			sourceImagePhysicalPathKey = sourcePhysicalPathKey_1;
			mmsSourceImageAssetPathName = mmsSourceAssetPathName_1;
			sourceImagePhysicalDeliveryURL = sourcePhysicalDeliveryURL_1;
		}
		else
		{
			string errorMessage =
				string() + "OverlayImageOnVideo is not receiving one Video and one Image" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", referenceContentType_1: " + MMSEngineDBFacade::toString(referenceContentType_1) +
				", sourceMediaItemKey_1: " + to_string(sourceMediaItemKey_1) + ", sourcePhysicalPathKey_1: " + to_string(sourcePhysicalPathKey_1) +
				", contentType_2: " + MMSEngineDBFacade::toString(referenceContentType_2) +
				", sourceMediaItemKey_2: " + to_string(sourceMediaItemKey_2) + ", sourcePhysicalPathKey_2: " + to_string(sourcePhysicalPathKey_2);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot = nullptr;
		{
			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				MMSEngineDBFacade::ContentType videoContentType = MMSEngineDBFacade::ContentType::Video;
				encodingProfileKey =
					_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, videoContentType, encodingProfileLabel);
			}

			if (encodingProfileKey != -1)
			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		// 2023-04-23: qui dovremmo utilizzare il metodo
		// getEncodedFileExtensionByEncodingProfile
		//	che ritorna l'estensione in base all'encodingProfile.
		//	Non abbiamo ancora utilizzato questo metodo perchè si dovrebbe
		// verificare che funziona 	anche per estensioni diverse da
		// sourceVideoFileExtension
		string encodedFileName = to_string(ingestionJobKey) + "_overlayedimage" + sourceVideoFileExtension;

		string encodedTranscoderStagingAssetPathName; // used in case of
													  // external encoder
		fs::path encodedNFSStagingAssetPathName;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				// as specified by doc
				// (TASK_01_Add_Content_JSON_Format.txt), in case of hls and
				// external encoder (binary is ingested through PUSH), the
				// directory inside the tar.gz has to be 'content'
				encodedFileName, // content
				-1,				 // _encodingItem->_mediaItemKey, not used because
								 // encodedFileName is not ""
				-1,				 // _encodingItem->_physicalPathKey, not used because
								 // encodedFileName is not ""
				removeLinuxPathIfExist
			);

			fs::path workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(workspace);
			encodedNFSStagingAssetPathName = workspaceIngestionRepository / encodedFileName;
		}

		_mmsEngineDBFacade->addEncoding_OverlayImageOnVideoJob(
			workspace, ingestionJobKey, encodingProfileKey, encodingProfileDetailsRoot, sourceVideoMediaItemKey, sourceVideoPhysicalPathKey,
			videoDurationInMilliSeconds, mmsSourceVideoAssetPathName, sourceVideoPhysicalDeliveryURL, sourceVideoFileExtension,
			sourceImageMediaItemKey, sourceImagePhysicalPathKey, mmsSourceImageAssetPathName, sourceImagePhysicalDeliveryURL,
			sourceVideoTranscoderStagingAssetPathName, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(),
			encodingPriority, _mmsWorkflowIngestionURL, _mmsBinaryIngestionURL, _mmsIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageOverlayImageOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageOverlayImageOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageOverlayTextOnVideoTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong number of dependencies" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		}
		else
		{
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));
		}

		int64_t sourceMediaItemKey;
		int64_t sourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType referenceContentType;
		string sourceAssetPathName;
		string sourceRelativePath;
		string sourceFileName;
		string sourceFileExtension;
		int64_t sourceDurationInMilliSecs;
		string sourcePhysicalDeliveryURL;
		string sourceTranscoderStagingAssetPathName;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
			sourceFileExtension, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName, ignore) = dependencyInfo;

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot = nullptr;
		{
			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				MMSEngineDBFacade::ContentType videoContentType = MMSEngineDBFacade::ContentType::Video;
				encodingProfileKey =
					_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, videoContentType, encodingProfileLabel);
			}

			if (encodingProfileKey != -1)
			{
				string jsonEncodingProfile;

				tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
					_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
				tie(ignore, ignore, ignore, jsonEncodingProfile) = encodingProfileDetails;

				encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
			}
		}

		// 2023-04-23: qui dovremmo utilizzare il metodo
		// getEncodedFileExtensionByEncodingProfile
		//	che ritorna l'estensione in base all'encodingProfile.
		//	Non abbiamo ancora utilizzato questo metodo perchè si dovrebbe
		// verificare che funziona 	anche per estensioni diverse da
		// sourceFileExtension
		string encodedFileName = to_string(ingestionJobKey) + "_overlayedText" + sourceFileExtension;

		string encodedTranscoderStagingAssetPathName; // used in case of
													  // external encoder
		fs::path encodedNFSStagingAssetPathName;
		{
			bool removeLinuxPathIfExist = false;
			bool neededForTranscoder = true;

			encodedTranscoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
				workspace->_directoryName,	// workspaceDirectoryName
				to_string(ingestionJobKey), // directoryNamePrefix
				"/",						// relativePath,
				// as specified by doc
				// (TASK_01_Add_Content_JSON_Format.txt), in case of hls and
				// external encoder (binary is ingested through PUSH), the
				// directory inside the tar.gz has to be 'content'
				encodedFileName, // content
				-1,				 // _encodingItem->_mediaItemKey, not used because
								 // encodedFileName is not ""
				-1,				 // _encodingItem->_physicalPathKey, not used because
								 // encodedFileName is not ""
				removeLinuxPathIfExist
			);

			encodedNFSStagingAssetPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace) / encodedFileName;
		}

		SPDLOG_INFO(
			string() + "addEncoding_OverlayTextOnVideoJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingPriority: " + MMSEngineDBFacade::toString(encodingPriority)
		);
		_mmsEngineDBFacade->addEncoding_OverlayTextOnVideoJob(
			workspace, ingestionJobKey, encodingPriority,

			encodingProfileKey, encodingProfileDetailsRoot,

			sourceAssetPathName, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceFileExtension,

			sourceTranscoderStagingAssetPathName, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(),
			_mmsWorkflowIngestionURL, _mmsBinaryIngestionURL, _mmsIngestionURL
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageOverlayTextOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageOverlayTextOnVideoTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
