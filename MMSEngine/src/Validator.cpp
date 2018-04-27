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

bool Validator::isVideoAudioFileFormat(string fileFormat)
{
    // see https://en.wikipedia.org/wiki/Video_file_format
    vector<string> suffixes = {
        "webm",
        "mkv",
        "flv",
        "vob",
        "ogv",
        "ogg",
        "avi",
        "mov",
        "wmv",
        "yuv",
        "mp4",
        "m4p ",
        "mpg",
        "mp2",
        "mpeg",
        "mjpeg",
        "m4v",
        "3gp",
        "3g2",
        "mxf"
    };

    string lowerCaseFileFormat;
    lowerCaseFileFormat.resize(fileFormat.size());
    transform(fileFormat.begin(), fileFormat.end(), lowerCaseFileFormat.begin(), [](unsigned char c){return tolower(c); } );
    for (string suffix: suffixes)
    {
        if (lowerCaseFileFormat == suffix) 
            return true;
    }
    
    return false;
}

bool Validator::isImageFileFormat(string fileFormat)
{
    // see https://en.wikipedia.org/wiki/Video_file_format
    vector<string> suffixes = {
        ".jpg",
        ".jpeg",
        ".tif",
        ".tiff",
        ".bmp",
        ".gif",
        ".png"
    };

    string lowerCaseFileFormat;
    lowerCaseFileFormat.resize(fileFormat.size());
    transform(fileFormat.begin(), fileFormat.end(), lowerCaseFileFormat.begin(), [](unsigned char c){return tolower(c); } );
    for (string suffix: suffixes)
    {
        if (lowerCaseFileFormat == suffix) 
            return true;
    }
    
    return false;
}

void Validator::validateEncodingProfilesSetRootMetadata(
    MMSEngineDBFacade::ContentType contentType,
    Json::Value encodingProfilesSetRoot)
{
    vector<string> mandatoryFields = {
        "Label"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(encodingProfilesSetRoot, mandatoryField))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "profiles";
    if (_mmsEngineDBFacade->isMetadataPresent(encodingProfilesSetRoot, field))
    {
        Json::Value profilesRoot = encodingProfilesSetRoot[field];

        for (int profileIndex = 0; profileIndex < profilesRoot.size(); profileIndex++)
        {
            Json::Value profileRoot = profilesRoot[profileIndex];

            if (contentType == MMSEngineDBFacade::ContentType::Video)
                validateEncodingProfileRootVideoMetadata(profileRoot);
            else if (contentType == MMSEngineDBFacade::ContentType::Audio)
                validateEncodingProfileRootAudioMetadata(profileRoot);
            else // if (contentType == MMSEngineDBFacade::ContentType::Image)
                validateEncodingProfileRootImageMetadata(profileRoot);
        }
    }
}

void Validator::validateEncodingProfileRootVideoMetadata(
    Json::Value encodingProfileRoot)
{
    {
        vector<string> mandatoryFields = {
            "Label",
            "fileFormat",
            "video",
            "audio"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileRoot, mandatoryField))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
    
    {
        string field = "video";
        Json::Value encodingProfileVideoRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "codec",
            "width",
            "height",
            "kBitrate",
            "twoPasses"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileVideoRoot, mandatoryField))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }

    {
        string field = "audio";
        Json::Value encodingProfileAudioRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "codec",
            "kBitrate"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileAudioRoot, mandatoryField))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
}

void Validator::validateEncodingProfileRootAudioMetadata(
    Json::Value encodingProfileRoot)
{
    {
        vector<string> mandatoryFields = {
            "Label",
            "fileFormat",
            "audio"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileRoot, mandatoryField))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
    
    {
        string field = "audio";
        Json::Value encodingProfileAudioRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "codec",
            "kBitrate"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileAudioRoot, mandatoryField))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
}

void Validator::validateEncodingProfileRootImageMetadata(
    Json::Value encodingProfileRoot)
{
    {
        vector<string> mandatoryFields = {
            "Label",
            "format",
            "width",
            "height",
            "aspectRatio",
            "interlaceType"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileRoot, mandatoryField))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }    
}

