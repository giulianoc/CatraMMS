
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

void MMSEngineProcessor::manageSlideShowTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() == 0)
		{
			string errorMessage = string() + "No images found" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
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

		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot;
		{
			// This task shall contain EncodingProfileKey or
			// EncodingProfileLabel. We cannot have EncodingProfilesSetKey
			// because we replaced it with a GroupOfTasks
			//  having just EncodingProfileKey

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

		json imagesRoot = json::array();
		json audiosRoot = json::array();
		float shortestAudioDurationInSeconds = -1.0;

		for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType : dependencies)
		{
			MMSEngineDBFacade::ContentType referenceContentType;
			int64_t sourceMediaItemKey;
			int64_t sourcePhysicalPathKey;
			string sourceAssetPathName;
			string sourceRelativePath;
			string sourceFileName;
			string sourceFileExtension;
			int64_t sourceDurationInMilliSecs;
			string sourcePhysicalDeliveryURL;
			string sourceTranscoderStagingAssetPathName;
			bool stopIfReferenceProcessingError;
			tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
				processDependencyInfo(workspace, ingestionJobKey, keyAndDependencyType);
			tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
				sourceFileExtension, sourceDurationInMilliSecs, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName,
				stopIfReferenceProcessingError) = dependencyInfo;

			if (referenceContentType == MMSEngineDBFacade::ContentType::Image)
			{
				json imageRoot;

				imageRoot["sourceAssetPathName"] = sourceAssetPathName;
				imageRoot["sourceFileExtension"] = sourceFileExtension;
				imageRoot["sourcePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;
				imageRoot["sourceTranscoderStagingAssetPathName"] = sourceTranscoderStagingAssetPathName;

				imagesRoot.push_back(imageRoot);
			}
			else if (referenceContentType == MMSEngineDBFacade::ContentType::Audio)
			{
				json audioRoot;

				audioRoot["sourceAssetPathName"] = sourceAssetPathName;
				audioRoot["sourceFileExtension"] = sourceFileExtension;
				audioRoot["sourcePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;
				audioRoot["sourceTranscoderStagingAssetPathName"] = sourceTranscoderStagingAssetPathName;

				audiosRoot.push_back(audioRoot);

				if (shortestAudioDurationInSeconds == -1.0 || shortestAudioDurationInSeconds > sourceDurationInMilliSecs / 1000)
					shortestAudioDurationInSeconds = sourceDurationInMilliSecs / 1000;
			}
			else
			{
				string errorMessage = string() +
									  "It is not possible to build a slideshow with a media that "
									  "is not an Image-Audio" +
									  ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		// 2023-04-23: qui dovremmo utilizzare il metodo
		// getEncodedFileExtensionByEncodingProfile
		//	che ritorna l'estensione in base all'encodingProfile.
		//	Non abbiamo ancora utilizzato questo metodo perchè si dovrebbe
		// verificare che funziona 	anche per estensioni diverse da .mp4
		string targetFileFormat = ".mp4";
		string encodedFileName = to_string(ingestionJobKey) + "_slideShow" + targetFileFormat;

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

		_mmsEngineDBFacade->addEncoding_SlideShowJob(
			workspace, ingestionJobKey, encodingProfileKey, encodingProfileDetailsRoot, targetFileFormat, imagesRoot, audiosRoot,
			shortestAudioDurationInSeconds, encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL,
			_mmsBinaryIngestionURL, _mmsIngestionURL, encodingPriority
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageSlideShowTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageSlideShowTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
