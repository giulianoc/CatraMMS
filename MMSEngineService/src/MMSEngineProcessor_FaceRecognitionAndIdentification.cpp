
#include "MMSEngineProcessor.h"
#include "JSONUtils.h"
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
#include "catralibraries/StringUtils.h"
#include "catralibraries/ProcessUtility.h"
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


// this is to generate one Frame
void MMSEngineProcessor::manageFaceRecognitionMediaTask(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong medias number to be processed for Face Recognition" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
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

		string faceRecognitionCascadeName;
		string faceRecognitionOutput;
		long initialFramesNumberToBeSkipped;
		bool oneFramePerSecond;
		{
			string field = "cascadeName";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			faceRecognitionCascadeName = JSONUtils::asString(parametersRoot, field, "");

			field = "output";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			faceRecognitionOutput = JSONUtils::asString(parametersRoot, field, "");

			initialFramesNumberToBeSkipped = 0;
			oneFramePerSecond = true;
			if (faceRecognitionOutput == "FrameContainingFace")
			{
				field = "initialFramesNumberToBeSkipped";
				initialFramesNumberToBeSkipped = JSONUtils::asInt(parametersRoot, field, 0);

				field = "oneFramePerSecond";
				oneFramePerSecond = JSONUtils::asBool(parametersRoot, field, true);
			}
		}

		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];

			string mmsAssetPathName;
			MMSEngineDBFacade::ContentType contentType;
			string title;
			int64_t sourceMediaItemKey;
			int64_t sourcePhysicalPathKey;

			int64_t key;
			MMSEngineDBFacade::ContentType referenceContentType;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;

				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(sourcePhysicalPathKey, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) =
					physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName;

				sourceMediaItemKey = key;

				{
					bool warningIfMissing = false;
					tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
						contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
							workspace->_workspaceKey, key, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

					tie(contentType, ignore, ignore, ignore, ignore, ignore) = contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
				}
			}
			else
			{
				tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;

				sourcePhysicalPathKey = key;

				{
					bool warningIfMissing = false;
					tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
							_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

					tie(sourceMediaItemKey, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
				}
			}

			_mmsEngineDBFacade->addEncoding_FaceRecognitionJob(
				workspace, ingestionJobKey, sourceMediaItemKey, sourcePhysicalPathKey, mmsAssetPathName, faceRecognitionCascadeName,
				faceRecognitionOutput, encodingPriority, initialFramesNumberToBeSkipped, oneFramePerSecond
			);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageFaceRecognitionMediaTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageFaceRecognitionMediaTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

void MMSEngineProcessor::manageFaceIdentificationMediaTask(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = string() + "Wrong medias number to be processed for Face Identification" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
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

		string faceIdentificationCascadeName;
		string deepLearnedModelTagsCommaSeparated;
		{
			string field = "cascadeName";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			faceIdentificationCascadeName = JSONUtils::asString(parametersRoot, field, "");

			field = "deepLearnedModelTags";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = string() + "Field is not present or it is null" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", Field: " + field;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			deepLearnedModelTagsCommaSeparated = JSONUtils::asString(parametersRoot, field, "");
		}

		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];

			string mmsAssetPathName;
			MMSEngineDBFacade::ContentType contentType;
			string title;

			int64_t key;
			MMSEngineDBFacade::ContentType referenceContentType;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;

				bool warningIfMissing = false;
				tuple<int64_t, string, int, string, string, int64_t, string> physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(ignore, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) =
					physicalPathKeyPhysicalPathFileNameSizeInBytesAndDeliveryFileName;

				{
					bool warningIfMissing = false;
					tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
						contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = _mmsEngineDBFacade->getMediaItemKeyDetails(
							workspace->_workspaceKey, key, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

					tie(contentType, ignore, ignore, ignore, ignore, ignore) = contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
				}
			}
			else
			{
				tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
					_mmsStorage->getPhysicalPathDetails(
						key,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);
				tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathFileNameSizeInBytesAndDeliveryFileName;
				{
					bool warningIfMissing = false;
					tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
							_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
								workspace->_workspaceKey, key, warningIfMissing,
								// 2022-12-18: MIK potrebbe essere stato
								// appena aggiunto
								true
							);

					tie(ignore, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
				}
			}

			_mmsEngineDBFacade->addEncoding_FaceIdentificationJob(
				workspace, ingestionJobKey, mmsAssetPathName, faceIdentificationCascadeName, deepLearnedModelTagsCommaSeparated, encodingPriority
			);
		}
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageFaceIdendificationMediaTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageFaceIdendificationMediaTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}

