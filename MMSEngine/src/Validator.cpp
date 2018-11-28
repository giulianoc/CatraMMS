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
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        Json::Value configuration
) 
{
    _logger             = logger;
    _mmsEngineDBFacade  = mmsEngineDBFacade;

    _storagePath = configuration["storage"].get("path", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->path: " + _storagePath
    );
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
        "mxf",
        "ts"
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
        "cac_champagne.ttf",
        "OpenSans-BoldItalic.ttf",
        "OpenSans-ExtraBoldItalic.ttf",
        "OpenSans-Italic.ttf",
        "OpenSans-Light.ttf",
        "OpenSans-SemiboldItalic.ttf",
        "Pacifico.ttf",
        "Windsong.ttf",
        "DancingScript-Regular.otf",
        "OpenSans-Bold.ttf",
        "OpenSans-ExtraBold.ttf",
        "OpenSans-LightItalic.ttf",
        "OpenSans-Regular.ttf",
        "OpenSans-Semibold.ttf",
        "Sofia-Regular.otf"
    };

    for (string validFontType: validFontTypes)
    {
        if (fontType == validFontType) 
            return true;
    }
    
    return false;
}

bool Validator::isColorValid(string color)
{
    vector<string> validColors = {
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

    for (string validColor: validColors)
    {
        if (color == validColor) 
            return true;
    }
    
    return false;
}

void Validator::validateEncodingProfilesSetRootMetadata(
    MMSEngineDBFacade::ContentType contentType,
    Json::Value encodingProfilesSetRoot)
{
    vector<string> mandatoryFields = {
        "Label",
        "Profiles"
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

    /*
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
    */
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
        string field = "Label";
        string label = encodingProfileRoot.get(field, "XXX").asString();
        string mmsPredefinedProfilePrefix ("MMS_");
        if (label.compare(0, mmsPredefinedProfilePrefix.size(), mmsPredefinedProfilePrefix) == 0)   
        {
            string errorMessage = __FILEREF__ + "Profiles starting with " + mmsPredefinedProfilePrefix + " are reserved"
                    + ", Label: " + label;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
    {
        string field = "Video";
        Json::Value encodingProfileVideoRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "Codec",
            "Width",
            "Height",
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
            "Codec"
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
        string field = "Label";
        string label = encodingProfileRoot.get(field, "XXX").asString();
        string mmsPredefinedProfilePrefix ("MMS_");
        if (label.compare(0, mmsPredefinedProfilePrefix.size(), mmsPredefinedProfilePrefix) == 0)   
        {
            string errorMessage = __FILEREF__ + "Profiles starting with " + mmsPredefinedProfilePrefix + " are reserved"
                    + ", Label: " + label;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
    {
        string field = "Audio";
        Json::Value encodingProfileAudioRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "Codec"
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
            "FileFormat",
            "Image"
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
        string field = "Label";
        string label = encodingProfileRoot.get(field, "XXX").asString();
        string mmsPredefinedProfilePrefix ("MMS_");
        if (label.compare(0, mmsPredefinedProfilePrefix.size(), mmsPredefinedProfilePrefix) == 0)   
        {
            string errorMessage = __FILEREF__ + "Profiles starting with " + mmsPredefinedProfilePrefix + " are reserved"
                    + ", Label: " + label;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
    {
        string field = "Image";
        Json::Value encodingProfileImageRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "Width",
            "Height",
            "AspectRatio",
            "InterlaceType"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!isMetadataPresent(encodingProfileImageRoot, mandatoryField))
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

void Validator::validateIngestedRootMetadata(int64_t workspaceKey, Json::Value root)
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

    // this method is called when the json is just ingested, for this reason
    // we cannot validate dependencies too because we would not have them (they have to be generated yet)
    // and the check will fail
    bool validateDependenciesToo = false;
    if (taskType == "GroupOfTasks")
    {
        validateGroupOfTasksMetadata(workspaceKey, taskRoot, validateDependenciesToo);
    }
    else
    {
        validateSingleTaskMetadata(workspaceKey, taskRoot, validateDependenciesToo);
    }    
}

void Validator::validateGroupOfTasksMetadata(int64_t workspaceKey, 
        Json::Value groupOfTasksRoot, bool validateDependenciesToo)
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
            validateGroupOfTasksMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }        
    }
    
    validateEvents(workspaceKey, groupOfTasksRoot, validateDependenciesToo);
}

void Validator::validateEvents(int64_t workspaceKey, Json::Value taskOrGroupOfTasksRoot, bool validateDependenciesToo)
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
            validateGroupOfTasksMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot, validateDependenciesToo);
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
            validateGroupOfTasksMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot, validateDependenciesToo);
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
            validateGroupOfTasksMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }
    }    
}

vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>> Validator::validateSingleTaskMetadata(
        int64_t workspaceKey, Json::Value taskRoot, bool validateDependenciesToo)
{
    MMSEngineDBFacade::IngestionType    ingestionType;
    vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>           dependencies;

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

    string label;
    field = "Label";
    if (isMetadataPresent(taskRoot, field))
        label = taskRoot.get(field, "").asString();

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
        validateAddContentMetadata(label, parametersRoot);
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
        validateRemoveContentMetadata(workspaceKey, label, parametersRoot, dependencies);
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
        validateEncodeMetadata(workspaceKey, label, parametersRoot, dependencies);
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
        validateFrameMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validatePeriodicalFramesMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validatePeriodicalFramesMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validateIFramesMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validateIFramesMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validateSlideshowMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validateConcatDemuxerMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validateCutMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validateOverlayImageOnVideoMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validateOverlayTextOnVideoMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);        
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
        validateEmailNotificationMetadata(label, parametersRoot, validateDependenciesToo, dependencies);        
    }
    else if (type == "FTP-Delivery")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::FTPDelivery;
        
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
        validateFTPDeliveryMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "HTTP-Callback")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::HTTPCallback;
        
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
        validateHTTPCallbackMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Local-Copy")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LocalCopy;
        
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
        validateLocalCopyMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Extract-Tracks")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::ExtractTracks;
        
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
        validateExtractTracksMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Post-On-Facebook")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::PostOnFacebook;
        
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
        validatePostOnFacebookMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Field 'Type' is wrong"
                + ", Type: " + type;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
        
    validateEvents(workspaceKey, taskRoot, validateDependenciesToo);
    
    return dependencies;
}

vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>> Validator::validateSingleTaskMetadata(int64_t workspaceKey,
        MMSEngineDBFacade::IngestionType ingestionType, Json::Value parametersRoot)
{
    vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>                     dependencies;

    // we can validate dependencies too because this method is called by the processor
    // when the dependencies would be already generated
    bool validateDependenciesToo = true;

    string label;
    
    if (ingestionType == MMSEngineDBFacade::IngestionType::AddContent)
    {
        validateAddContentMetadata(label, parametersRoot);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::RemoveContent)
    {
        validateRemoveContentMetadata(workspaceKey, label, parametersRoot, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Encode)
    {
        validateEncodeMetadata(workspaceKey, label, parametersRoot, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
    {
        validateFrameMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
            || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames)
    {
        validatePeriodicalFramesMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::IFrames
            || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
    {
        validateIFramesMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Slideshow)
    {
        validateSlideshowMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::ConcatDemuxer)
    {
        validateConcatDemuxerMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Cut)
    {
        validateCutMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayImageOnVideo)
    {
        validateOverlayImageOnVideoMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayTextOnVideo)
    {
        validateOverlayTextOnVideoMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::EmailNotification)
    {
        validateEmailNotificationMetadata(label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::FTPDelivery)
    {
        validateFTPDeliveryMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::HTTPCallback)
    {
        validateHTTPCallbackMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::LocalCopy)
    {
        validateLocalCopyMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::ExtractTracks)
    {
        validateExtractTracksMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::PostOnFacebook)
    {
        validatePostOnFacebookMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
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
    string label, Json::Value parametersRoot)
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
                    + ", label: " + label
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
            + ", label: " + label
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

void Validator::validateRemoveContentMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
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
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
    }    
}

void Validator::validateEncodeMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
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
                    + ", EncodingPriority: " + encodingPriority
                    + ", label: " + label
                    ;
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
                + ", label: " + label
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
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        // Json::Value referenceRoot = referencesRoot[0];

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = true;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
    }    
}

void Validator::validateFrameMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    // see sample in directory samples
     
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
            if (dependencies.size() == 1)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = dependencies[0];

                if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer a video content"
                            + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                        + ", referenceMediaItemKey: " + to_string(key)
                        + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                        + ", label: " + label
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
    }    
}

void Validator::validatePeriodicalFramesMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
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
                    + ", label: " + label
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
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
            if (dependencies.size() == 1)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = dependencies[0];

                if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer a video content"
                            + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                        + ", referenceMediaItemKey: " + to_string(key)
                        + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                        + ", label: " + label
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
    }        
}

