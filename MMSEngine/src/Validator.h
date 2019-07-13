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

class Validator {
public:
    enum class DependencyType {
        MediaItemKey,
        PhysicalPathKey,
        IngestionJobKey
    };
    
public:
    Validator(            
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            Json::Value configuration
    );
    
    Validator(const Validator& orig);
    virtual ~Validator();
    
    bool isVideoAudioFileFormat(string fileFormat);
    
    bool isImageFileFormat(string fileFormat);

    bool isFontTypeValid(string fontType);
    
    bool isColorValid(string fontColor);

	bool isVideoSpeedTypeValid(string speed);

    bool isFaceRecognitionCascadeNameValid(string faceRecognitionCascadeName);

    bool isFaceRecognitionOutputValid(string faceRecognitionOutput);

    bool isLiveRecorderOutputValid(string liveRecorderOutput);

    void validateIngestedRootMetadata(int64_t workspaceKey, Json::Value root);

    void validateGroupOfTasksMetadata(int64_t workspaceKey, Json::Value groupOfTasksRoot, bool validateDependenciesToo);

    vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>> validateSingleTaskMetadata(
            int64_t workspaceKey, Json::Value taskRoot, bool validateDependenciesToo);

    void validateEvents(int64_t workspaceKey, Json::Value taskOrGroupOfTasksRoot, bool validateDependenciesToo);

    vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>> validateSingleTaskMetadata(int64_t workspaceKey,
        MMSEngineDBFacade::IngestionType ingestionType, Json::Value parametersRoot);

    void validateAddContentMetadata(string label, Json::Value parametersRoot);

    void validateRemoveContentMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);
    
    void validateEncodeMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateFrameMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validatePeriodicalFramesMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);
    
    void validateIFramesMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateSlideshowMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);
    
    void validateConcatDemuxerMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateCutMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateOverlayImageOnVideoMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateOverlayTextOnVideoMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateEmailNotificationMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

	void validateMediaCrossReferenceMetadata(int64_t workspaceKey, string label,
		Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateFTPDeliveryMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateHTTPCallbackMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateLocalCopyMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateExtractTracksMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validatePostOnFacebookMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validatePostOnYouTubeMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateFaceRecognitionMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);
    
    void validateFaceIdentificationMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);
    
    void validateLiveRecorderMetadata(int64_t workspaceKey, string label,
        Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);
    
	void validateChangeFileFormatMetadata(int64_t workspaceKey, string label,
		Json::Value parametersRoot, 
		bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

	void validateVideoSpeedMetadata(int64_t workspaceKey, string label,
		Json::Value parametersRoot, bool validateDependenciesToo,
		vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void validateEncodingProfilesSetRootMetadata(
        MMSEngineDBFacade::ContentType contentType, 
        Json::Value encodingProfilesSetRoot);
        
    void validateEncodingProfileRootMetadata(
        MMSEngineDBFacade::ContentType contentType,
        Json::Value encodingProfileRoot);

    static bool isMetadataPresent(Json::Value root, string field);
    
private:
    shared_ptr<spdlog::logger>          _logger;
    shared_ptr<MMSEngineDBFacade>       _mmsEngineDBFacade;
    string                              _storagePath;

    void validateEncodingProfileRootVideoMetadata(
        Json::Value encodingProfileRoot);
    
    void validateEncodingProfileRootAudioMetadata(
        Json::Value encodingProfileRoot);
    
    void validateEncodingProfileRootImageMetadata(
        Json::Value encodingProfileRoot);
    
    void fillDependencies(int64_t workspaceKey, string label,
        Json::Value parametersRoot,         
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies,
        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
        bool encodingProfileFieldsToBeManaged);

	void validateCrossReference(string label, Json::Value crossReferenceRoot, bool mediaItemKeyMandatory);
};

#endif /* VALIDATOR_H */

