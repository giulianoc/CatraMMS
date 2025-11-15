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

  public:
	Validator(const shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade, const json& configuration);

	Validator(const Validator &orig);
	virtual ~Validator();

	static bool isVideoAudioFileFormat(const string &fileFormat);

	static bool isWorkflowAsLibraryTypeValid(const string& workflowAsLibraryType);

	static bool isImageFileFormat(const string &fileFormat);

	static bool isCutTypeValid(const string &cutType);

	static bool isAddSilentTypeValid(const string &addType);

	static bool isFacebookNodeTypeValid(const string &nodeType);

	static bool isFacebookLiveTypeValid(const string& nodeType);

	static bool isYouTubeLiveBroadcastSourceTypeValid(const string &sourceType);

	static bool isFacebookLiveBroadcastSourceTypeValid(const string &sourceType);

	static bool isYouTubePrivacyStatusValid(const string &privacyStatus);

	static bool isYouTubeTokenTypeValid(const string &tokenType);

	static bool isTimecodeValid(const string& timecode);
	static bool isFontTypeValid(const string &fontType);

	static bool isColorValid(const string& fontColor);

	static bool isVideoSpeedTypeValid(const string& speed);

	static bool isFaceRecognitionCascadeNameValid(const string &faceRecognitionCascadeName);

	static bool isFaceRecognitionOutputValid(const string &faceRecognitionOutput);

	static bool isLiveRecorderOutputValid(const string& liveRecorderOutput);

	static bool isLiveProxyOutputTypeValid(const string &liveProxyOutputType);

	static bool isLiveGridOutputTypeValid(const string& liveGridOutputType);

	// time_t sDateSecondsToUtc(string sDate);
	// int64_t sDateMilliSecondsToUtc(string sDate);

	void validateIngestedRootMetadata(int64_t workspaceKey, const json& root);

	void validateGroupOfTasksMetadata(int64_t workspaceKey, const json &groupOfTasksRoot, bool validateDependenciesToo);

	static void validateGroupOfTasksMetadata(int64_t workspaceKey, const json& parametersRoot);

	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>
	validateSingleTaskMetadata(int64_t workspaceKey, const json &taskRoot, bool validateDependenciesToo);

	void validateEvents(int64_t workspaceKey, const json &taskOrGroupOfTasksRoot, bool validateDependenciesToo);

	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>
	validateSingleTaskMetadata(int64_t workspaceKey, MMSEngineDBFacade::IngestionType ingestionType, const json& parametersRoot);

	void validateAddContentMetadata(const string &label, const json &parametersRoot);

	void validateAddSilentAudioMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateRemoveContentMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateEncodeMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFrameMetadata(
		int64_t workspaceKey, const string& label, const json& parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePeriodicalFramesMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateIFramesMetadata(
		int64_t workspaceKey, const string& label, const json& parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateSlideshowMetadata(
		int64_t workspaceKey, const string& label, const json& parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateConcatDemuxerMetadata(
		int64_t workspaceKey, const string& label, const json& parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateCutMetadata(
		int64_t workspaceKey, const string& label, const json& parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateOverlayImageOnVideoMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateOverlayTextOnVideoMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	) const;

	void validateEmailNotificationMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	static void validateCheckStreamingMetadata(int64_t workspaceKey, const string &label, const json &parametersRoot);

	void validateMediaCrossReferenceMetadata(
		int64_t workspaceKey, const string& label, const json& parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFTPDeliveryMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateHTTPCallbackMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLocalCopyMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateExtractTracksMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePostOnFacebookMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePostOnYouTubeMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFaceRecognitionMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFaceIdentificationMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveRecorderMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateChangeFileFormatMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateVideoSpeedMetadata(
		int64_t workspaceKey, const string& label, const json& parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePictureInPictureMetadata(
		int64_t workspaceKey, const string& label, const json& parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateIntroOutroOverlayMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveProxyMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateYouTubeLiveBroadcastMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFacebookLiveBroadcastMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateVODProxyMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateCountdownMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveGridMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	static void validateLiveCutMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	static void validateEncodingProfilesSetRootMetadata(MMSEngineDBFacade::ContentType contentType, const json& encodingProfilesSetRoot);

	void validateEncodingProfileRootMetadata(MMSEngineDBFacade::ContentType contentType, const json& encodingProfileRoot);

	void validateWorkflowAsLibraryMetadata(
		int64_t workspaceKey, const string &label, const json &parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void fillReferencesOutput(int64_t workspaceKey, const json &parametersRoot, vector<pair<int64_t, int64_t>> &referencesOutput) const;

  private:
	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	string _storagePath;

	static void validateEncodingProfileRootVideoMetadata(const json &encodingProfileRoot);

	static void validateEncodingProfileRootAudioMetadata(const json &encodingProfileRoot);

	static void validateEncodingProfileRootImageMetadata(const json &encodingProfileRoot);

	void fillDependencies(
		int64_t workspaceKey, const string &label, const json &parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies,
		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey, bool encodingProfileFieldsToBeManaged
	) const;

	static void validateCrossReference(const string &label, const json &crossReferenceRoot, bool mediaItemKeyMandatory);

	static void validateOutputRootMetadata(int64_t workspaceKey, const string &label, const json &outputRoot, bool encodingMandatory);
};
