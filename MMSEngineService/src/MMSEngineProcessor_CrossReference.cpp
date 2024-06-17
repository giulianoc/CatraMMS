
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/fmt/fmt.h"

void MMSEngineProcessor::manageMediaCrossReferenceTask(
	int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
)
{
	try
	{
		if (dependencies.size() != 2)
		{
			string errorMessage = fmt::format(
				"No configured Two Media in order to create the Cross Reference"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", dependencies.size: {}",
				_processorIdentifier, ingestionJobKey, dependencies.size()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string field = "type";
		MMSEngineDBFacade::CrossReferenceType crossReferenceType =
			MMSEngineDBFacade::toCrossReferenceType(JSONUtils::asString(parametersRoot, field, "", true));
		if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::VideoOfImage)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfVideo;
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::AudioOfImage)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfAudio;
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::ImageForSlideShow)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::SlideShowOfImage;
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::AudioForSlideShow)
			crossReferenceType = MMSEngineDBFacade::CrossReferenceType::SlideShowOfAudio;
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
			// first image, second video

			json crossReferenceParametersRoot;

			if (firstContentType == MMSEngineDBFacade::ContentType::Video && secondContentType == MMSEngineDBFacade::ContentType::Image)
			{
				SPDLOG_INFO(
					"Add Cross Reference"
					", sourceMediaItemKey: {}"
					", crossReferenceType: {}"
					", targetMediaItemKey: {}",
					secondMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), firstMediaItemKey
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, secondMediaItemKey, crossReferenceType, firstMediaItemKey, crossReferenceParametersRoot
				);
			}
			else if (firstContentType == MMSEngineDBFacade::ContentType::Image && secondContentType == MMSEngineDBFacade::ContentType::Video)
			{
				SPDLOG_INFO(
					"Add Cross Reference"
					", sourceMediaItemKey: {}"
					", crossReferenceType: {}"
					", targetMediaItemKey: {}",
					firstMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), secondMediaItemKey
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
				);
			}
			else
			{
				string errorMessage = fmt::format(
					"Wrong content type"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size: {}"
					", crossReferenceType: {}"
					", firstContentType: {}"
					", secondContentType: {}"
					", firstMediaItemKey: {}"
					", secondMediaItemKey: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size(), MMSEngineDBFacade::toString(crossReferenceType),
					MMSEngineDBFacade::toString(firstContentType), MMSEngineDBFacade::toString(secondContentType), firstMediaItemKey,
					secondMediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::ImageOfAudio)
		{
			// first image, second audio

			json crossReferenceParametersRoot;

			if (firstContentType == MMSEngineDBFacade::ContentType::Audio && secondContentType == MMSEngineDBFacade::ContentType::Image)
			{
				SPDLOG_INFO(
					"Add Cross Reference"
					", sourceMediaItemKey: {}"
					", crossReferenceType: {}"
					", targetMediaItemKey: {}",
					secondMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), firstMediaItemKey
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, secondMediaItemKey, crossReferenceType, firstMediaItemKey, crossReferenceParametersRoot
				);
			}
			else if (firstContentType == MMSEngineDBFacade::ContentType::Image && secondContentType == MMSEngineDBFacade::ContentType::Audio)
			{
				SPDLOG_INFO(
					"Add Cross Reference"
					", sourceMediaItemKey: {}"
					", crossReferenceType: {}"
					", targetMediaItemKey: {}",
					firstMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), secondMediaItemKey
				);
				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
				);
			}
			else
			{
				string errorMessage = fmt::format(
					"Wrong content type"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size: {}"
					", crossReferenceType: {}"
					", firstContentType: {}"
					", secondContentType: {}"
					", firstMediaItemKey: {}"
					", secondMediaItemKey: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size(), MMSEngineDBFacade::toString(crossReferenceType),
					MMSEngineDBFacade::toString(firstContentType), MMSEngineDBFacade::toString(secondContentType), firstMediaItemKey,
					secondMediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::SlideShowOfImage)
		{
			// first video, second image

			json crossReferenceParametersRoot;

			if (firstContentType == MMSEngineDBFacade::ContentType::Video && secondContentType == MMSEngineDBFacade::ContentType::Image)
			{
				SPDLOG_INFO(
					"Add Cross Reference"
					", sourceMediaItemKey: {}"
					", crossReferenceType: {}"
					", targetMediaItemKey: {}",
					firstMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), secondMediaItemKey
				);

				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
				);
			}
			else if (firstContentType == MMSEngineDBFacade::ContentType::Image && secondContentType == MMSEngineDBFacade::ContentType::Video)
			{
				SPDLOG_INFO(
					"Add Cross Reference"
					", sourceMediaItemKey: {}"
					", crossReferenceType: {}"
					", targetMediaItemKey: {}",
					secondMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), firstMediaItemKey
				);

				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, secondMediaItemKey, crossReferenceType, firstMediaItemKey, crossReferenceParametersRoot
				);
			}
			else
			{
				string errorMessage = fmt::format(
					"Wrong content type"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size: {}"
					", crossReferenceType: {}"
					", firstContentType: {}"
					", secondContentType: {}"
					", firstMediaItemKey: {}"
					", secondMediaItemKey: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size(), MMSEngineDBFacade::toString(crossReferenceType),
					MMSEngineDBFacade::toString(firstContentType), MMSEngineDBFacade::toString(secondContentType), firstMediaItemKey,
					secondMediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::SlideShowOfAudio)
		{
			// first video, second audio

			json crossReferenceParametersRoot;

			if (firstContentType == MMSEngineDBFacade::ContentType::Video && secondContentType == MMSEngineDBFacade::ContentType::Audio)
			{
				SPDLOG_INFO(
					"Add Cross Reference"
					", sourceMediaItemKey: {}"
					", crossReferenceType: {}"
					", targetMediaItemKey: {}",
					firstMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), secondMediaItemKey
				);

				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
				);
			}
			else if (firstContentType == MMSEngineDBFacade::ContentType::Audio && secondContentType == MMSEngineDBFacade::ContentType::Video)
			{
				SPDLOG_INFO(
					"Add Cross Reference"
					", sourceMediaItemKey: {}"
					", crossReferenceType: {}"
					", targetMediaItemKey: {}",
					secondMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), firstMediaItemKey
				);

				_mmsEngineDBFacade->addCrossReference(
					ingestionJobKey, secondMediaItemKey, crossReferenceType, firstMediaItemKey, crossReferenceParametersRoot
				);
			}
			else
			{
				string errorMessage = fmt::format(
					"Wrong content type"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size: {}"
					", crossReferenceType: {}"
					", firstContentType: {}"
					", secondContentType: {}"
					", firstMediaItemKey: {}"
					", secondMediaItemKey: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size(), MMSEngineDBFacade::toString(crossReferenceType),
					MMSEngineDBFacade::toString(firstContentType), MMSEngineDBFacade::toString(secondContentType), firstMediaItemKey,
					secondMediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::CutOfVideo)
		{
			// first video, second video

			if (firstContentType != MMSEngineDBFacade::ContentType::Video || secondContentType != MMSEngineDBFacade::ContentType::Video)
			{
				string errorMessage = fmt::format(
					"Wrong content type"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size: {}"
					", crossReferenceType: {}"
					", firstContentType: {}"
					", secondContentType: {}"
					", firstMediaItemKey: {}"
					", secondMediaItemKey: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size(), MMSEngineDBFacade::toString(crossReferenceType),
					MMSEngineDBFacade::toString(firstContentType), MMSEngineDBFacade::toString(secondContentType), firstMediaItemKey,
					secondMediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "parameters";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = fmt::format(
					"Cross Reference Parameters are not present"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size: {}"
					", crossReferenceType: {}"
					", firstContentType: {}"
					", secondContentType: {}"
					", firstMediaItemKey: {}"
					", secondMediaItemKey: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size(), MMSEngineDBFacade::toString(crossReferenceType),
					MMSEngineDBFacade::toString(firstContentType), MMSEngineDBFacade::toString(secondContentType), firstMediaItemKey,
					secondMediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json crossReferenceParametersRoot = parametersRoot[field];

			SPDLOG_INFO(
				"Add Cross Reference"
				", sourceMediaItemKey: {}"
				", crossReferenceType: {}"
				", targetMediaItemKey: {}",
				firstMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), secondMediaItemKey
			);

			_mmsEngineDBFacade->addCrossReference(
				ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
			);
		}
		else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::CutOfAudio)
		{
			// first audio, second audio

			if (firstContentType != MMSEngineDBFacade::ContentType::Audio || secondContentType != MMSEngineDBFacade::ContentType::Audio)
			{
				string errorMessage = fmt::format(
					"Wrong content type"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size: {}"
					", crossReferenceType: {}"
					", firstContentType: {}"
					", secondContentType: {}"
					", firstMediaItemKey: {}"
					", secondMediaItemKey: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size(), MMSEngineDBFacade::toString(crossReferenceType),
					MMSEngineDBFacade::toString(firstContentType), MMSEngineDBFacade::toString(secondContentType), firstMediaItemKey,
					secondMediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "parameters";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string errorMessage = fmt::format(
					"Cross Reference Parameters are not present"
					", _processorIdentifier: {}"
					", ingestionJobKey: {}"
					", dependencies.size: {}"
					", crossReferenceType: {}"
					", firstContentType: {}"
					", secondContentType: {}"
					", firstMediaItemKey: {}"
					", secondMediaItemKey: {}",
					_processorIdentifier, ingestionJobKey, dependencies.size(), MMSEngineDBFacade::toString(crossReferenceType),
					MMSEngineDBFacade::toString(firstContentType), MMSEngineDBFacade::toString(secondContentType), firstMediaItemKey,
					secondMediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json crossReferenceParametersRoot = parametersRoot[field];

			SPDLOG_INFO(
				"Add Cross Reference"
				", sourceMediaItemKey: {}"
				", crossReferenceType: {}"
				", targetMediaItemKey: {}",
				firstMediaItemKey, MMSEngineDBFacade::toString(crossReferenceType), secondMediaItemKey
			);

			_mmsEngineDBFacade->addCrossReference(
				ingestionJobKey, firstMediaItemKey, crossReferenceType, secondMediaItemKey, crossReferenceParametersRoot
			);
		}
		else
		{
			string errorMessage = fmt::format(
				"Wrong content type"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", dependencies.size: {}"
				", crossReferenceType: {}"
				", firstContentType: {}"
				", secondContentType: {}"
				", firstMediaItemKey: {}"
				", secondMediaItemKey: {}",
				_processorIdentifier, ingestionJobKey, dependencies.size(), MMSEngineDBFacade::toString(crossReferenceType),
				MMSEngineDBFacade::toString(firstContentType), MMSEngineDBFacade::toString(secondContentType), firstMediaItemKey, secondMediaItemKey
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		SPDLOG_INFO(
			"Update IngestionJob"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", IngestionStatus: {}"
			", errorMessage: {}",
			_processorIdentifier, ingestionJobKey, "End_TaskSuccess", ""
		);
		_mmsEngineDBFacade->updateIngestionJob(
			ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_TaskSuccess,
			"" // errorMessage
		);
	}
	catch (DeadlockFound &e)
	{
		SPDLOG_ERROR(
			"manageMediaCrossReferenceTask failed"
			", _processorIdentifier: "
			", ingestionJobKey: "
			", e.what(): ",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"manageMediaCrossReferenceTask failed"
			", _processorIdentifier: "
			", ingestionJobKey: "
			", e.what(): ",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"manageMediaCrossReferenceTask failed"
			", _processorIdentifier: "
			", ingestionJobKey: "
			", e.what(): ",
			_processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw e;
	}
}
