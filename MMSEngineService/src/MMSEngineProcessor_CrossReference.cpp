
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

void MMSEngineProcessor::manageMediaCrossReferenceTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 2)
		{
			string errorMessage = string() +
								  "No configured Two Media in order to create the Cross "
								  "Reference" +
								  ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", dependencies.size: " + to_string(dependencies.size());
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string field = "type";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			string errorMessage = string() + "Field is not present or it is null" + ", Field: " + field;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		MMSEngineDBFacade::CrossReferenceType crossReferenceType =
			MMSEngineDBFacade::toCrossReferenceType(JSONUtils::asString(parametersRoot, field, ""));
		if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::VideoOfImage)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfVideo;
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::AudioOfImage)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfAudio;
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::VideoOfPoster)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::PosterOfVideo;

		MMSEngineDBFacade::ContentType firstContentType;
		int64_t firstMediaItemKey;
		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];

			int64_t key;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, firstContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				firstMediaItemKey = key;

				// serve solo per verificare che il media item è del workspace
				// di cui si hanno ii diritti di accesso Se mediaitem non
				// appartiene al workspace avremo una eccezione
				// (MediaItemKeyNotFound)
				_mmsEngineDBFacade->getMediaItemKeyDetails(workspace->_workspaceKey, firstMediaItemKey, false, false);
			}
			else
			{
				int64_t physicalPathKey = key;

				bool warningIfMissing = false;
				tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, physicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

				tie(firstMediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
			}
		}

		MMSEngineDBFacade::ContentType secondContentType;
		int64_t secondMediaItemKey;
		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[1];

			int64_t key;
			Validator::DependencyType dependencyType;
			bool stopIfReferenceProcessingError;

			tie(key, secondContentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				secondMediaItemKey = key;

				// serve solo per verificare che il media item è del workspace
				// di cui si hanno ii diritti di accesso Se mediaitem non
				// appartiene al workspace avremo una eccezione
				// (MediaItemKeyNotFound)
				_mmsEngineDBFacade->getMediaItemKeyDetails(workspace->_workspaceKey, secondMediaItemKey, false, false);
			}
			else
			{
				int64_t physicalPathKey = key;

				bool warningIfMissing = false;
				tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspace->_workspaceKey, physicalPathKey, warningIfMissing,
							// 2022-12-18: MIK potrebbe essere stato appena
							// aggiunto
							true
						);

				tie(secondMediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
					mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
			}
		}

		if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::ImageOfVideo ||
			crossReferenceType == MMSEngineDBFacade::CrossReferenceType::FaceOfVideo ||
			crossReferenceType == MMSEngineDBFacade::CrossReferenceType::PosterOfVideo)
		{
			json crossReferenceParametersRoot;

			if (firstContentType == MMSEngineDBFacade::ContentType::Video && secondContentType == MMSEngineDBFacade::ContentType::Image)
			{
				SPDLOG_INFO(
					string() + "Add Cross Reference" + ", sourceMediaItemKey: " + to_string(secondMediaItemKey) + ", crossReferenceType: " +
					MMSEngineDBFacade::toString(crossReferenceType) + ", targetMediaItemKey: " + to_string(firstMediaItemKey)
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, secondMediaItemKey, crossReferenceType, firstMediaItemKey, crossReferenceParametersRoot
				);
			}
			else if (firstContentType == MMSEngineDBFacade::ContentType::Image && secondContentType == MMSEngineDBFacade::ContentType::Video)
			{
				SPDLOG_INFO(
					string() + "Add Cross Reference" + ", sourceMediaItemKey: " + to_string(firstMediaItemKey) + ", crossReferenceType: " +
					MMSEngineDBFacade::toString(crossReferenceType) + ", targetMediaItemKey: " + to_string(secondMediaItemKey)
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
				);
			}
			else
			{
				string errorMessage = string() + "Wrong content type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
									  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
									  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
									  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
									  ", firstMediaItemKey: " + to_string(firstMediaItemKey) +
									  ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::ImageOfAudio)
		{
			json crossReferenceParametersRoot;

			if (firstContentType == MMSEngineDBFacade::ContentType::Audio && secondContentType == MMSEngineDBFacade::ContentType::Image)
			{
				SPDLOG_INFO(
					string() + "Add Cross Reference" + ", sourceMediaItemKey: " + to_string(secondMediaItemKey) + ", crossReferenceType: " +
					MMSEngineDBFacade::toString(crossReferenceType) + ", targetMediaItemKey: " + to_string(firstMediaItemKey)
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, secondMediaItemKey, crossReferenceType, firstMediaItemKey, crossReferenceParametersRoot
				);
			}
			else if (firstContentType == MMSEngineDBFacade::ContentType::Image && secondContentType == MMSEngineDBFacade::ContentType::Audio)
			{
				SPDLOG_INFO(
					string() + "Add Cross Reference" + ", sourceMediaItemKey: " + to_string(firstMediaItemKey) + ", crossReferenceType: " +
					MMSEngineDBFacade::toString(crossReferenceType) + ", targetMediaItemKey: " + to_string(secondMediaItemKey)
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
				);
			}
			else
			{
				string errorMessage = string() + "Wrong content type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
									  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
									  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
									  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
									  ", firstMediaItemKey: " + to_string(firstMediaItemKey) +
									  ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::CutOfVideo)
		{
			if (firstContentType != MMSEngineDBFacade::ContentType::Video || secondContentType != MMSEngineDBFacade::ContentType::Video)
			{
				string errorMessage = string() + "Wrong content type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
									  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
									  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
									  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
									  ", firstMediaItemKey: " + to_string(firstMediaItemKey) +
									  ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "parameters";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage =
					string() + "Cross Reference Parameters are not present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
					", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
					", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
					", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
					", firstMediaItemKey: " + to_string(firstMediaItemKey) + ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json crossReferenceParametersRoot = parametersRoot[field];

			_mmsEngineDBFacade->addCrossReference(
				ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
			);
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::CutOfAudio)
		{
			if (firstContentType != MMSEngineDBFacade::ContentType::Audio || secondContentType != MMSEngineDBFacade::ContentType::Audio)
			{
				string errorMessage = string() + "Wrong content type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
									  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
									  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
									  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
									  ", firstMediaItemKey: " + to_string(firstMediaItemKey) +
									  ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "parameters";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage =
					string() + "Cross Reference Parameters are not present" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
					", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
					", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
					", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
					", firstMediaItemKey: " + to_string(firstMediaItemKey) + ", secondMediaItemKey: " + to_string(secondMediaItemKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json crossReferenceParametersRoot = parametersRoot[field];

			_mmsEngineDBFacade->addCrossReference(
				ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
			);
		}
		else
		{
			string errorMessage = string() + "Wrong type" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", dependencies.size: " + to_string(dependencies.size()) +
								  ", crossReferenceType: " + MMSEngineDBFacade::toString(crossReferenceType) +
								  ", firstContentType: " + MMSEngineDBFacade::toString(firstContentType) +
								  ", secondContentType: " + MMSEngineDBFacade::toString(secondContentType) +
								  ", firstMediaItemKey: " + to_string(firstMediaItemKey) + ", secondMediaItemKey: " + to_string(secondMediaItemKey);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		SPDLOG_INFO(
			string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " + "End_TaskSuccess" + ", errorMessage: " + ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (DeadlockFound &e)
	{
		SPDLOG_ERROR(
			string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", e.what(): " + e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "manageMediaCrossReferenceTask failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
