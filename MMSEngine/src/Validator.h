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
    
    void validateProcessMetadata(Json::Value processRoot);

    pair<MMSEngineDBFacade::ContentType,vector<int64_t>>  
            validateTaskMetadata(Json::Value taskRoot);

    pair<MMSEngineDBFacade::ContentType,vector<int64_t>> 
        validateTaskMetadata(MMSEngineDBFacade::IngestionType ingestionType, 
        Json::Value parametersRoot);

    MMSEngineDBFacade::ContentType validateContentIngestionMetadata(
        Json::Value parametersRoot);

    MMSEngineDBFacade::ContentType validateFrameMetadata(
        Json::Value parametersRoot, vector<int64_t>& dependencies);

    MMSEngineDBFacade::ContentType validatePeriodicalFramesMetadata(
        Json::Value parametersRoot, vector<int64_t>& dependencies);
    
    MMSEngineDBFacade::ContentType validateIFramesMetadata(
        Json::Value parametersRoot, vector<int64_t>& dependencies);

    MMSEngineDBFacade::ContentType validateConcatDemuxerMetadata(
        Json::Value parametersRoot, vector<int64_t>& dependencies);

private:
    shared_ptr<spdlog::logger>          _logger;
    shared_ptr<MMSEngineDBFacade>       _mmsEngineDBFacade;

    bool isMetadataPresent(Json::Value root, string field);

};

#endif /* VALIDATOR_H */