void Validator::validateRootMetadata(Json::Value root)
{    
    string field = "Type";
    if (!_mmsEngineDBFacade->isMetadataPresent(root, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }    
    string type = root.get(field, "XXX").asString();
    if (type != "Workflow")
    {
        string errorMessage = __FILEREF__ + "Type field is wrong"
                + ", Type: " + type;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    string taskField = "Task";
    string groupOfTasksField = "GroupOfTasks";
    if (_mmsEngineDBFacade->isMetadataPresent(root, taskField))
    {
        Json::Value taskRoot = root[taskField];  

        validateTaskMetadata(taskRoot);
    }
    else if (_mmsEngineDBFacade->isMetadataPresent(root, groupOfTasksField))
    {
        Json::Value groupOfTasksRoot = root[groupOfTasksField];  

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

vector<int64_t> Validator::validateTaskMetadata(Json::Value taskRoot)
{
    MMSEngineDBFacade::IngestionType    ingestionType;
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
        validateContentIngestionMetadata(parametersRoot);
    }
    else if (type == "Encode")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Encode;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateEncodeMetadata(parametersRoot, dependencies);
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
        validateFrameMetadata(parametersRoot, dependencies);        
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
        validatePeriodicalFramesMetadata(parametersRoot, dependencies);        
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
        validatePeriodicalFramesMetadata(parametersRoot, dependencies);        
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
        validateIFramesMetadata(parametersRoot, dependencies);        
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
        validateIFramesMetadata(parametersRoot, dependencies);        
    }
    else if (type == "Slideshow")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Slideshow;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateSlideshowMetadata(parametersRoot, dependencies);        
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
        validateConcatDemuxerMetadata(parametersRoot, dependencies);        
    }
    else if (type == "Cut")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Cut;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateCutMetadata(parametersRoot, dependencies);        
    }
    else if (type == "Email-Notification")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::EmailNotification;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateEmailNotificationMetadata(parametersRoot, dependencies);        
    }
    else
    {
        string errorMessage = __FILEREF__ + "Field 'Type' is wrong"
                + ", Type: " + type;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
        
    validateEvents(taskRoot);
    
    return dependencies;
}

