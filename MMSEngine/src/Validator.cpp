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
        "jpg",
        "jpeg",
        "tif",
        "tiff",
        "bmp",
        "gif",
        "png"
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

bool Validator::isFontTypeValid(string fontType)
{
    vector<string> validFontTypes = {
        "..."
    };

    for (string validFontType: validFontTypes)
    {
        if (fontType == validFontType) 
            return true;
    }
    
    return false;
}

bool Validator::isFontColorValid(string fontColor)
{
    vector<string> validFontColors = {
        "black",
        "blue",
        "gray",
        "green",
        "orange",
        "purple",
        "red",
        "violet",
        "white",
        "yellow"
    };

    for (string validFontColor: validFontColors)
    {
        if (fontColor == validFontColor) 
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
            Json::StreamWriterBuilder wbuilder;
            string sEncodingProfilesSetRoot = Json::writeString(wbuilder, encodingProfilesSetRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sEncodingProfilesSetRoot: " + sEncodingProfilesSetRoot
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "Profiles";
    if (_mmsEngineDBFacade->isMetadataPresent(encodingProfilesSetRoot, field))
    {
        Json::Value profilesRoot = encodingProfilesSetRoot[field];

        for (int profileIndex = 0; profileIndex < profilesRoot.size(); profileIndex++)
        {
            Json::Value encodingProfileRoot = profilesRoot[profileIndex];

            validateEncodingProfileRootMetadata(contentType, encodingProfileRoot);
        }
    }
}

void Validator::validateEncodingProfileRootMetadata(
    MMSEngineDBFacade::ContentType contentType,
    Json::Value encodingProfileRoot)
{
    if (contentType == MMSEngineDBFacade::ContentType::Video)
        validateEncodingProfileRootVideoMetadata(encodingProfileRoot);
    else if (contentType == MMSEngineDBFacade::ContentType::Audio)
        validateEncodingProfileRootAudioMetadata(encodingProfileRoot);
    else // if (contentType == MMSEngineDBFacade::ContentType::Image)
        validateEncodingProfileRootImageMetadata(encodingProfileRoot);
}

void Validator::validateEncodingProfileRootVideoMetadata(
    Json::Value encodingProfileRoot)
{
    {
        vector<string> mandatoryFields = {
            "Label",
            "FileFormat",
            "Video",
            "Audio"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileRoot, mandatoryField))
            {
                Json::StreamWriterBuilder wbuilder;
                string sEncodingProfileRoot = Json::writeString(wbuilder, encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
    
    {
        string field = "Video";
        Json::Value encodingProfileVideoRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "Codec",
            "Width",
            "Height",
            "KBitRate",
            "TwoPasses"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileVideoRoot, mandatoryField))
            {
                Json::StreamWriterBuilder wbuilder;
                string sEncodingProfileRoot = Json::writeString(wbuilder, encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }

    {
        string field = "Audio";
        Json::Value encodingProfileAudioRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "Codec",
            "KBitRate"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileAudioRoot, mandatoryField))
            {
                Json::StreamWriterBuilder wbuilder;
                string sEncodingProfileRoot = Json::writeString(wbuilder, encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
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
            "FileFormat",
            "Audio"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileRoot, mandatoryField))
            {
                Json::StreamWriterBuilder wbuilder;
                string sEncodingProfileRoot = Json::writeString(wbuilder, encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
    
    {
        string field = "Audio";
        Json::Value encodingProfileAudioRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "Codec",
            "KBitRate"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileAudioRoot, mandatoryField))
            {
                Json::StreamWriterBuilder wbuilder;
                string sEncodingProfileRoot = Json::writeString(wbuilder, encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
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
            "Format",
            "Width",
            "Height",
            "AspectRatio",
            "InterlaceType"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileRoot, mandatoryField))
            {
                Json::StreamWriterBuilder wbuilder;
                string sEncodingProfileRoot = Json::writeString(wbuilder, encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }    
}

void Validator::validateRootMetadata(int64_t workspaceKey, Json::Value root)
{    
    string field = "Type";
    if (!_mmsEngineDBFacade->isMetadataPresent(root, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sRoot = Json::writeString(wbuilder, root);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + 
                + ", sRoot: " + sRoot
                ;
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
    
    field = "Task";
    if (!_mmsEngineDBFacade->isMetadataPresent(root, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sRoot = Json::writeString(wbuilder, root);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sRoot: " + sRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }    
    Json::Value taskRoot = root[field];                        

    field = "Type";
    if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sRoot = Json::writeString(wbuilder, root);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sRoot: " + sRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }    
    string taskType = taskRoot.get(field, "XXX").asString();

    if (taskType == "GroupOfTasks")
    {
        validateGroupOfTasksMetadata(workspaceKey, taskRoot);
    }
    else
    {
        validateSingleTaskMetadata(workspaceKey, taskRoot);
    }    
}

void Validator::validateGroupOfTasksMetadata(int64_t workspaceKey, Json::Value groupOfTasksRoot)
{
    string field = "Parameters";
    if (!isMetadataPresent(groupOfTasksRoot, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sGroupOfTasksRoot = Json::writeString(wbuilder, groupOfTasksRoot);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sGroupOfTasksRoot: " + sGroupOfTasksRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    Json::Value parametersRoot = groupOfTasksRoot[field];
    
    field = "ExecutionType";
    if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sGroupOfTasksRoot = Json::writeString(wbuilder, groupOfTasksRoot);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sGroupOfTasksRoot: " + sGroupOfTasksRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string executionType = parametersRoot.get(field, "XXX").asString();
    if (executionType != "parallel" 
            && executionType != "sequential")
    {
        string errorMessage = __FILEREF__ + "executionType field is wrong"
                + ", executionType: " + executionType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    field = "Tasks";
    if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sGroupOfTasksRoot = Json::writeString(wbuilder, groupOfTasksRoot);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sGroupOfTasksRoot: " + sGroupOfTasksRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    Json::Value tasksRoot = parametersRoot[field];
    
    if (tasksRoot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No Tasks are present inside the GroupOfTasks item";
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    for (int taskIndex = 0; taskIndex < tasksRoot.size(); ++taskIndex)
    {
        Json::Value taskRoot = tasksRoot[taskIndex];
        
        field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sGroupOfTasksRoot = Json::writeString(wbuilder, groupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                + ", sGroupOfTasksRoot: " + sGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            validateGroupOfTasksMetadata(workspaceKey, taskRoot);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot);
        }        
    }
    
    validateEvents(workspaceKey, groupOfTasksRoot);
}

void Validator::validateEvents(int64_t workspaceKey, Json::Value taskOrGroupOfTasksRoot)
{
    string field = "OnSuccess";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onSuccessRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!_mmsEngineDBFacade->isMetadataPresent(onSuccessRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskOrGroupOfTasksRoot = Json::writeString(wbuilder, taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value taskRoot = onSuccessRoot[field];                        

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskOrGroupOfTasksRoot = Json::writeString(wbuilder, taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            validateGroupOfTasksMetadata(workspaceKey, taskRoot);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot);
        }
    }

    field = "OnError";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onErrorRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!_mmsEngineDBFacade->isMetadataPresent(onErrorRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskOrGroupOfTasksRoot = Json::writeString(wbuilder, taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value taskRoot = onErrorRoot[field];                        

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskOrGroupOfTasksRoot = Json::writeString(wbuilder, taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            validateGroupOfTasksMetadata(workspaceKey, taskRoot);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot);
        }
    }    
    
    field = "OnComplete";
    if (_mmsEngineDBFacade->isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onCompleteRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!_mmsEngineDBFacade->isMetadataPresent(onCompleteRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskOrGroupOfTasksRoot = Json::writeString(wbuilder, taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        Json::Value taskRoot = onCompleteRoot[field];                        

        string field = "Type";
        if (!_mmsEngineDBFacade->isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskOrGroupOfTasksRoot = Json::writeString(wbuilder, taskOrGroupOfTasksRoot);

            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "XXX").asString();

        if (taskType == "GroupOfTasks")
        {
            validateGroupOfTasksMetadata(workspaceKey, taskRoot);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot);
        }
    }    
}

vector<pair<int64_t,Validator::DependencyType>> Validator::validateSingleTaskMetadata(
        int64_t workspaceKey, Json::Value taskRoot)
{
    MMSEngineDBFacade::IngestionType    ingestionType;
    vector<pair<int64_t,DependencyType>>           dependencies;

    string field = "Type";
    if (!isMetadataPresent(taskRoot, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sTaskRoot: " + sTaskRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string type = taskRoot.get("Type", "XXX").asString();
    if (type == "Add-Content")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::AddContent;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateAddContentMetadata(parametersRoot);
    }
    else if (type == "Remove-Content")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::RemoveContent;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateRemoveContentMetadata(workspaceKey, parametersRoot, dependencies);
    }
    else if (type == "Encode")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Encode;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateEncodeMetadata(workspaceKey, parametersRoot, dependencies);
    }
    else if (type == "Frame")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Frame;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateFrameMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "Periodical-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::PeriodicalFrames;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validatePeriodicalFramesMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "Motion-JPEG-by-Periodical-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validatePeriodicalFramesMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "I-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::IFrames;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateIFramesMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "Motion-JPEG-by-I-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateIFramesMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "Slideshow")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Slideshow;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateSlideshowMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "Concat-Demuxer")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::ConcatDemuxer;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateConcatDemuxerMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "Cut")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Cut;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateCutMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "Overlay-Image-On-Video")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::OverlayImageOnVideo;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateOverlayImageOnVideoMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "Overlay-Text-On-Video")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::OverlayTextOnVideo;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value parametersRoot = taskRoot[field]; 
        validateOverlayTextOnVideoMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (type == "Email-Notification")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::EmailNotification;
        
        field = "Parameters";
        if (!isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskRoot = Json::writeString(wbuilder, taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
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
        
    validateEvents(workspaceKey, taskRoot);
    
    return dependencies;
}

vector<pair<int64_t,Validator::DependencyType>> Validator::validateSingleTaskMetadata(int64_t workspaceKey,
        MMSEngineDBFacade::IngestionType ingestionType, Json::Value parametersRoot)
{
    vector<pair<int64_t,DependencyType>>                     dependencies;

    if (ingestionType == MMSEngineDBFacade::IngestionType::AddContent)
    {
        validateAddContentMetadata(parametersRoot);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::RemoveContent)
    {
        validateRemoveContentMetadata(workspaceKey, parametersRoot, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Encode)
    {
        validateEncodeMetadata(workspaceKey, parametersRoot, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
    {
        validateFrameMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
            || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames)
    {
        validatePeriodicalFramesMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::IFrames
            || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
    {
        validateIFramesMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Slideshow)
    {
        validateSlideshowMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::ConcatDemuxer)
    {
        validateConcatDemuxerMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Cut)
    {
        validateCutMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayImageOnVideo)
    {
        validateOverlayImageOnVideoMetadata(workspaceKey, parametersRoot, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayTextOnVideo)
    {
        validateOverlayTextOnVideoMetadata(workspaceKey, parametersRoot, dependencies);        
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

void Validator::validateAddContentMetadata(
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
            Json::StreamWriterBuilder wbuilder;
            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    ;
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

void Validator::validateRemoveContentMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t,DependencyType>>& dependencies)
{     
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() < 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
        {
            Json::Value referenceRoot = referencesRoot[referenceIndex];

            int64_t referenceMediaItemKey = -1;
            int64_t referencePhysicalPathKey = -1;
            int64_t referenceIngestionJobKey = -1;
            string referenceUniqueName = "";
            bool referenceLabel = false;
            
            field = "ReferenceMediaItemKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferencePhysicalPathKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceIngestionJobKey";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceUniqueName";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            field = "ReferenceLabel";
                            if (!isMetadataPresent(referenceRoot, field))
                            {
                                Json::StreamWriterBuilder wbuilder;
                                string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                        + ", Field: " + "Reference..."
                                        + ", sParametersRoot: " + sParametersRoot
                                        ;
                                _logger->error(errorMessage);

                                throw runtime_error(errorMessage);
                            }
                            else
                            {
                                referenceLabel = true;
                            }
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
                    referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
                else if (referencePhysicalPathKey != -1)
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                            referencePhysicalPathKey, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
                else if (referenceIngestionJobKey != -1)
                {
                    tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                            referenceIngestionJobKey, warningIfMissing);  

                    tie(referenceMediaItemKey, referencePhysicalPathKey, referenceContentType) =
                            mediaItemKeyPhysicalPathKeyAndContentType;
                }
                else if (referenceUniqueName != "")
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                            workspaceKey, referenceUniqueName, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
                else // referenceLabel
                {
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

            if (referenceLabel == false && referencePhysicalPathKey == -1)
            {
                int64_t encodingProfileKey = -1;
                
                field = "EncodingProfileKey";
                if (isMetadataPresent(referenceRoot, field))
                {
                    int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                            referenceMediaItemKey, encodingProfileKey);
                }  
                else
                {
                    field = "EncodingProfileLabel";
                    if (isMetadataPresent(referenceRoot, field))
                    {
                        string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                        referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey,
                                referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                    }        
                }
            }
            
            if (referencePhysicalPathKey != -1)
                dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
            else if (referenceMediaItemKey != -1)
                dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
            else    // referenceLabel
                ;
        }
    }    
}

void Validator::validateEncodeMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t,DependencyType>>& dependencies)
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
    string encodingProfileLabelField = "EncodingProfileLabel";
    if (!isMetadataPresent(parametersRoot, encodingProfilesSetKeyField)
            && !isMetadataPresent(parametersRoot, encodingProfilesSetLabelField)
            && !isMetadataPresent(parametersRoot, encodingProfileLabelField)
            && !isMetadataPresent(parametersRoot, encodingProfileKeyField))
    {
        string errorMessage = __FILEREF__ + "Neither of the following fields are present"
                + ", Field: " + encodingProfilesSetKeyField
                + ", Field: " + encodingProfilesSetLabelField
                + ", Field: " + encodingProfileLabelField
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
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        bool referenceLabel = false;
        
        field = "ReferenceMediaItemKey";
        if (!isMetadataPresent(referenceRoot, field))
        {
            field = "ReferencePhysicalPathKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceIngestionJobKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceUniqueName";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceLabel";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            Json::StreamWriterBuilder wbuilder;
                            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                    + ", Field: " + "Reference..."
                                    + ", sParametersRoot: " + sParametersRoot
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                        else
                        {
                            referenceLabel = true;
                        }
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
                referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
            else if (referencePhysicalPathKey != -1)
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        referencePhysicalPathKey, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else if (referenceIngestionJobKey != -1)
            {
                int64_t localReferencePhysicalPathKey;
                
                tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                        referenceIngestionJobKey, warningIfMissing);  

                tie(referenceMediaItemKey, localReferencePhysicalPathKey, referenceContentType) =
                        mediaItemKeyPhysicalPathKeyAndContentType;
            }
            else if (referenceUniqueName != "")
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        workspaceKey, referenceUniqueName, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else // referenceLabel
            {
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
        
        if (referenceLabel == false && referencePhysicalPathKey == -1)
        {
            int64_t encodingProfileKey = -1;

            field = "EncodingProfileKey";
            if (isMetadataPresent(referenceRoot, field))
            {
                int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                        referenceMediaItemKey, encodingProfileKey);
            }  
            else
            {
                field = "EncodingProfileLabel";
                if (isMetadataPresent(referenceRoot, field))
                {
                    string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey, 
                            referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                }        
            }
        }

        if (referencePhysicalPathKey != -1)
            dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
        else if (referenceMediaItemKey != -1)
            dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
        else    // referenceLabel
            ;
    }    
}

void Validator::validateFrameMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t,DependencyType>>& dependencies)
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
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        bool referenceLabel = false;
        
        field = "ReferenceMediaItemKey";
        if (!isMetadataPresent(referenceRoot, field))
        {
            field = "ReferencePhysicalPathKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceIngestionJobKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceUniqueName";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceLabel";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            Json::StreamWriterBuilder wbuilder;
                            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                    + ", Field: " + "Reference..."
                                    + ", sParametersRoot: " + sParametersRoot
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                        else
                        {
                            referenceLabel = true;
                        }
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
                referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
            else if (referencePhysicalPathKey != -1)
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        referencePhysicalPathKey, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else if (referenceIngestionJobKey != -1)
            {
                int64_t localReferencePhysicalPathKey;
                
                tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                        referenceIngestionJobKey, warningIfMissing);  

                tie(referenceMediaItemKey, localReferencePhysicalPathKey, referenceContentType) =
                        mediaItemKeyPhysicalPathKeyAndContentType;
            }
            else if (referenceUniqueName != "")
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        workspaceKey, referenceUniqueName, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else // referenceLabel
            {
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
        
        if (referenceLabel == false 
                && referenceContentType != MMSEngineDBFacade::ContentType::Video)
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

        if (referenceLabel == false && referencePhysicalPathKey == -1)
        {
            int64_t encodingProfileKey = -1;

            field = "EncodingProfileKey";
            if (isMetadataPresent(referenceRoot, field))
            {
                int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                        referenceMediaItemKey, encodingProfileKey);
            }  
            else
            {
                field = "EncodingProfileLabel";
                if (isMetadataPresent(referenceRoot, field))
                {
                    string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey,
                            referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                }        
            }
        }

        if (referencePhysicalPathKey != -1)
            dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
        else if (referenceMediaItemKey != -1)
            dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
        else    // referenceLabel
            ;
    }    
}

void Validator::validatePeriodicalFramesMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t,DependencyType>>& dependencies)
{
    vector<string> mandatoryFields = {
        // "SourceFileName",
        "PeriodInSeconds"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(parametersRoot, mandatoryField))
        {
            Json::StreamWriterBuilder wbuilder;
            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    ;
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
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        bool referenceLabel = false;
        
        field = "ReferenceMediaItemKey";
        if (!isMetadataPresent(referenceRoot, field))
        {
            field = "ReferencePhysicalPathKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceIngestionJobKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceUniqueName";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceLabel";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            Json::StreamWriterBuilder wbuilder;
                            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                    + ", Field: " + "Reference..."
                                    + ", sParametersRoot: " + sParametersRoot
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                        else
                        {
                            referenceLabel = true;
                        }
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
                referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
            else if (referencePhysicalPathKey != -1)
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        referencePhysicalPathKey, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else if (referenceIngestionJobKey != -1)
            {
                int64_t localReferencePhysicalPathKey;
                
                tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                        referenceIngestionJobKey, warningIfMissing);  

                tie(referenceMediaItemKey, localReferencePhysicalPathKey, referenceContentType) =
                        mediaItemKeyPhysicalPathKeyAndContentType;
            }
            else if (referenceUniqueName != "")
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        workspaceKey, referenceUniqueName, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else // referenceLabel
            {
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

        if (referenceLabel == false 
                && referenceContentType != MMSEngineDBFacade::ContentType::Video)
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

        if (referenceLabel == false && referencePhysicalPathKey == -1)
        {
            int64_t encodingProfileKey = -1;

            field = "EncodingProfileKey";
            if (isMetadataPresent(referenceRoot, field))
            {
                int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                        referenceMediaItemKey, encodingProfileKey);
            }  
            else
            {
                field = "EncodingProfileLabel";
                if (isMetadataPresent(referenceRoot, field))
                {
                    string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey,
                            referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                }        
            }
        }

        if (referencePhysicalPathKey != -1)
            dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
        else if (referenceMediaItemKey != -1)
            dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
        else    // referenceLabel
            ;
    }        
}

void Validator::validateIFramesMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t,DependencyType>>& dependencies)
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
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        bool referenceLabel = false;
        
        field = "ReferenceMediaItemKey";
        if (!isMetadataPresent(referenceRoot, field))
        {
            field = "ReferencePhysicalPathKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceIngestionJobKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceUniqueName";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceLabel";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            Json::StreamWriterBuilder wbuilder;
                            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                    + ", Field: " + "Reference..."
                                    + ", sParametersRoot: " + sParametersRoot
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                        else
                        {
                            referenceLabel = true;
                        }
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
                referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
            else if (referencePhysicalPathKey != -1)
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        referencePhysicalPathKey, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else if (referenceIngestionJobKey != -1)
            {
                int64_t referencePhysicalPathKey;
                
                tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                        referenceIngestionJobKey, warningIfMissing);  

                tie(referenceMediaItemKey, referencePhysicalPathKey, referenceContentType) =
                        mediaItemKeyPhysicalPathKeyAndContentType;
            }
            else if (referenceUniqueName != "")
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        workspaceKey, referenceUniqueName, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else // referenceLabel
            {
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

        if (referenceLabel == false 
                && referenceContentType != MMSEngineDBFacade::ContentType::Video)
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

        if (referenceLabel == false && referencePhysicalPathKey == -1)
        {
            int64_t encodingProfileKey = -1;

            field = "EncodingProfileKey";
            if (isMetadataPresent(referenceRoot, field))
            {
                int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                        referenceMediaItemKey, encodingProfileKey);
            }  
            else
            {
                field = "EncodingProfileLabel";
                if (isMetadataPresent(referenceRoot, field))
                {
                    string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey,
                            referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                }        
            }
        }

        if (referencePhysicalPathKey != -1)
            dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
        else if (referenceMediaItemKey != -1)
            dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
        else    // referenceLabel
            ;
    }    
}

void Validator::validateSlideshowMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t,DependencyType>>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "References"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(parametersRoot, mandatoryField))
        {
            Json::StreamWriterBuilder wbuilder;
            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
                    
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    ;
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
            int64_t referencePhysicalPathKey = -1;
            int64_t referenceIngestionJobKey = -1;
            string referenceUniqueName = "";
            bool referenceLabel = false;
            
            string field = "ReferenceMediaItemKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferencePhysicalPathKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceIngestionJobKey";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceUniqueName";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            field = "ReferenceLabel";
                            if (!isMetadataPresent(referenceRoot, field))
                            {
                                Json::StreamWriterBuilder wbuilder;
                                string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                        + ", Field: " + "Reference..."
                                        + ", sParametersRoot: " + sParametersRoot
                                        ;
                                _logger->error(errorMessage);

                                throw runtime_error(errorMessage);
                            }
                            else
                            {
                                referenceLabel = true;
                            }
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
                    referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
                else if (referencePhysicalPathKey != -1)
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                            referencePhysicalPathKey, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
                else if (referenceIngestionJobKey != -1)
                {
                    int64_t localReferencePhysicalPathKey;

                    tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                            referenceIngestionJobKey, warningIfMissing);  

                    tie(referenceMediaItemKey, localReferencePhysicalPathKey, referenceContentType) =
                            mediaItemKeyPhysicalPathKeyAndContentType;
                }
                else if (referenceUniqueName != "")
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                            workspaceKey, referenceUniqueName, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
                else // referenceLabel
                {
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

            if (referenceLabel == false 
                    && referenceContentType != MMSEngineDBFacade::ContentType::Image)
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

            if (referenceLabel == false && referencePhysicalPathKey == -1)
            {
                int64_t encodingProfileKey = -1;
                
                field = "EncodingProfileKey";
                if (isMetadataPresent(referenceRoot, field))
                {
                    int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                            referenceMediaItemKey, encodingProfileKey);
                }  
                else
                {
                    field = "EncodingProfileLabel";
                    if (isMetadataPresent(referenceRoot, field))
                    {
                        string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                        referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey,
                                referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                    }        
                }
            }
            
            if (referencePhysicalPathKey != -1)
                dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
            else if (referenceMediaItemKey != -1)
                dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
            else    // referenceLabel
                ;
        }
    }
}

void Validator::validateConcatDemuxerMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t,DependencyType>>& dependencies)
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
            int64_t referencePhysicalPathKey = -1;
            int64_t referenceIngestionJobKey = -1;
            string referenceUniqueName = "";
            bool referenceLabel = false;
            
            string field = "ReferenceMediaItemKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferencePhysicalPathKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceIngestionJobKey";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceUniqueName";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            field = "ReferenceLabel";
                            if (!isMetadataPresent(referenceRoot, field))
                            {
                                Json::StreamWriterBuilder wbuilder;
                                string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                        + ", Field: " + "Reference..."
                                        + ", sParametersRoot: " + sParametersRoot
                                        ;
                                _logger->error(errorMessage);

                                throw runtime_error(errorMessage);
                            }
                            else
                            {
                                referenceLabel = true;
                            }
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
                    referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
                else if (referencePhysicalPathKey != -1)
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                            referencePhysicalPathKey, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
                else if (referenceIngestionJobKey != -1)
                {
                    int64_t localReferencePhysicalPathKey;

                    tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                            referenceIngestionJobKey, warningIfMissing);  

                    tie(referenceMediaItemKey, localReferencePhysicalPathKey, referenceContentType) =
                            mediaItemKeyPhysicalPathKeyAndContentType;
                }
                else if (referenceUniqueName != "")
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                            workspaceKey, referenceUniqueName, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
                else // referenceLabel
                {
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

            if (referenceLabel == false 
                    && referenceContentType != MMSEngineDBFacade::ContentType::Video
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

            if (referenceLabel == false && referencePhysicalPathKey == -1)
            {
                int64_t encodingProfileKey = -1;
                
                field = "EncodingProfileKey";
                if (isMetadataPresent(referenceRoot, field))
                {
                    int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                            referenceMediaItemKey, encodingProfileKey);
                }  
                else
                {
                    field = "EncodingProfileLabel";
                    if (isMetadataPresent(referenceRoot, field))
                    {
                        string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                        referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey,
                                referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                    }        
                }
            }
            
            if (referencePhysicalPathKey != -1)
                dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
            else if (referenceMediaItemKey != -1)
                dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
            else    // referenceLabel
                ;
        }
    }
}

