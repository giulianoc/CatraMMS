
#include "JSONUtils.h"
#include "MMSEngineProcessor.h"

using namespace std;
using json = nlohmann::json;

// this is to generate one Frame
void MMSEngineProcessor::manageFaceRecognitionMediaTask(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, const shared_ptr<Workspace>& workspace, const json& parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
) const
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = std::format("Wrong medias number to be processed for Face Recognition"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", dependencies.size: {}",
				_processorIdentifier, ingestionJobKey, dependencies.size());
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isPresent(parametersRoot, field))
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		else
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));

		string faceRecognitionCascadeName;
		string faceRecognitionOutput;
		long initialFramesNumberToBeSkipped;
		bool oneFramePerSecond;
		{
			field = "cascadeName";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = std::format("Field is not present or it is null"
					", _processorIdentifier: {}"
					", Field: {}", _processorIdentifier, field);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			faceRecognitionCascadeName = JSONUtils::asString(parametersRoot, field, "");

			field = "output";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = std::format("Field is not present or it is null"
					", _processorIdentifier: {}"
					", Field: {}", _processorIdentifier, field);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			faceRecognitionOutput = JSONUtils::asString(parametersRoot, field, "");

			initialFramesNumberToBeSkipped = 0;
			oneFramePerSecond = true;
			if (faceRecognitionOutput == "FrameContainingFace")
			{
				field = "initialFramesNumberToBeSkipped";
				initialFramesNumberToBeSkipped = JSONUtils::asInt32(parametersRoot, field, 0);

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

			auto [key, referenceContentType, dependencyType, stopIfReferenceProcessingError] = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;

				bool warningIfMissing = false;
				tie(sourcePhysicalPathKey, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) =
					_mmsStorage->getPhysicalPathDetails(
						key, encodingProfileKey, warningIfMissing,
						// 2022-12-18: MIK potrebbe essere stato appena
						// aggiunto
						true
					);

				sourceMediaItemKey = key;

				tie(contentType, ignore, ignore, ignore, ignore, ignore) = _mmsEngineDBFacade->getMediaItemKeyDetails(
					workspace->_workspaceKey, key, false,
					// 2022-12-18: MIK potrebbe essere stato appena
					// aggiunto
					true
				);
			}
			else
			{
				tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = _mmsStorage->getPhysicalPathDetails(
					key,
					// 2022-12-18: MIK potrebbe essere stato appena
					// aggiunto
					true
				);

				sourcePhysicalPathKey = key;

				tie(sourceMediaItemKey, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
					_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
						workspace->_workspaceKey, key, false,
						// 2022-12-18: MIK potrebbe essere stato
						// appena aggiunto
						true
					);
			}

			_mmsEngineDBFacade->addEncoding_FaceRecognitionJob(
				workspace, ingestionJobKey, sourceMediaItemKey, sourcePhysicalPathKey, mmsAssetPathName, faceRecognitionCascadeName,
				faceRecognitionOutput, encodingPriority, initialFramesNumberToBeSkipped, oneFramePerSecond
			);
		}
	}
	catch (exception &e)
	{
		LOG_ERROR("manageFaceRecognitionMediaTask failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw;
	}
}

void MMSEngineProcessor::manageFaceIdentificationMediaTask(
	int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, const shared_ptr<Workspace>& workspace,
	const json& parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
) const
{
	try
	{
		if (dependencies.size() != 1)
		{
			string errorMessage = std::format("Wrong medias number to be processed for Face Identification"
				", _processorIdentifier: {}"
				", ingestionJobKey: {}"
				", dependencies.size: {}", _processorIdentifier, ingestionJobKey, dependencies.size());
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		MMSEngineDBFacade::EncodingPriority encodingPriority;
		string field = "encodingPriority";
		if (!JSONUtils::isPresent(parametersRoot, field))
			encodingPriority = static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
		else
			encodingPriority = MMSEngineDBFacade::toEncodingPriority(JSONUtils::asString(parametersRoot, field, ""));

		string faceIdentificationCascadeName;
		string deepLearnedModelTagsCommaSeparated;
		{
			field = "cascadeName";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = std::format("Field is not present or it is null"
					", _processorIdentifier: {}"
					", Field: {}", _processorIdentifier, field);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			faceIdentificationCascadeName = JSONUtils::asString(parametersRoot, field, "");

			field = "deepLearnedModelTags";
			if (!JSONUtils::isPresent(parametersRoot, field))
			{
				string errorMessage = std::format("Field is not present or it is null"
					", _processorIdentifier: {}"
					", Field: {}", _processorIdentifier, field);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			deepLearnedModelTagsCommaSeparated = JSONUtils::asString(parametersRoot, field, "");
		}

		{
			tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> &keyAndDependencyType = dependencies[0];

			string mmsAssetPathName;
			MMSEngineDBFacade::ContentType contentType;
			string title;

			auto [key, referenceContentType, dependencyType, stopIfReferenceProcessingError] = keyAndDependencyType;

			if (dependencyType == Validator::DependencyType::MediaItemKey)
			{
				int64_t encodingProfileKey = -1;

				tie(ignore, mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = _mmsStorage->getPhysicalPathDetails(
					key, encodingProfileKey, false,
					// 2022-12-18: MIK potrebbe essere stato appena
					// aggiunto
					true
				);

				tie(contentType, ignore, ignore, ignore, ignore, ignore) = _mmsEngineDBFacade->getMediaItemKeyDetails(
					workspace->_workspaceKey, key, false,
					// 2022-12-18: MIK potrebbe essere stato appena
					// aggiunto
					true
				);
			}
			else
			{
				tie(mmsAssetPathName, ignore, ignore, ignore, ignore, ignore) = _mmsStorage->getPhysicalPathDetails(
					key,
					// 2022-12-18: MIK potrebbe essere stato appena
					// aggiunto
					true
				);
				tie(ignore, contentType, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
					_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
						workspace->_workspaceKey, key, false,
						// 2022-12-18: MIK potrebbe essere stato
						// appena aggiunto
						true
					);
			}

			_mmsEngineDBFacade->addEncoding_FaceIdentificationJob(
				workspace, ingestionJobKey, mmsAssetPathName, faceIdentificationCascadeName, deepLearnedModelTagsCommaSeparated, encodingPriority
			);
		}
	}
	catch (exception &e)
	{
		LOG_ERROR("manageFaceIdendificationMediaTask failed"
			", _processorIdentifier: {}"
			", ingestionJobKey: {}"
			", exception: {}", _processorIdentifier, ingestionJobKey, e.what()
		);

		// Update IngestionJob done in the calling method

		throw;
	}
}
