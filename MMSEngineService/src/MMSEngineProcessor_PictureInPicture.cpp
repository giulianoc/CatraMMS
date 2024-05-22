
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"
/*
#include <stdio.h>

#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "FFMpeg.h"
#include "GEOInfoTimes.h"
#include "MMSCURL.h"
#include "PersistenceLock.h"
#include "ThreadsStatisticTimes.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/System.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
// #include "EMailSender.h"
#include "Magick++.h"
// #include <openssl/md5.h>
#include "spdlog/spdlog.h"
#include <openssl/evp.h>

#define MD5BUFFERSIZE 16384
*/

void MMSEngineProcessor::managePictureInPictureTask(
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
		string sourceAssetPathName_1;
		string sourceRelativePath_1;
		string sourceFileName_1;
		string sourceFileExtension_1;
		int64_t sourceDurationInMilliSeconds_1;
		string sourcePhysicalDeliveryURL_1;
		string sourceTranscoderStagingAssetPathName_1;
		bool stopIfReferenceProcessingError_1;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo_1 =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey_1, sourcePhysicalPathKey_1, referenceContentType_1, sourceAssetPathName_1, sourceRelativePath_1, sourceFileName_1,
			sourceFileExtension_1, sourceDurationInMilliSeconds_1, sourcePhysicalDeliveryURL_1, sourceTranscoderStagingAssetPathName_1,
			stopIfReferenceProcessingError_1) = dependencyInfo_1;

		int64_t sourceMediaItemKey_2;
		int64_t sourcePhysicalPathKey_2;
		MMSEngineDBFacade::ContentType referenceContentType_2;
		string sourceAssetPathName_2;
		string sourceRelativePath_2;
		string sourceFileName_2;
		string sourceFileExtension_2;
		int64_t sourceDurationInMilliSeconds_2;
		string sourcePhysicalDeliveryURL_2;
		string sourceTranscoderStagingAssetPathName_2;
		bool stopIfReferenceProcessingError_2;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo_2 =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[1]);
		tie(sourceMediaItemKey_2, sourcePhysicalPathKey_2, referenceContentType_2, sourceAssetPathName_2, sourceRelativePath_2, sourceFileName_2,
			sourceFileExtension_2, sourceDurationInMilliSeconds_2, sourcePhysicalDeliveryURL_2, sourceTranscoderStagingAssetPathName_2,
			stopIfReferenceProcessingError_2) = dependencyInfo_2;

		field = "SecondVideoOverlayedOnFirst";
		bool secondVideoOverlayedOnFirst = JSONUtils::asBool(parametersRoot, field, true);

		field = "SoundOfFirstVideo";
		bool soundOfFirstVideo = JSONUtils::asBool(parametersRoot, field, true);

		int64_t mainSourceMediaItemKey;
		int64_t mainSourcePhysicalPathKey;
		string mainSourceAssetPathName;
		int64_t mainSourceDurationInMilliSeconds;
		string mainSourceFileExtension;
		string mainSourcePhysicalDeliveryURL;
		string mainSourceTranscoderStagingAssetPathName;
		int64_t overlaySourceMediaItemKey;
		int64_t overlaySourcePhysicalPathKey;
		string overlaySourceAssetPathName;
		int64_t overlaySourceDurationInMilliSeconds;
		string overlaySourceFileExtension;
		string overlaySourcePhysicalDeliveryURL;
		string overlaySourceTranscoderStagingAssetPathName;
		bool soundOfMain;

		if (secondVideoOverlayedOnFirst)
		{
			mainSourceMediaItemKey = sourceMediaItemKey_1;
			mainSourcePhysicalPathKey = sourcePhysicalPathKey_1;
			mainSourceAssetPathName = sourceAssetPathName_1;
			mainSourceDurationInMilliSeconds = sourceDurationInMilliSeconds_1;
			mainSourceFileExtension = sourceFileExtension_1;
			mainSourcePhysicalDeliveryURL = sourcePhysicalDeliveryURL_1;
			mainSourceTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_1;

			overlaySourceMediaItemKey = sourceMediaItemKey_2;
			overlaySourcePhysicalPathKey = sourcePhysicalPathKey_2;
			overlaySourceAssetPathName = sourceAssetPathName_2;
			overlaySourceDurationInMilliSeconds = sourceDurationInMilliSeconds_2;
			overlaySourceFileExtension = sourceFileExtension_2;
			overlaySourcePhysicalDeliveryURL = sourcePhysicalDeliveryURL_2;
			overlaySourceTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_2;

			if (soundOfFirstVideo)
				soundOfMain = true;
			else
				soundOfMain = false;
		}
		else
		{
			mainSourceMediaItemKey = sourceMediaItemKey_2;
			mainSourcePhysicalPathKey = sourcePhysicalPathKey_2;
			mainSourceAssetPathName = sourceAssetPathName_2;
			mainSourceDurationInMilliSeconds = sourceDurationInMilliSeconds_2;
			mainSourceFileExtension = sourceFileExtension_2;
			mainSourcePhysicalDeliveryURL = sourcePhysicalDeliveryURL_2;
			mainSourceTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_2;

			overlaySourceMediaItemKey = sourceMediaItemKey_1;
			overlaySourcePhysicalPathKey = sourcePhysicalPathKey_1;
			overlaySourceAssetPathName = sourceAssetPathName_1;
			overlaySourceDurationInMilliSeconds = sourceDurationInMilliSeconds_1;
			overlaySourceFileExtension = sourceFileExtension_1;
			overlaySourcePhysicalDeliveryURL = sourcePhysicalDeliveryURL_1;
			overlaySourceTranscoderStagingAssetPathName = sourceTranscoderStagingAssetPathName_1;

			if (soundOfFirstVideo)
				soundOfMain = false;
			else
				soundOfMain = true;
		}

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot = nullptr;
		{
			// This task shall contain encodingProfileKey or
			// encodingProfileLabel. We cannot have encodingProfilesSetKey
			// because we replaced it with a GroupOfTasks
			//  having just encodingProfileKey

			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (JSONUtils::isMetadataPresent(parametersRoot, keyField))
			{
				encodingProfileKey = JSONUtils::asInt64(parametersRoot, keyField, 0);
			}
			else if (JSONUtils::isMetadataPresent(parametersRoot, labelField))
			{
				string encodingProfileLabel = JSONUtils::asString(parametersRoot, labelField, "");

				encodingProfileKey = _mmsEngineDBFacade->getEncodingProfileKeyByLabel(
					workspace->_workspaceKey, MMSEngineDBFacade::ContentType::Video, encodingProfileLabel
				);
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
		// mainSourceFileExtension
		string encodedFileName = to_string(ingestionJobKey) + "_pictureInPicture" + mainSourceFileExtension;

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

		_mmsEngineDBFacade->addEncoding_PictureInPictureJob(
			workspace, ingestionJobKey, mainSourceMediaItemKey, mainSourcePhysicalPathKey, mainSourceAssetPathName, mainSourceDurationInMilliSeconds,
			mainSourceFileExtension, mainSourcePhysicalDeliveryURL, mainSourceTranscoderStagingAssetPathName, overlaySourceMediaItemKey,
			overlaySourcePhysicalPathKey, overlaySourceAssetPathName, overlaySourceDurationInMilliSeconds, overlaySourceFileExtension,
			overlaySourcePhysicalDeliveryURL, overlaySourceTranscoderStagingAssetPathName, soundOfMain, encodingProfileKey,
			encodingProfileDetailsRoot, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL,
			_mmsBinaryIngestionURL, _mmsIngestionURL, encodingPriority
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "managePictureInPictureTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "managePictureInPictureTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