void Validator::validateCutMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t, DependencyType>>& dependencies)
{
    // see sample in directory samples
        
    string field = "StartTimeInSeconds";
    if (!isMetadataPresent(parametersRoot, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
                        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sParametersRoot: " + sParametersRoot
                ;
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
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        bool referenceLabel = false;
        
        field = "ReferenceMediaItemKey";
        if (!isMetadataPresent(referenceRoot, field))
        {
            field = "ReferencePhysicalPathKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceIngestionJobKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceUniqueName";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceLabel";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            Json::StreamWriterBuilder wbuilder;
                            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                    + ", Field: " + "Reference..."
                                    + ", sParametersRoot: " + sParametersRoot
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                        else
                        {
                            referenceLabel = true;
                        }
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
                referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
            else if (referencePhysicalPathKey != -1)
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        referencePhysicalPathKey, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else if (referenceIngestionJobKey != -1)
            {
                int64_t localReferencePhysicalPathKey;

                tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                        referenceIngestionJobKey, warningIfMissing);  

                tie(referenceMediaItemKey, localReferencePhysicalPathKey, referenceContentType) =
                        mediaItemKeyPhysicalPathKeyAndContentType;
            }
            else if (referenceUniqueName != "")
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        workspaceKey, referenceUniqueName, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else // referenceLabel
            {
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

        if (referenceLabel == false 
                && referenceContentType != MMSEngineDBFacade::ContentType::Video)
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

        if (referenceLabel == false && referencePhysicalPathKey == -1)
        {
            int64_t encodingProfileKey = -1;

            field = "EncodingProfileKey";
            if (isMetadataPresent(referenceRoot, field))
            {
                int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                        referenceMediaItemKey, encodingProfileKey);
            }  
            else
            {
                field = "EncodingProfileLabel";
                if (isMetadataPresent(referenceRoot, field))
                {
                    string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey,
                            referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                }        
            }
        }

        if (referencePhysicalPathKey != -1)
            dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
        else if (referenceMediaItemKey != -1)
            dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
        else    // referenceLabel
            ;
    }        
}

void Validator::validateOverlayImageOnVideoMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t,DependencyType>>& dependencies)
{
    
    vector<string> mandatoryFields = {
        "imagePosition_X_InPixel",
        "imagePosition_Y_InPixel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(parametersRoot, mandatoryField))
        {
            Json::StreamWriterBuilder wbuilder;
            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
                    
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    ;
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
        if (referencesRoot.size() != 2)
        {
            string errorMessage = __FILEREF__ + "Field is present but it does not have two elements"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool imagePresent = false;
        bool videoPresent = false;
        for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
        {
            Json::Value referenceRoot = referencesRoot[referenceIndex];

            int64_t referenceMediaItemKey = -1;
            int64_t referencePhysicalPathKey = -1;
            int64_t referenceIngestionJobKey = -1;
            string referenceUniqueName = "";
            bool referenceLabel = false;
            
            string field = "ReferenceMediaItemKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferencePhysicalPathKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceIngestionJobKey";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceUniqueName";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            field = "ReferenceLabel";
                            if (!isMetadataPresent(referenceRoot, field))
                            {
                                Json::StreamWriterBuilder wbuilder;
                                string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                        + ", Field: " + "Reference..."
                                        + ", sParametersRoot: " + sParametersRoot
                                        ;
                                _logger->error(errorMessage);

                                throw runtime_error(errorMessage);
                            }
                            else
                            {
                                referenceLabel = true;
                            }
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
                    referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
                else if (referencePhysicalPathKey != -1)
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                            referencePhysicalPathKey, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
                else if (referenceIngestionJobKey != -1)
                {
                    int64_t localReferencePhysicalPathKey;

                    tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                            referenceIngestionJobKey, warningIfMissing);  

                    tie(referenceMediaItemKey, localReferencePhysicalPathKey, referenceContentType) =
                            mediaItemKeyPhysicalPathKeyAndContentType;
                }
                else if (referenceUniqueName != "")
                {
                    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                            _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                            workspaceKey, referenceUniqueName, warningIfMissing);  

                    referenceMediaItemKey = mediaItemKeyAndContentType.first;
                    referenceContentType = mediaItemKeyAndContentType.second;
                }
                else // referenceLabel
                {
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

            if (referenceLabel == false)
            {
                if (referenceContentType == MMSEngineDBFacade::ContentType::Video)
                {
                    if (videoPresent)
                    {
                        string errorMessage = __FILEREF__ + "References are referring two videos"
                                ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }

                    videoPresent = true;
                }
                else if (referenceContentType == MMSEngineDBFacade::ContentType::Image)
                {
                    if (imagePresent)
                    {
                        string errorMessage = __FILEREF__ + "References are referring two images"
                                ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }

                    imagePresent = true;
                }
                else
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer a video or image content"
                        + ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
                        + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                        + ", referenceUniqueName: " + referenceUniqueName
                        + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }

            if (referenceLabel == false && referencePhysicalPathKey == -1)
            {
                int64_t encodingProfileKey = -1;
                
                field = "EncodingProfileKey";
                if (isMetadataPresent(referenceRoot, field))
                {
                    int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                            referenceMediaItemKey, encodingProfileKey);
                }  
                else
                {
                    field = "EncodingProfileLabel";
                    if (isMetadataPresent(referenceRoot, field))
                    {
                        string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                        referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey,
                                referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                    }        
                }
            }
            
            if (referencePhysicalPathKey != -1)
                dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
            else if (referenceMediaItemKey != -1)
                dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
            else    // referenceLabel
                ;
        }
    }
}

