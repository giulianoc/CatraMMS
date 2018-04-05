/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   Validator.cpp
 * Author: giuliano
 * 
 * Created on March 29, 2018, 6:27 AM
 */

#include "Validator.h"

Validator::Validator(
        shared_ptr<spdlog::logger> logger, 
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade
) 
{
    _logger             = logger;
    _mmsEngineDBFacade  = mmsEngineDBFacade;
}

Validator::Validator(const Validator& orig) {
}

Validator::~Validator() {
}

void Validator::validateProcessMetadata(Json::Value processRoot)
{
    // check Process metadata
    
    string taskField = "Task";
    string groupOfTasksField = "GroupOfTasks";
    if (_mmsEngineDBFacade->isMetadataPresent(processRoot, taskField))
    {
        Json::Value taskRoot = processRoot[taskField];  

        validateTaskMetadata(taskRoot);
    }
    else if (_mmsEngineDBFacade->isMetadataPresent(processRoot, groupOfTasksField))
    {
        Json::Value groupOfTasksRoot = processRoot[groupOfTasksField];  

        validateGroupOfTasksMetadata(groupOfTasksRoot);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Both Fields are not present or are null"
                + ", Field: " + taskField
                + ", Field: " + groupOfTasksField
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
}

void Validator::validateGroupOfTasksMetadata(Json::Value groupOfTasksRoot)
{
    string field = "ExecutionType";
    if (!_mmsEngineDBFacade->isMetadataPresent(groupOfTasksRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string executionType = groupOfTasksRoot.get(field, "XXX").asString();
    if (executionType != "parallel")
    {
        string errorMessage = __FILEREF__ + "executionType field is wrong"
                + ", executionType: " + executionType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    field = "Tasks";
    if (!_mmsEngineDBFacade->isMetadataPresent(groupOfTasksRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    Json::Value tasksRoot = groupOfTasksRoot[field];
    
    if (tasksRoot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No Tasks are present inside the GroupOfTasks item";
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    for (int taskIndex = 0; taskIndex < tasksRoot.size(); ++taskIndex)
    {
        Json::Value taskRoot = tasksRoot[taskIndex];
        
        validateTaskMetadata(taskRoot);
    }
    
    validateEvents(groupOfTasksRoot);
}

void Validator::validateEvents(Json::Value taskOrGroupOfTasksRoot)
{
    string field = "OnSuccess";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onSuccessRoot = taskOrGroupOfTasksRoot[field];
        
        string taskField = "Task";
        string groupOfTasksField = "GroupOfTasks";
        if (_mmsEngineDBFacade->isMetadataPresent(onSuccessRoot, taskField))
        {
            Json::Value onSuccessTaskRoot = onSuccessRoot[taskField];                        

            validateTaskMetadata(onSuccessTaskRoot);
        }
        else if (_mmsEngineDBFacade->isMetadataPresent(onSuccessRoot, groupOfTasksField))
        {
            Json::Value onSuccessGroupOfTasksRoot = onSuccessRoot[groupOfTasksField];                        

            validateGroupOfTasksMetadata(onSuccessGroupOfTasksRoot);
        }
    }

    field = "OnError";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onErrorRoot = taskOrGroupOfTasksRoot[field];
        
        string taskField = "Task";
        string groupOfTasksField = "GroupOfTasks";
        if (_mmsEngineDBFacade->isMetadataPresent(onErrorRoot, taskField))
        {
            Json::Value onErrorTaskRoot = onErrorRoot[taskField];                        

            validateTaskMetadata(onErrorTaskRoot);
        }
        else if (_mmsEngineDBFacade->isMetadataPresent(onErrorRoot, groupOfTasksField))
        {
            Json::Value onErrorGroupOfTasksRoot = onErrorRoot[groupOfTasksField];                        

            validateGroupOfTasksMetadata(onErrorGroupOfTasksRoot);
        }
    }    
    
    field = "OnComplete";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onCompleteRoot = taskOrGroupOfTasksRoot[field];
        
        string taskField = "Task";
        string groupOfTasksField = "GroupOfTasks";
        if (_mmsEngineDBFacade->isMetadataPresent(onCompleteRoot, taskField))
        {
            Json::Value onCompleteTaskRoot = onCompleteRoot[taskField];                        

            validateTaskMetadata(onCompleteTaskRoot);
        }
        else if (_mmsEngineDBFacade->isMetadataPresent(onCompleteRoot, groupOfTasksField))
        {
            Json::Value onCompleteGroupOfTasksRoot = onCompleteRoot[groupOfTasksField];                        

            validateGroupOfTasksMetadata(onCompleteGroupOfTasksRoot);
        }
    }    
}

pair<MMSEngineDBFacade::ContentType,vector<int64_t>> 
        Validator::validateTaskMetadata(Json::Value taskRoot)
{
    MMSEngineDBFacade::IngestionType    ingestionType;
    MMSEngineDBFacade::ContentType      contentType;
    vector<int64_t>                     dependencies;

    string field = "Type";
    if (!isMetadataPresent(taskRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string type = taskRoot.get("Type", "XXX").asString();
    if (type == "Content-Ingestion")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::ContentIngestion;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        contentType = validateContentIngestionMetadata(
                parametersRoot);
    }
    else if (type == "Frame")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Frame;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        contentType = validateFrameMetadata(parametersRoot, dependencies);        
    }
    else if (type == "Periodical-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::PeriodicalFrames;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        contentType = validatePeriodicalFramesMetadata(parametersRoot, dependencies);        
    }
    else if (type == "Motion-JPEG-by-Periodical-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        contentType = validatePeriodicalFramesMetadata(parametersRoot, dependencies);        
    }
    else if (type == "I-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::IFrames;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        contentType = validateIFramesMetadata(parametersRoot, dependencies);        
    }
    else if (type == "Motion-JPEG-by-I-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        contentType = validateIFramesMetadata(parametersRoot, dependencies);        
    }
    else if (type == "Concat-Demuxer")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::ConcatDemuxer;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        contentType = validateConcatDemuxerMetadata(parametersRoot, dependencies);        
    }
    else
    {
        string errorMessage = __FILEREF__ + "Field 'Type' is wrong"
                + ", Type: " + type;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
        
    validateEvents(taskRoot);
    
    return make_pair(contentType, dependencies);
}

pair<MMSEngineDBFacade::ContentType,vector<int64_t>> 
        Validator::validateTaskMetadata(
        MMSEngineDBFacade::IngestionType ingestionType, Json::Value parametersRoot)
{
    MMSEngineDBFacade::ContentType      contentType;
    vector<int64_t>                     dependencies;

    if (ingestionType == MMSEngineDBFacade::IngestionType::ContentIngestion)
    {

        contentType = validateContentIngestionMetadata(
                parametersRoot);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
    {
        MMSEngineDBFacade::ContentType contentType =
            validateFrameMetadata(parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
            || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames)
    {
        MMSEngineDBFacade::ContentType contentType =
            validatePeriodicalFramesMetadata(parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::IFrames
            || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
    {
        MMSEngineDBFacade::ContentType contentType =
            validateIFramesMetadata(parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::ConcatDemuxer)
    {
        MMSEngineDBFacade::ContentType contentType =
            validateConcatDemuxerMetadata(parametersRoot, dependencies);        
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown IngestionType"
                + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    
    return make_pair(contentType, dependencies);
}

MMSEngineDBFacade::ContentType Validator::validateContentIngestionMetadata(
    Json::Value parametersRoot)
{
    // see sample in directory samples
    
    MMSEngineDBFacade::ContentType         contentType;
    
    vector<string> mandatoryFields = {
        // "SourceURL",     it is optional in case of push
        "SourceFileName",
        "ContentType"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(parametersRoot, mandatoryField))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string sContentType = parametersRoot.get("ContentType", "XXX").asString();
    try
    {
        contentType = MMSEngineDBFacade::toContentType(sContentType);
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "Field 'ContentType' is wrong"
                + ", sContentType: " + sContentType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string field;
    if (contentType == MMSEngineDBFacade::ContentType::Video 
            || contentType == MMSEngineDBFacade::ContentType::Audio)
    {
        field = "EncodingPriority";
        if (isMetadataPresent(parametersRoot, field))
        {
            string encodingPriority = parametersRoot.get(field, "XXX").asString();
            try
            {
                MMSEngineDBFacade::toEncodingPriority(encodingPriority);    // it generate an exception in case of wrong string
            }
            catch(exception e)
            {
                string errorMessage = __FILEREF__ + "Field 'EncodingPriority' is wrong"
                        + ", EncodingPriority: " + encodingPriority;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }

    /*
    // Territories
    {
        field = "Territories";
        if (isMetadataPresent(parametersRoot, field))
        {
            const Json::Value territories = parametersRoot[field];
            
            for( Json::ValueIterator itr = territories.begin() ; itr != territories.end() ; itr++ ) 
            {
                Json::Value territory = territories[territoryIndex];
            }
        
    }
    */
            
    return contentType;
}

MMSEngineDBFacade::ContentType Validator::validateFrameMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "SourceFileName"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(parametersRoot, mandatoryField))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value referenceRoot = referencesRoot[0];

        int64_t referenceMediaItemKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        field = "ReferenceMediaItemKey";
        if (!isMetadataPresent(referenceRoot, field))
        {
            field = "ReferenceIngestionJobKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceUniqueName";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", Field: " + "Reference...";
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                else
                {
                    referenceUniqueName = referenceRoot.get(field, "XXX").asString();
                }        
            }
            else
            {
                referenceIngestionJobKey = referenceRoot.get(field, "XXX").asInt64();
            }        
        }
        else
        {
            referenceMediaItemKey = referenceRoot.get(field, "XXX").asInt64();    
        }

        MMSEngineDBFacade::ContentType      referenceContentType;
        try
        {
            bool warningIfMissing = true;
            if (referenceMediaItemKey != -1)
            {
                referenceContentType = _mmsEngineDBFacade->getMediaItemKeyDetails(
                    referenceMediaItemKey, warningIfMissing); 
            }
            else if (referenceIngestionJobKey != -1)
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByIngestionJobKey(
                        referenceIngestionJobKey, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else // if (referenceUniqueName != "")
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        referenceUniqueName, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
        }
        catch(runtime_error e)
        {
            string errorMessage = __FILEREF__ + "Reference... was not found"
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    ;
            _logger->warn(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getMediaItemKeyDetails failed"
                    + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
        {
            string errorMessage = __FILEREF__ + "Reference... does not refer a video content"
                + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                + ", referenceUniqueName: " + referenceUniqueName
                + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        dependencies.push_back(referenceMediaItemKey);
    }
    
    MMSEngineDBFacade::ContentType      frameContentType = MMSEngineDBFacade::ContentType::Image;

    /*
    // Territories
    {
        field = "Territories";
        if (isMetadataPresent(contentIngestion, field))
        {
            const Json::Value territories = contentIngestion[field];
            
            for( Json::ValueIterator itr = territories.begin() ; itr != territories.end() ; itr++ ) 
            {
                Json::Value territory = territories[territoryIndex];
            }
        
    }
    */
            
    return frameContentType;
}

MMSEngineDBFacade::ContentType Validator::validatePeriodicalFramesMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "SourceFileName",
        "PeriodInSeconds"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(parametersRoot, mandatoryField))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value referenceRoot = referencesRoot[0];

        int64_t referenceMediaItemKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        field = "ReferenceMediaItemKey";
        if (!isMetadataPresent(referenceRoot, field))
        {
            field = "ReferenceIngestionJobKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceUniqueName";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", Field: " + "Reference...";
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                else
                {
                    referenceUniqueName = referenceRoot.get(field, "XXX").asString();
                }        
            }
            else
            {
                referenceIngestionJobKey = referenceRoot.get(field, "XXX").asInt64();
            }        
        }
        else
        {
            referenceMediaItemKey = referenceRoot.get(field, "XXX").asInt64();    
        }
        
        MMSEngineDBFacade::ContentType      referenceContentType;
        try
        {
            bool warningIfMissing = true;
            if (referenceMediaItemKey != -1)
            {
                referenceContentType = _mmsEngineDBFacade->getMediaItemKeyDetails(
                    referenceMediaItemKey, warningIfMissing); 
            }
            else if (referenceIngestionJobKey != -1)
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByIngestionJobKey(
                        referenceIngestionJobKey, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else // if (referenceUniqueName != "")
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        referenceUniqueName, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
        }
        catch(runtime_error e)
        {
            string errorMessage = __FILEREF__ + "Reference... was not found"
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    ;
            _logger->warn(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getMediaItemKeyDetails failed"
                    + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
        {
            string errorMessage = __FILEREF__ + "Reference... does not refer a video content"
                + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                + ", referenceUniqueName: " + referenceUniqueName
                + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        dependencies.push_back(referenceMediaItemKey);
    }
        
    MMSEngineDBFacade::ContentType      frameContentType = MMSEngineDBFacade::ContentType::Image;

    /*
    // Territories
    {
        field = "Territories";
        if (isMetadataPresent(contentIngestion, field))
        {
            const Json::Value territories = contentIngestion[field];
            
            for( Json::ValueIterator itr = territories.begin() ; itr != territories.end() ; itr++ ) 
            {
                Json::Value territory = territories[territoryIndex];
            }
        
    }
    */
            
    return frameContentType;
}

MMSEngineDBFacade::ContentType Validator::validateIFramesMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "SourceFileName"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(parametersRoot, mandatoryField))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value referenceRoot = referencesRoot[0];

        int64_t referenceMediaItemKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        field = "ReferenceMediaItemKey";
        if (!isMetadataPresent(referenceRoot, field))
        {
            field = "ReferenceIngestionJobKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceUniqueName";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", Field: " + "Reference...";
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                else
                {
                    referenceUniqueName = referenceRoot.get(field, "XXX").asString();
                }        
            }
            else
            {
                referenceIngestionJobKey = referenceRoot.get(field, "XXX").asInt64();
            }        
        }
        else
        {
            referenceMediaItemKey = referenceRoot.get(field, "XXX").asInt64();    
        }

        MMSEngineDBFacade::ContentType      referenceContentType;
        try
        {
            bool warningIfMissing = true;
            if (referenceMediaItemKey != -1)
            {
                referenceContentType = _mmsEngineDBFacade->getMediaItemKeyDetails(
                    referenceMediaItemKey, warningIfMissing); 
            }
            else if (referenceIngestionJobKey != -1)
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByIngestionJobKey(
                        referenceIngestionJobKey, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else // if (referenceUniqueName != "")
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        referenceUniqueName, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
        }
        catch(MediaItemKeyNotFound e)
        {
            string errorMessage = __FILEREF__ + "Reference was not found"
                    + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    + ", referenceUniqueName: " + referenceUniqueName
                    ;
            _logger->warn(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(runtime_error e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getMediaItemKeyDetails failed"
                    + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    + ", referenceUniqueName: " + referenceUniqueName
                    ;
            _logger->warn(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getMediaItemKeyDetails failed"
                    + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    + ", referenceUniqueName: " + referenceUniqueName
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
        {
            string errorMessage = __FILEREF__ + "Reference... does not refer a video content"
                + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                + ", referenceUniqueName: " + referenceUniqueName
                + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        dependencies.push_back(referenceMediaItemKey);
    }
    
    MMSEngineDBFacade::ContentType      frameContentType = MMSEngineDBFacade::ContentType::Image;

    /*
    // Territories
    {
        field = "Territories";
        if (isMetadataPresent(contentIngestion, field))
        {
            const Json::Value territories = contentIngestion[field];
            
            for( Json::ValueIterator itr = territories.begin() ; itr != territories.end() ; itr++ ) 
            {
                Json::Value territory = territories[territoryIndex];
            }
        
    }
    */
            
    return frameContentType;
}

MMSEngineDBFacade::ContentType Validator::validateConcatDemuxerMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "SourceFileName"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(parametersRoot, mandatoryField))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() == 0)
        {
            string errorMessage = __FILEREF__ + "Field is present but it does not have any element"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
        {
            Json::Value referenceRoot = referencesRoot[referenceIndex];

            int64_t referenceMediaItemKey = -1;
            int64_t referenceIngestionJobKey = -1;
            string referenceUniqueName = "";
            string field = "ReferenceMediaItemKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceIngestionJobKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceUniqueName";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", Field: " + "Reference...";
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    else
                    {
                        referenceUniqueName = parametersRoot.get(field, "XXX").asString();
                    }        
                }
                else
                {
                    referenceIngestionJobKey = parametersRoot.get(field, "XXX").asInt64();
                }        
            }
            else
            {
                referenceMediaItemKey = parametersRoot.get(field, "XXX").asInt64();    
            }

            MMSEngineDBFacade::ContentType      referenceContentType;
            try
            {
                bool warningIfMissing = true;
                if (referenceMediaItemKey != -1)
                {
                    referenceContentType = _mmsEngineDBFacade->getMediaItemKeyDetails(
                        referenceMediaItemKey, warningIfMissing); 
                }
                else if (referenceIngestionJobKey != -1)
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByIngestionJobKey(
                            referenceIngestionJobKey, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
                else // if (referenceUniqueName != "")
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                            referenceUniqueName, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
            }
            catch(runtime_error e)
            {
                string errorMessage = __FILEREF__ + "Reference... was not found"
                        + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                        ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);
            }
            catch(exception e)
            {
                string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getMediaItemKeyDetails failed"
                        + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                        + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }


            if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
            {
                string errorMessage = __FILEREF__ + "Reference... does not refer a video content"
                    + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    + ", referenceUniqueName: " + referenceUniqueName
                    + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            dependencies.push_back(referenceMediaItemKey);
        }
    }

    MMSEngineDBFacade::ContentType      contentType = MMSEngineDBFacade::ContentType::Video;
    
    /*
    // Territories
    {
        field = "Territories";
        if (isMetadataPresent(contentIngestion, field))
        {
            const Json::Value territories = contentIngestion[field];
            
            for( Json::ValueIterator itr = territories.begin() ; itr != territories.end() ; itr++ ) 
            {
                Json::Value territory = territories[territoryIndex];
            }
        
    }
    */
            
    return contentType;
}

bool Validator::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull())
        return true;
    else
        return false;
}
