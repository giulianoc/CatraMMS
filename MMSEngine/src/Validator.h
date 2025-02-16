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

#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "MMSEngineDBFacade.h"

class Validator
{
  public:
	enum class DependencyType
	{
		MediaItemKey,
		PhysicalPathKey,
		IngestionJobKey
	};

  public:
	Validator(shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, json configuration);

	Validator(const Validator &orig);
	virtual ~Validator();

	bool isVideoAudioFileFormat(string fileFormat);

	bool isWorkflowAsLibraryTypeValid(string workflowAsLibraryType);

	bool isImageFileFormat(string fileFormat);

	bool isCutTypeValid(string cutType);

	bool isAddSilentTypeValid(string addType);

	bool isFacebookNodeTypeValid(string nodeType);

	bool isFacebookLiveTypeValid(string nodeType);

	bool isYouTubeLiveBroadcastSourceTypeValid(string sourceType);

	bool isFacebookLiveBroadcastSourceTypeValid(string sourceType);

	bool isYouTubePrivacyStatusValid(string privacyStatus);

	bool isYouTubeTokenTypeValid(string tokenType);

	bool isFontTypeValid(string fontType);

	bool isColorValid(string fontColor);

	bool isVideoSpeedTypeValid(string speed);

	bool isFaceRecognitionCascadeNameValid(string faceRecognitionCascadeName);

	bool isFaceRecognitionOutputValid(string faceRecognitionOutput);

	bool isLiveRecorderOutputValid(string liveRecorderOutput);

	bool isLiveProxyOutputTypeValid(string liveProxyOutputType);

	bool isLiveGridOutputTypeValid(string liveGridOutputType);

	// time_t sDateSecondsToUtc(string sDate);
	// int64_t sDateMilliSecondsToUtc(string sDate);

	void validateIngestedRootMetadata(int64_t workspaceKey, json root);

	void validateGroupOfTasksMetadata(int64_t workspaceKey, json groupOfTasksRoot, bool validateDependenciesToo);

	void validateGroupOfTasksMetadata(int64_t workspaceKey, json parametersRoot);

	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>
	validateSingleTaskMetadata(int64_t workspaceKey, json taskRoot, bool validateDependenciesToo);

	void validateEvents(int64_t workspaceKey, json taskOrGroupOfTasksRoot, bool validateDependenciesToo);

	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>
	validateSingleTaskMetadata(int64_t workspaceKey, MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot);

	void validateAddContentMetadata(string label, json parametersRoot);

	void validateAddSilentAudioMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateRemoveContentMetadata(
		int64_t workspaceKey, string label, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateEncodeMetadata(
		int64_t workspaceKey, string label, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFrameMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePeriodicalFramesMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateIFramesMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateSlideshowMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateConcatDemuxerMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateCutMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateOverlayImageOnVideoMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateOverlayTextOnVideoMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateEmailNotificationMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateCheckStreamingMetadata(int64_t workspaceKey, string label, json parametersRoot);

	void validateMediaCrossReferenceMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFTPDeliveryMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateHTTPCallbackMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLocalCopyMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateExtractTracksMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePostOnFacebookMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePostOnYouTubeMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFaceRecognitionMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFaceIdentificationMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveRecorderMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateChangeFileFormatMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateVideoSpeedMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validatePictureInPictureMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateIntroOutroOverlayMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveProxyMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateYouTubeLiveBroadcastMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateFacebookLiveBroadcastMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateVODProxyMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateCountdownMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveGridMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateLiveCutMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void validateEncodingProfilesSetRootMetadata(MMSEngineDBFacade::ContentType contentType, json encodingProfilesSetRoot);

	void validateEncodingProfileRootMetadata(MMSEngineDBFacade::ContentType contentType, json encodingProfileRoot);

	void validateWorkflowAsLibraryMetadata(
		int64_t workspaceKey, string label, json parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void fillReferencesOutput(int64_t workspaceKey, json parametersRoot, vector<pair<int64_t, int64_t>> &referencesOutput);

  private:
	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	string _storagePath;

	void validateEncodingProfileRootVideoMetadata(json encodingProfileRoot);

	void validateEncodingProfileRootAudioMetadata(json encodingProfileRoot);

	void validateEncodingProfileRootImageMetadata(json encodingProfileRoot);

	void fillDependencies(
		int64_t workspaceKey, string label, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies,
		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey, bool encodingProfileFieldsToBeManaged
	);

	void validateCrossReference(string label, json crossReferenceRoot, bool mediaItemKeyMandatory);

	void validateOutputRootMetadata(int64_t workspaceKey, string label, json outputRoot, bool encodingMandatory);
};

#endif /* VALIDATOR_H */
