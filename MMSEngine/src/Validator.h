/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Validator.h
 * Author: giuliano
 *
 * Created on March 29, 2018, 6:27 AM
 */

#pragma once

#include "MMSEngineDBFacade.h"

class Validator final
{
  public:
	enum class DependencyType
	{
		MediaItemKey,
		PhysicalPathKey,
		IngestionJobKey
	};
	static const char *toString(const DependencyType &dependencyType)
	{
		switch (dependencyType)
		{
		case DependencyType::MediaItemKey:
			return "MediaItemKey";
		case DependencyType::PhysicalPathKey:
			return "PhysicalPathKey";
		case DependencyType::IngestionJobKey:
			return "IngestionJobKey";
		default:
		{
			const std::string errorMessage = fmt::format("toString with a wrong DependencyType: {}", static_cast<int>(dependencyType));
			SPDLOG_ERROR(errorMessage);
			throw std::runtime_error(errorMessage);
		}
		}
	}

  public:
	Validator(const std::shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade, const nlohmann::json& configuration);

	Validator(const Validator &orig);
	virtual ~Validator();

	static bool isVideoAudioFileFormat(const std::string &fileFormat);

	static bool isWorkflowAsLibraryTypeValid(const std::string& workflowAsLibraryType);

	static bool isImageFileFormat(const std::string &fileFormat);

	static bool isCutTypeValid(const std::string &cutType);

	static bool isAddSilentTypeValid(const std::string &addType);

	static bool isFacebookNodeTypeValid(const std::string &nodeType);

	static bool isFacebookLiveTypeValid(const std::string& nodeType);

	static bool isYouTubeLiveBroadcastSourceTypeValid(const std::string &sourceType);

	static bool isFacebookLiveBroadcastSourceTypeValid(const std::string &sourceType);

	static bool isYouTubePrivacyStatusValid(const std::string &privacyStatus);

	static bool isYouTubeTokenTypeValid(const std::string &tokenType);

	static bool isTimecodeValid(const std::string& timecode);
	static bool isFontTypeValid(const std::string &fontType);

	static bool isColorValid(const std::string& fontColor);

	static bool isVideoSpeedTypeValid(const std::string& speed);

	static bool isFaceRecognitionCascadeNameValid(const std::string &faceRecognitionCascadeName);

	static bool isFaceRecognitionOutputValid(const std::string &faceRecognitionOutput);

	static bool isLiveRecorderOutputValid(const std::string& liveRecorderOutput);

	static bool isLiveProxyOutputTypeValid(const std::string &liveProxyOutputType);

	static bool isLiveGridOutputTypeValid(const std::string& liveGridOutputType);

	// time_t sDateSecondsToUtc(std::string sDate);
	// int64_t sDateMilliSecondsToUtc(std::string sDate);

	void validateIngestedRootMetadata(int64_t workspaceKey, const nlohmann::json& root);

	void validateGroupOfTasksMetadata(int64_t workspaceKey, const nlohmann::json &groupOfTasksRoot, bool validateDependenciesToo);

	static void validateGroupOfTasksMetadata(int64_t workspaceKey, const nlohmann::json& parametersRoot);

	std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>
	validateSingleTaskMetadata(int64_t workspaceKey, const nlohmann::json &taskRoot, bool validateDependenciesToo);

	void validateEvents(int64_t workspaceKey, const nlohmann::json &taskOrGroupOfTasksRoot, bool validateDependenciesToo);

	std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>
	validateSingleTaskMetadata(int64_t workspaceKey, MMSEngineDBFacade::IngestionType ingestionType, const nlohmann::json& parametersRoot);

	void validateAddContentMetadata(const std::string &label, const nlohmann::json &parametersRoot);

	void validateAddSilentAudioMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateRemoveContentMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateEncodeMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFrameMetadata(
		int64_t workspaceKey, const std::string& label, const nlohmann::json& parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePeriodicalFramesMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateIFramesMetadata(
		int64_t workspaceKey, const std::string& label, const nlohmann::json& parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateSlideshowMetadata(
		int64_t workspaceKey, const std::string& label, const nlohmann::json& parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateConcatDemuxerMetadata(
		int64_t workspaceKey, const std::string& label, const nlohmann::json& parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateCutMetadata(
		int64_t workspaceKey, const std::string& label, const nlohmann::json& parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateOverlayImageOnVideoMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateOverlayTextOnVideoMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	) const;

	void validateEmailNotificationMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	static void validateCheckStreamingMetadata(int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot);

	void validateMediaCrossReferenceMetadata(
		int64_t workspaceKey, const std::string& label, const nlohmann::json& parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFTPDeliveryMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateHTTPCallbackMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLocalCopyMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateExtractTracksMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePostOnFacebookMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePostOnYouTubeMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFaceRecognitionMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	) const;

	void validateFaceIdentificationMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveRecorderMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateChangeFileFormatMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateVideoSpeedMetadata(
		int64_t workspaceKey, const std::string& label, const nlohmann::json& parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePictureInPictureMetadata(
		int64_t workspaceKey, const std::string& label, const nlohmann::json& parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateIntroOutroOverlayMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveProxyMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateYouTubeLiveBroadcastMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFacebookLiveBroadcastMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateVODProxyMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateCountdownMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveGridMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	static void validateLiveCutMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	static void validateEncodingProfilesSetRootMetadata(MMSEngineDBFacade::ContentType contentType, const nlohmann::json& encodingProfilesSetRoot);

	void validateEncodingProfileRootMetadata(MMSEngineDBFacade::ContentType contentType, const nlohmann::json& encodingProfileRoot);

	void validateWorkflowAsLibraryMetadata(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot, bool validateDependenciesToo,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void fillReferencesOutput(int64_t workspaceKey, const nlohmann::json &parametersRoot, std::vector<std::pair<int64_t, int64_t>> &referencesOutput) const;

  private:
	std::shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	std::string _storagePath;

	static void validateEncodingProfileRootVideoMetadata(const nlohmann::json &encodingProfileRoot);

	static void validateEncodingProfileRootAudioMetadata(const nlohmann::json &encodingProfileRoot);

	static void validateEncodingProfileRootImageMetadata(const nlohmann::json &encodingProfileRoot);

	void fillDependencies(
		int64_t workspaceKey, const std::string &label, const nlohmann::json &parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies,
		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey, bool encodingProfileFieldsToBeManaged
	) const;

	static void validateCrossReference(const std::string &label, const nlohmann::json &crossReferenceRoot, bool mediaItemKeyMandatory);

	static void validateOutputRootMetadata(int64_t workspaceKey, const std::string &label, const nlohmann::json &outputRoot, bool encodingMandatory);
};
