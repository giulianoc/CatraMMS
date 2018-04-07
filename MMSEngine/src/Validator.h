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
    Validator(            
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade
    );
    
    Validator(const Validator& orig);
    virtual ~Validator();
    
    bool isVideoAudioMedia(string mediaSourceFileName);
    
    bool isImageMedia(string mediaSourceFileName);

    void validateRootMetadata(Json::Value root);

    void validateGroupOfTasksMetadata(Json::Value groupOfTasksRoot);

    vector<int64_t> validateTaskMetadata(Json::Value taskRoot);

    void validateEvents(Json::Value taskOrGroupOfTasksRoot);

    vector<int64_t> validateTaskMetadata(
        MMSEngineDBFacade::IngestionType ingestionType, 
        Json::Value parametersRoot);

    void validateContentIngestionMetadata(Json::Value parametersRoot);

    void validateFrameMetadata(Json::Value parametersRoot, vector<int64_t>& dependencies);

    void validatePeriodicalFramesMetadata(
        Json::Value parametersRoot, vector<int64_t>& dependencies);
    
    void validateIFramesMetadata(
        Json::Value parametersRoot, vector<int64_t>& dependencies);

    void validateConcatDemuxerMetadata(
        Json::Value parametersRoot, vector<int64_t>& dependencies);

    void validateCutMetadata(
        Json::Value parametersRoot, vector<int64_t>& dependencies);

private:
    shared_ptr<spdlog::logger>          _logger;
    shared_ptr<MMSEngineDBFacade>       _mmsEngineDBFacade;

    bool isMetadataPresent(Json::Value root, string field);

};

#endif /* VALIDATOR_H */