void Validator::validateIFramesMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    // see sample in directory samples
        
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
            if (dependencies.size() == 1)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = dependencies[0];

                if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer a video content"
                            + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                        + ", referenceMediaItemKey: " + to_string(key)
                        + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                        + ", label: " + label
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
    }    
}

void Validator::validateSlideshowMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{    
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
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
            for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

                if (referenceContentType != MMSEngineDBFacade::ContentType::Image)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer an image content"
                        + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                        + ", referenceMediaItemKey: " + to_string(key)
                        + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                        + ", label: " + label
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
    }
}

void Validator::validateConcatDemuxerMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    // see sample in directory samples
            
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        /*
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() < 2)
        {
            string errorMessage = __FILEREF__ + "Field is present but it does not have enough elements (2)"
                    + ", Field: " + field
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        */

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
            // It is not important the number of References but how many media items it refers.
            // For example ReferenceIngestionJobKey is just one Reference but it could reference
            // a log of media items in case the IngestionJob generates a log of media contents
            if (dependencies.size() < 1)
            {
                string errorMessage = __FILEREF__ + "Field is present but it does not refer enough elements (1)"
                        + ", Field: " + field
                        + ", dependencies.size: " + to_string(dependencies.size())
                        + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            MMSEngineDBFacade::ContentType firstContentType;
            bool firstContentTypeInitialized = false;
            for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

                if (firstContentTypeInitialized)
                {
                    if (referenceContentType != firstContentType)
                    {
                        string errorMessage = __FILEREF__ + "Reference... does not refer the correct ContentType"
                                + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                            + ", referenceMediaItemKey: " + to_string(key)
                            + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                            + ", label: " + label
                                ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                else
                {
                    if (referenceContentType != MMSEngineDBFacade::ContentType::Video
                            && referenceContentType != MMSEngineDBFacade::ContentType::Audio)
                    {
                        string errorMessage = __FILEREF__ + "Reference... does not refer a video or audio content"
                            + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                            + ", referenceMediaItemKey: " + to_string(key)
                            + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                            + ", label: " + label
                                ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }

                    firstContentType = referenceContentType;
                    firstContentTypeInitialized = true;
                }
            }
        }
    }
}

void Validator::validateCutMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
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
                + ", label: " + label
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
                + ", label: " + label
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    field = "References";
    if (isMetadataPresent(parametersRoot, field))
    {
        /*
        Json::Value referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        */

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
            if (dependencies.size() != 1)
            {
                string errorMessage = __FILEREF__ + "No correct number of Media to be cut"
                        + ", dependencies.size: " + to_string(dependencies.size())
                        + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = dependencies[0];

                if (referenceContentType != MMSEngineDBFacade::ContentType::Video
                        && referenceContentType != MMSEngineDBFacade::ContentType::Audio)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer a video-audio content"
                            + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                        + ", referenceMediaItemKey: " + to_string(key)
                        + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                        + ", label: " + label
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
    }        
}

void Validator::validateOverlayImageOnVideoMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    
    vector<string> mandatoryFields = {
        "ImagePosition_X_InPixel",
        "ImagePosition_Y_InPixel"
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
                    + ", label: " + label
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
                    + ", Field: " + field
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
            if (dependencies.size() == 2)
            {
                int64_t key_1;
                MMSEngineDBFacade::ContentType referenceContentType_1;
                Validator::DependencyType dependencyType_1;

                tie(key_1, referenceContentType_1, dependencyType_1) = dependencies[0];

                int64_t key_2;
                MMSEngineDBFacade::ContentType referenceContentType_2;
                Validator::DependencyType dependencyType_2;

                tie(key_2, referenceContentType_2, dependencyType_2) = dependencies[1];

                if (referenceContentType_1 == MMSEngineDBFacade::ContentType::Video
                        && referenceContentType_2 == MMSEngineDBFacade::ContentType::Image)
                {
                }
                else if (referenceContentType_1 == MMSEngineDBFacade::ContentType::Image
                        && referenceContentType_2 == MMSEngineDBFacade::ContentType::Video)
                {
                }
                else
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer a video and an image content"
                            + ", dependencyType_1: " + to_string(static_cast<int>(dependencyType_1))
                        + ", referenceMediaItemKey_1: " + to_string(key_1)
                        + ", referenceContentType_1: " + MMSEngineDBFacade::toString(referenceContentType_1)
                            + ", dependencyType_2: " + to_string(static_cast<int>(dependencyType_2))
                        + ", referenceMediaItemKey_2: " + to_string(key_2)
                        + ", referenceContentType_2: " + MMSEngineDBFacade::toString(referenceContentType_2)
                        + ", label: " + label
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
    }
}

void Validator::validateOverlayTextOnVideoMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
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
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "FontType";
    if (isMetadataPresent(parametersRoot, field))
    {
        string fontType = parametersRoot.get(field, "XXX").asString();
                        
        if (!isFontTypeValid(fontType))
        {
            string errorMessage = string("Unknown fontType")
                + ", fontType: " + fontType
                + ", label: " + label
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "FontColor";
    if (isMetadataPresent(parametersRoot, field))
    {
        string fontColor = parametersRoot.get(field, "XXX").asString();
                        
        if (!isColorValid(fontColor))
        {
            string errorMessage = string("Unknown fontColor")
                + ", fontColor: " + fontColor
                + ", label: " + label
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "TextPercentageOpacity";
    if (isMetadataPresent(parametersRoot, field))
    {
        int textPercentageOpacity = parametersRoot.get(field, 200).asInt();
                        
        if (textPercentageOpacity > 100)
        {
            string errorMessage = string("Wrong textPercentageOpacity")
                + ", textPercentageOpacity: " + to_string(textPercentageOpacity)
                + ", label: " + label
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "BoxEnable";
    if (isMetadataPresent(parametersRoot, field))
    {
        bool boxEnable = parametersRoot.get(field, true).asBool();                        
    }

    field = "BoxPercentageOpacity";
    if (isMetadataPresent(parametersRoot, field))
    {
        int boxPercentageOpacity = parametersRoot.get(field, 200).asInt();
                        
        if (boxPercentageOpacity > 100)
        {
            string errorMessage = string("Wrong boxPercentageOpacity")
                + ", boxPercentageOpacity: " + to_string(boxPercentageOpacity)
                + ", label: " + label
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
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
                    + ", EncodingPriority: " + encodingPriority
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "BoxColor";
    if (isMetadataPresent(parametersRoot, field))
    {
        string boxColor = parametersRoot.get(field, "XXX").asString();
                        
        if (!isColorValid(boxColor))
        {
            string errorMessage = string("Unknown boxColor")
                + ", boxColor: " + boxColor
                + ", label: " + label
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
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
            if (dependencies.size() == 1)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = dependencies[0];

                if (referenceContentType != MMSEngineDBFacade::ContentType::Video)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer a video content"
                            + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                        + ", referenceMediaItemKey: " + to_string(key)
                        + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                        + ", label: " + label
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
    }        
}

void Validator::validateEmailNotificationMetadata(string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
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
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
    if (validateDependenciesToo)
    {
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
                        + ", label: " + label
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
                                + ", label: " + label
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
                {
                    MMSEngineDBFacade::ContentType      referenceContentType;

                    dependencies.push_back(make_tuple(referenceIngestionJobKey, referenceContentType, DependencyType::IngestionJobKey));
                }
            }
        } 
    }
}

void Validator::validateFTPDeliveryMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "Server",
        "UserName",
        "Password"
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
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
    if (validateDependenciesToo)
    {
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
                        + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
            bool encodingProfileFieldsToBeManaged = false;
            fillDependencies(workspaceKey, parametersRoot, dependencies,
                    priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                    encodingProfileFieldsToBeManaged);
        } 
    }
}

void Validator::validateHTTPCallbackMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "HostName",
        "URI"
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
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
    string field = "Method";
    if (isMetadataPresent(parametersRoot, field))
    {
        string method = parametersRoot.get(field, "XXX").asString();
                        
        if (method != "GET" && method != "POST")
        {
            string errorMessage = string("Unknown Method")
                + ", method: " + method
                + ", label: " + label
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "Headers";
    if (isMetadataPresent(parametersRoot, field))
    {
        Json::Value headersRoot = parametersRoot[field];
        
        if (headersRoot.type() != Json::arrayValue)
        {
            string errorMessage = __FILEREF__ + "Field is present but it is not an array of strings"
                    + ", Field: " + field
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        for (int userHeaderIndex = 0; userHeaderIndex < headersRoot.size(); ++userHeaderIndex)
        {
            if (headersRoot[userHeaderIndex].type() != Json::stringValue)
            {
                string errorMessage = __FILEREF__ + "Field is present but it does not contain strings"
                        + ", Field: " + field
                    + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }   
        
    if (validateDependenciesToo)
    {
        // References is optional because in case of dependency managed automatically
        // by MMS (i.e.: onSuccess)
        field = "References";
        if (isMetadataPresent(parametersRoot, field))
        {
            Json::Value referencesRoot = parametersRoot[field];
            if (referencesRoot.size() < 1)
            {
                string errorMessage = __FILEREF__ + "Field is present but it does not have enough elements"
                        + ", Field: " + field
                        + ", referencesRoot.size(): " + to_string(referencesRoot.size())
                        + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
            bool encodingProfileFieldsToBeManaged = false;
            fillDependencies(workspaceKey, parametersRoot, dependencies,
                    priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                    encodingProfileFieldsToBeManaged);
        } 
    }
}

void Validator::validateLocalCopyMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "LocalPath"
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
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
    string field = "LocalPath";
    string localPath = parametersRoot.get(field, "XXX").asString();
    if (localPath.size() >= _storagePath.size() && 0 == localPath.compare(0, _storagePath.size(), _storagePath))
    {
        string errorMessage = __FILEREF__ + "'LocalPath' cannot be within the dedicated storage managed by MMS"
                + ", Field: " + field
                + ", localPath: " + localPath
                    + ", label: " + label
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    if (validateDependenciesToo)
    {
        // References is optional because in case of dependency managed automatically
        // by MMS (i.e.: onSuccess)
        field = "References";
        if (isMetadataPresent(parametersRoot, field))
        {
            Json::Value referencesRoot = parametersRoot[field];
            if (referencesRoot.size() < 1)
            {
                string errorMessage = __FILEREF__ + "Field is present but it does not have enough elements"
                        + ", Field: " + field
                        + ", referencesRoot.size(): " + to_string(referencesRoot.size())
                        + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
            bool encodingProfileFieldsToBeManaged = false;
            fillDependencies(workspaceKey, parametersRoot, dependencies,
                    priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                    encodingProfileFieldsToBeManaged);
        }  
    }
}

void Validator::validateExtractTracksMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{

    
    vector<string> mandatoryFields = {
        "Tracks",
        "OutputFileFormat"
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
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "Tracks";
    Json::Value tracksToot = parametersRoot[field];
    if (tracksToot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No correct number of Tracks"
                + ", tracksToot.size: " + to_string(tracksToot.size())
                    + ", label: " + label
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    for (int trackIndex = 0; trackIndex < tracksToot.size(); trackIndex++)
    {
        Json::Value trackRoot = tracksToot[trackIndex];
        
        field = "TrackType";
        if (!isMetadataPresent(trackRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTrackRoot = Json::writeString(wbuilder, trackRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTrackRoot: " + sTrackRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string trackType = trackRoot.get(field, "XXX").asString();
        if (trackType != "video" && trackType != "audio")
        {
            string errorMessage = __FILEREF__ + field + " is wrong (it could be only 'video' or 'audio'"
                    + ", Field: " + field
                    + ", trackType: " + trackType
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "OutputFileFormat";
    string outputFileFormat = parametersRoot.get(field, "XXX").asString();
    if (!isVideoAudioFileFormat(outputFileFormat))
    {
        string errorMessage = __FILEREF__ + field + " is wrong (it could be only 'video' or 'audio'"
                + ", Field: " + field
                + ", outputFileFormat: " + outputFileFormat
                + ", label: " + label
                ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }

    if (validateDependenciesToo)
    {
        // References is optional because in case of dependency managed automatically
        // by MMS (i.e.: onSuccess)
        field = "References";
        if (isMetadataPresent(parametersRoot, field))
        {
            Json::Value referencesRoot = parametersRoot[field];
            if (referencesRoot.size() == 0)
            {
                string errorMessage = __FILEREF__ + "No References"
                        + ", referencesRoot.size: " + to_string(referencesRoot.size())
                        + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
            bool encodingProfileFieldsToBeManaged = false;
            fillDependencies(workspaceKey, parametersRoot, dependencies,
                    priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                    encodingProfileFieldsToBeManaged);

            for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

                if (referenceContentType != MMSEngineDBFacade::ContentType::Video
                        && referenceContentType != MMSEngineDBFacade::ContentType::Audio)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer a video or audio content"
                        + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                        + ", referenceMediaItemKey: " + to_string(key)
                        + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                        + ", label: " + label
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }    
    }
}

void Validator::validatePostOnFacebookMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{     
    vector<string> mandatoryFields = {
        "NodeId",   // page_id || user_id || event_id || group_id
        "AccessToken"
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
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    if (validateDependenciesToo)
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
                        + ", referencesRoot.size: " + to_string(referencesRoot.size())
                        + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
            bool encodingProfileFieldsToBeManaged = false;
            fillDependencies(workspaceKey, parametersRoot, dependencies,
                    priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                    encodingProfileFieldsToBeManaged);

            for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

                if (referenceContentType != MMSEngineDBFacade::ContentType::Video
                        && referenceContentType != MMSEngineDBFacade::ContentType::Image)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer a video or image content"
                        + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
                        + ", referenceMediaItemKey: " + to_string(key)
                        + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
                        + ", label: " + label
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
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

void Validator::fillDependencies(int64_t workspaceKey, Json::Value parametersRoot, 
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies,
        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
        bool encodingProfileFieldsToBeManaged)
{
    string field = "References";
    Json::Value referencesRoot = parametersRoot[field];

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
                string userData;
                
                pair<MMSEngineDBFacade::ContentType,string> contentTypeAndUserData = 
                        _mmsEngineDBFacade->getMediaItemKeyDetails(referenceMediaItemKey, warningIfMissing); 
                tie(referenceContentType, userData) = contentTypeAndUserData;
            }
            else if (referencePhysicalPathKey != -1)
            {
                string userData;

                tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        referencePhysicalPathKey, warningIfMissing);  

                tie(referenceMediaItemKey,referenceContentType, userData) 
                        = mediaItemKeyContentTypeAndUserData;
            }
            else if (referenceIngestionJobKey != -1)
            {
                // the difference with the other if is that here, associated to the ingestionJobKey,
                // we may have a list of mediaItems (i.e.: periodic-frame)
                vector<tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType>> mediaItemsDetails;

                _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                        referenceIngestionJobKey, mediaItemsDetails, warningIfMissing);  

                if (mediaItemsDetails.size() == 0)
                {
                    Json::StreamWriterBuilder wbuilder;
                    string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

                    string errorMessage = __FILEREF__ + "No media items found"
                            + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                            + ", sParametersRoot: " + sParametersRoot
                            ;
                    _logger->warn(errorMessage);
                }
                else
                {
                    for (tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> 
                            mediaItemKeyPhysicalPathKeyAndContentType: mediaItemsDetails)
                    {
                        if (priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey)
                        {
                            tie(referenceMediaItemKey, referencePhysicalPathKey, referenceContentType) =
                                mediaItemKeyPhysicalPathKeyAndContentType;

                            if (referencePhysicalPathKey != -1)
                                dependencies.push_back(make_tuple(referencePhysicalPathKey, referenceContentType, DependencyType::PhysicalPathKey));
                            else if (referenceMediaItemKey != -1)
                                dependencies.push_back(make_tuple(referenceMediaItemKey, referenceContentType, DependencyType::MediaItemKey));
                            else    // referenceLabel
                                ;
                        }
                        else
                        {
                            int64_t localReferencePhysicalPathKey;

                            tie(referenceMediaItemKey, localReferencePhysicalPathKey, referenceContentType) =
                                mediaItemKeyPhysicalPathKeyAndContentType;

                            /*
                            if (referencePhysicalPathKey != -1)
                                dependencies.push_back(make_pair(referencePhysicalPathKey,DependencyType::PhysicalPathKey));
                            else 
                            */
                            if (referenceMediaItemKey != -1)
                                dependencies.push_back(make_tuple(referenceMediaItemKey, referenceContentType, DependencyType::MediaItemKey));
                            else    // referenceLabel
                                ;
                        }
                    }
                }
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

        if (referenceIngestionJobKey == -1)
        {
            // case referenceIngestionJobKey != -1 is already managed inside the previous if

            if (encodingProfileFieldsToBeManaged)
            {
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
            }
            
            if (referencePhysicalPathKey != -1)
                dependencies.push_back(make_tuple(referencePhysicalPathKey, referenceContentType, DependencyType::PhysicalPathKey));
            else if (referenceMediaItemKey != -1)
                dependencies.push_back(make_tuple(referenceMediaItemKey, referenceContentType, DependencyType::MediaItemKey));
            else    // referenceLabel
                ;
        }
    }
}