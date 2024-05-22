
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

void MMSEngineProcessor::manageVideoSpeedTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong media number to be encoded" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		else
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));

		int64_t sourceMediaItemKey;
		int64_t sourcePhysicalPathKey;
		MMSEngineDBFacade::ContentType referenceContentType;
		string sourceAssetPathName;
		string sourceRelativePath;
		string sourceFileName;
		string sourceFileExtension;
		int64_t sourceDurationInMilliSeconds;
		string sourcePhysicalDeliveryURL;
		string sourceTranscoderStagingAssetPathName;
		bool stopIfReferenceProcessingError;
		tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> dependencyInfo =
			processDependencyInfo(workspace, ingestionJobKey, dependencies[0]);
		tie(sourceMediaItemKey, sourcePhysicalPathKey, referenceContentType, sourceAssetPathName, sourceRelativePath, sourceFileName,
			sourceFileExtension, sourceDurationInMilliSeconds, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName,
			stopIfReferenceProcessingError) = dependencyInfo;

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

				encodingProfileKey =
					_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, referenceContentType, encodingProfileLabel);
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

		// Since it was a copy and past, next commant has to be checked.
		// It is not possible to manage more than one encode because:
		// 1. inside _mmsEngineDBFacade->addEncodingJob, the ingestionJob is
		// updated to encodingQueue
		//		and the second call will fail (because the update of the
		// ingestion was already done
		//	2. The ingestionJob mantains the status of the encoding, how would
		// be managed 		the status in case of more than one encoding?
		// for
		// (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>&
		// 		keyAndDependencyType: dependencies)

		// 2023-04-23: qui dovremmo utilizzare il metodo
		// getEncodedFileExtensionByEncodingProfile
		//	che ritorna l'estensione in base all'encodingProfile.
		//	Non abbiamo ancora utilizzato questo metodo perchè si dovrebbe
		// verificare che funziona 	anche per estensioni diverse da
		// sourceFileExtension
		string encodedFileName = to_string(ingestionJobKey) + "_videoSpeed" + sourceFileExtension;

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

		// 2021-08-26: si dovrebbe cambiare l'implementazione:
		//	aggiungere la gestione di multi-video-speed: l'encoding dovrebbe
		// eseguire il processing in modo sequenziale e utilizzare il bool
		//	stopIfReferenceProcessingError per decidere se interrompere in caso
		// di errore
		_mmsEngineDBFacade->addEncoding_VideoSpeed(
			workspace, ingestionJobKey, sourceMediaItemKey, sourcePhysicalPathKey, sourceAssetPathName, sourceDurationInMilliSeconds,
			sourceFileExtension, sourcePhysicalDeliveryURL, sourceTranscoderStagingAssetPathName, encodingProfileKey, encodingProfileDetailsRoot,
			encodedTranscoderStagingAssetPathName, encodedNFSStagingAssetPathName.string(), _mmsWorkflowIngestionURL, _mmsBinaryIngestionURL,
			_mmsIngestionURL, encodingPriority
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageVideoSpeedTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageVideoSpeedTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