void Validator::validateOverlayTextOnVideoMetadata(int64_t workspaceKey,
    Json::Value parametersRoot, vector<pair<int64_t, DependencyType>>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "Text"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!isMetadataPresent(parametersRoot, mandatoryField))
        {
            Json::StreamWriterBuilder wbuilder;
            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "fontType";
    if (isMetadataPresent(parametersRoot, field))
    {
        string fontType = parametersRoot.get(field, "XXX").asString();
                        
        if (!isFontTypeValid(fontType))
        {
            string errorMessage = string("Unknown fontType")
                + ", fontType: " + fontType
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "fontColor";
    if (isMetadataPresent(parametersRoot, field))
    {
        string fontColor = parametersRoot.get(field, "XXX").asString();
                        
        if (!isFontColorValid(fontColor))
        {
            string errorMessage = string("Unknown fontColor")
                + ", fontColor: " + fontColor
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "textPercentageOpacity";
    if (isMetadataPresent(parametersRoot, field))
    {
        int textPercentageOpacity = parametersRoot.get(field, 200).asInt();
                        
        if (textPercentageOpacity > 100)
        {
            string errorMessage = string("Wrong textPercentageOpacity")
                + ", textPercentageOpacity: " + to_string(textPercentageOpacity)
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "boxEnable";
    if (isMetadataPresent(parametersRoot, field))
    {
        bool boxEnable = parametersRoot.get(field, true).asBool();                        
    }

    field = "boxPercentageOpacity";
    if (isMetadataPresent(parametersRoot, field))
    {
        int boxPercentageOpacity = parametersRoot.get(field, 200).asInt();
                        
        if (boxPercentageOpacity > 100)
        {
            string errorMessage = string("Wrong boxPercentageOpacity")
                + ", boxPercentageOpacity: " + to_string(boxPercentageOpacity)
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
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
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        bool referenceLabel = false;
        
        field = "ReferenceMediaItemKey";
        if (!isMetadataPresent(referenceRoot, field))
        {
            field = "ReferencePhysicalPathKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceIngestionJobKey";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceUniqueName";
                    if (!isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceLabel";
                        if (!isMetadataPresent(referenceRoot, field))
                        {
                            Json::StreamWriterBuilder wbuilder;
                            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                    + ", Field: " + "Reference..."
                                    + ", sParametersRoot: " + sParametersRoot
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                        else
                        {
                            referenceLabel = true;
                        }
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
                referencePhysicalPathKey = referenceRoot.get(field, "XXX").asInt64();
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
            else if (referencePhysicalPathKey != -1)
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        referencePhysicalPathKey, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else if (referenceIngestionJobKey != -1)
            {
                int64_t localReferencePhysicalPathKey;

                tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                        referenceIngestionJobKey, warningIfMissing);  

                tie(referenceMediaItemKey, localReferencePhysicalPathKey, referenceContentType) =
                        mediaItemKeyPhysicalPathKeyAndContentType;
            }
            else if (referenceUniqueName != "")
            {
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        workspaceKey, referenceUniqueName, warningIfMissing);  

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;
            }
            else // referenceLabel
            {
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

        if (referenceLabel == false 
                && referenceContentType != MMSEngineDBFacade::ContentType::Video)
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

        if (referenceLabel == false && referencePhysicalPathKey == -1)
        {
            int64_t encodingProfileKey = -1;

            field = "EncodingProfileKey";
            if (isMetadataPresent(referenceRoot, field))
            {
                int64_t encodingProfileKey = referenceRoot.get(field, "0").asInt64();

                referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                        referenceMediaItemKey, encodingProfileKey);
            }  
            else
            {
                field = "EncodingProfileLabel";
                if (isMetadataPresent(referenceRoot, field))
                {
                    string encodingProfileLabel = referenceRoot.get(field, "0").asString();

                    referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(workspaceKey,
                            referenceMediaItemKey, referenceContentType, encodingProfileLabel);
                }        
            }
        }

        if (referencePhysicalPathKey != -1)
            dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
        else if (referenceMediaItemKey != -1)
            dependencies.push_back(make_pair(referenceMediaItemKey, DependencyType::MediaItemKey));
        else    // referenceLabel
            ;
    }        
}

void Validator::validateEmailNotificationMetadata(
    Json::Value parametersRoot, vector<pair<int64_t,DependencyType>>& dependencies)
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
            Json::StreamWriterBuilder wbuilder;
            string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    ;
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
            string errorMessage = __FILEREF__ + "Field is present but it does not have enough elements"
                    + ", Field: " + field
                    + ", referencesRoot.size(): " + to_string(referencesRoot.size())
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
        {
            Json::Value referenceRoot = referencesRoot[referenceIndex];

            int64_t referenceIngestionJobKey = -1;
            bool referenceLabel = false;
            
            field = "ReferenceIngestionJobKey";
            if (!isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceLabel";
                if (!isMetadataPresent(referenceRoot, field))
                {
                    Json::StreamWriterBuilder wbuilder;
                    string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", Field: " + "Reference..."
                            + ", sParametersRoot: " + sParametersRoot
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                else
                {
                    referenceLabel = true;
                }
            }
            else
            {
                referenceIngestionJobKey = referenceRoot.get(field, "XXX").asInt64();
            }        

            if (referenceIngestionJobKey != -1)
                dependencies.push_back(make_pair(referenceIngestionJobKey, DependencyType::IngestionJobKey));
        }
    }        
}

bool Validator::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull())
        return true;
    else
        return false;
}