vector<int64_t> Validator::validateTaskMetadata(
        MMSEngineDBFacade::IngestionType ingestionType, Json::Value parametersRoot)
{
    vector<int64_t>                     dependencies;

    if (ingestionType == MMSEngineDBFacade::IngestionType::ContentIngestion)
    {
        validateContentIngestionMetadata(parametersRoot);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Encode)
    {
        validateEncodeMetadata(parametersRoot, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
    {
        validateFrameMetadata(parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
            || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames)
    {
        validatePeriodicalFramesMetadata(parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::IFrames
            || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
    {
        validateIFramesMetadata(parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Slideshow)
    {
        validateSlideshowMetadata(parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::ConcatDemuxer)
    {
        validateConcatDemuxerMetadata(parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Cut)
    {
        validateCutMetadata(parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::EmailNotification)
    {
        validateEmailNotificationMetadata(parametersRoot, dependencies);        
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown IngestionType"
                + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    
    return dependencies;
}

void Validator::validateContentIngestionMetadata(
    Json::Value parametersRoot)
{
    vector<string> mandatoryFields = {
        // "SourceURL",     it is optional in case of push
        "FileFormat"
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
    string field = "FileFormat";
    string fileFormat = parametersRoot.get(field, "XXX").asString();

    if (!isVideoAudioFileFormat(fileFormat)
            && !isImageFileFormat(fileFormat))
    {
        string errorMessage = string("Unknown fileFormat")
            + ", fileFormat: " + fileFormat
        ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }

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
}

void Validator::validateEncodeMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    string field = "EncodingPriority";
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
        
    string encodingProfilesSetKeyField = "EncodingProfilesSetKey";
    string encodingProfilesSetLabelField = "EncodingProfilesSetLabel";
    string encodingProfileKeyField = "EncodingProfileKey";
    if (!isMetadataPresent(parametersRoot, encodingProfilesSetKeyField)
            && !isMetadataPresent(parametersRoot, encodingProfilesSetLabelField)
            && !isMetadataPresent(parametersRoot, encodingProfileKeyField))
    {
        string errorMessage = __FILEREF__ + "Neither of the following fields are present"
                + ", Field: " + encodingProfilesSetKeyField
                + ", Field: " + encodingProfilesSetLabelField
                + ", Field: " + encodingProfileKeyField
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    else if (isMetadataPresent(parametersRoot, encodingProfilesSetKeyField)
            && isMetadataPresent(parametersRoot, encodingProfileKeyField))
    {
        string errorMessage = __FILEREF__ + "Both fields are present"
                + ", Field: " + encodingProfilesSetKeyField
                + ", Field: " + encodingProfileKeyField
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    field = "References";
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
}

void Validator::validateFrameMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
     
    /*
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

    string field = "SourceFileName";
    string sourceFileName = parametersRoot.get(field, "XXX").asString();

    if (!isVideoAudioMedia(sourceFileName)
            && !isImageMedia(sourceFileName))
    {
        string errorMessage = string("Unknown sourceFileName extension")
            + ", sourceFileName: " + sourceFileName
        ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
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
}

void Validator::validatePeriodicalFramesMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    vector<string> mandatoryFields = {
        // "SourceFileName",
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

    /*
    string field = "SourceFileName";
    string sourceFileName = parametersRoot.get(field, "XXX").asString();

    if (!isVideoAudioMedia(sourceFileName)
            && !isImageMedia(sourceFileName))
    {
        string errorMessage = string("Unknown sourceFileName extension")
            + ", sourceFileName: " + sourceFileName
        ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
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
}

void Validator::validateIFramesMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
        
    /*
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

    string field = "SourceFileName";
    string sourceFileName = parametersRoot.get(field, "XXX").asString();

    if (!isVideoAudioMedia(sourceFileName)
            && !isImageMedia(sourceFileName))
    {
        string errorMessage = string("Unknown sourceFileName extension")
            + ", sourceFileName: " + sourceFileName
        ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
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
}

void Validator::validateSlideshowMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "References"
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

    /*
    string field = "SourceFileName";
    string sourceFileName = parametersRoot.get(field, "XXX").asString();

    if (!isVideoAudioMedia(sourceFileName))
    {
        string errorMessage = string("Unknown sourceFileName extension")
            + ", sourceFileName: " + sourceFileName
        ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
    string field = "References";
    {
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() < 1)
        {
            string errorMessage = __FILEREF__ + "Field is present but it does not have enough elements (1)"
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


            if (referenceContentType != MMSEngineDBFacade::ContentType::Image)
            {
                string errorMessage = __FILEREF__ + "Reference... does not refer a Image content"
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
}

void Validator::validateConcatDemuxerMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
        
    /*
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

    string field = "SourceFileName";
    string sourceFileName = parametersRoot.get(field, "XXX").asString();

    if (!isVideoAudioMedia(sourceFileName)
            && !isImageMedia(sourceFileName))
    {
        string errorMessage = string("Unknown sourceFileName extension")
            + ", sourceFileName: " + sourceFileName
        ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() < 2)
        {
            string errorMessage = __FILEREF__ + "Field is present but it does not have enough elements (2)"
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


            if (referenceContentType != MMSEngineDBFacade::ContentType::Video
                    && referenceContentType != MMSEngineDBFacade::ContentType::Audio)
            {
                string errorMessage = __FILEREF__ + "Reference... does not refer a video or audio content"
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
}

void Validator::validateCutMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
        
    /*
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
    
    string field = "SourceFileName";
    string sourceFileName = parametersRoot.get(field, "XXX").asString();

    if (!isVideoAudioMedia(sourceFileName)
            && !isImageMedia(sourceFileName))
    {
        string errorMessage = string("Unknown sourceFileName extension")
            + ", sourceFileName: " + sourceFileName
        ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
    string field = "StartTimeInSeconds";
    if (!isMetadataPresent(parametersRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string endTimeInSecondsField = "EndTimeInSeconds";
    string framesNumberField = "FramesNumber";
    if (!isMetadataPresent(parametersRoot, endTimeInSecondsField)
            && !isMetadataPresent(parametersRoot, framesNumberField))
    {
        string errorMessage = __FILEREF__ + "Both fields are not present or it is null"
                + ", Field: " + endTimeInSecondsField
                + ", Field: " + framesNumberField
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    field = "References";
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
}

void Validator::validateEmailNotificationMetadata(
    Json::Value parametersRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "EmailAddress",
        "Subject",
        "Message"
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
        if (referencesRoot.size() < 1)
        {
            string errorMessage = __FILEREF__ + "Field is present but it does not have enough elements (2)"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
        {
            Json::Value referenceRoot = referencesRoot[referenceIndex];

            int64_t referenceIngestionJobKey = -1;
            field = "ReferenceIngestionJobKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + "Reference...";
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            else
            {
                referenceIngestionJobKey = referenceRoot.get(field, "XXX").asInt64();
            }        

            dependencies.push_back(referenceIngestionJobKey);
        }
    }
        
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
}

bool Validator::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull())
        return true;
    else
        return false;
}
