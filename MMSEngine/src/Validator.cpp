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

#include "JSONUtils.h"
#include "Validator.h"

Validator::Validator(
        shared_ptr<spdlog::logger> logger, 
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        Json::Value configuration
) 
{
    _logger             = logger;
    _mmsEngineDBFacade  = mmsEngineDBFacade;

    _storagePath = configuration["storage"].get("path", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->path: " + _storagePath
    );
}

Validator::Validator(const Validator& orig) {
}

Validator::~Validator() {
}

void Validator::validateIngestedRootMetadata(int64_t workspaceKey, Json::Value root)
{
    string field = "Type";
    if (!JSONUtils::isMetadataPresent(root, field))
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
    string type = root.get(field, "").asString();
    if (type != "Workflow")
    {
        string errorMessage = __FILEREF__ + "Type field is wrong"
                + ", Type: " + type;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    field = "Task";
    if (!JSONUtils::isMetadataPresent(root, field))
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
    if (!JSONUtils::isMetadataPresent(taskRoot, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sRoot = Json::writeString(wbuilder, root);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sRoot: " + sRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }    
    string taskType = taskRoot.get(field, "").asString();

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
    if (!JSONUtils::isMetadataPresent(groupOfTasksRoot, field))
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
    
	validateGroupOfTasksMetadata(workspaceKey, parametersRoot);

    field = "Tasks";
    if (!JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sParametersRoot: " + sParametersRoot;
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
			string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", Field: " + field
                + ", sParametersRoot: " + sParametersRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "").asString();

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

void Validator::validateGroupOfTasksMetadata(int64_t workspaceKey, 
	Json::Value parametersRoot)
{
    string field = "ExecutionType";
    if (!JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        Json::StreamWriterBuilder wbuilder;
        string sParametersRoot = Json::writeString(wbuilder, parametersRoot);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sParametersRoot: " + sParametersRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string executionType = parametersRoot.get(field, "").asString();
    if (executionType != "parallel" 
            && executionType != "sequential")
    {
        string errorMessage = __FILEREF__ + "executionType field is wrong"
                + ", executionType: " + executionType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
}

void Validator::validateEvents(int64_t workspaceKey, Json::Value taskOrGroupOfTasksRoot, bool validateDependenciesToo)
{
    string field = "OnSuccess";
    if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onSuccessRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!JSONUtils::isMetadataPresent(onSuccessRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskOrGroupOfTasksRoot = Json::writeString(wbuilder, taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "").asString();

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
    if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onErrorRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!JSONUtils::isMetadataPresent(onErrorRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskOrGroupOfTasksRoot = Json::writeString(wbuilder, taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "").asString();

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
    if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        Json::Value onCompleteRoot = taskOrGroupOfTasksRoot[field];
        
        field = "Task";
        if (!JSONUtils::isMetadataPresent(onCompleteRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            Json::StreamWriterBuilder wbuilder;
            string sTaskOrGroupOfTasksRoot = Json::writeString(wbuilder, taskOrGroupOfTasksRoot);

            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = taskRoot.get(field, "").asString();

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

vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>
	Validator::validateSingleTaskMetadata(
	int64_t workspaceKey, Json::Value taskRoot, bool validateDependenciesToo)
{
    MMSEngineDBFacade::IngestionType    ingestionType;
    vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>           dependencies;

    string field = "Type";
    if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
    if (JSONUtils::isMetadataPresent(taskRoot, field))
        label = taskRoot.get(field, "").asString();

    string type = taskRoot.get("Type", "").asString();
    if (type == "Add-Content")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::AddContent;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateEmailNotificationMetadata(workspaceKey, label, parametersRoot,
				validateDependenciesToo, dependencies);        
    }
    else if (type == "Media-Cross-Reference")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::MediaCrossReference;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateMediaCrossReferenceMetadata(workspaceKey, label, parametersRoot,
				validateDependenciesToo, dependencies);        
    }
    else if (type == "FTP-Delivery")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::FTPDelivery;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
    else if (type == "Post-On-YouTube")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::PostOnYouTube;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validatePostOnYouTubeMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Face-Recognition")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::FaceRecognition;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateFaceRecognitionMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Face-Identification")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::FaceIdentification;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateFaceIdentificationMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Live-Recorder")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LiveRecorder;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateLiveRecorderMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Change-File-Format")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::ChangeFileFormat;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateChangeFileFormatMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Video-Speed")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::VideoSpeed;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateVideoSpeedMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Picture-In-Picture")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::VideoSpeed;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validatePictureInPictureMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Live-Proxy")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LiveProxy;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateLiveProxyMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Live-Grid")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LiveGrid;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateLiveGridMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);
    }
    else if (type == "Live-Cut")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LiveCut;
        
        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateLiveCutMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
    }
    else if (type == "Workflow-As-Library")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::WorkflowAsLibrary;

        field = "Parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
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
        validateWorkflowAsLibraryMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo, dependencies);
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

vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>
	Validator::validateSingleTaskMetadata(int64_t workspaceKey,
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
        validateEmailNotificationMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::MediaCrossReference)
    {
        validateMediaCrossReferenceMetadata(workspaceKey, label, parametersRoot, 
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
    else if (ingestionType == MMSEngineDBFacade::IngestionType::PostOnYouTube)
    {
        validatePostOnYouTubeMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::FaceRecognition)
    {
        validateFaceRecognitionMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::FaceIdentification)
    {
        validateFaceIdentificationMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveRecorder)
    {
        validateLiveRecorderMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::ChangeFileFormat)
    {
        validateChangeFileFormatMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::VideoSpeed)
    {
        validateVideoSpeedMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::PictureInPicture)
    {
        validatePictureInPictureMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveProxy)
    {
        validateLiveProxyMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveGrid)
    {
        validateLiveGridMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveCut)
    {
        validateLiveCutMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);        
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::WorkflowAsLibrary)
    {
        validateWorkflowAsLibraryMetadata(workspaceKey, label, parametersRoot, 
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
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
    string fileFormat = parametersRoot.get(field, "").asString();

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

	// in case of externalContent, it cannot be inside mms storage
	{
        field = "SourceURL";
        if (JSONUtils::isMetadataPresent(parametersRoot, field))
        {
			string sourceURL = parametersRoot.get(field, "").asString();

			string externalStoragePrefix("externalStorage://");
            if (sourceURL.size() >= externalStoragePrefix.size()
                    && 0 == sourceURL.compare(0, externalStoragePrefix.size(), externalStoragePrefix))
            {
				string externalStoragePathName = sourceURL.substr(externalStoragePrefix.length());
				if (externalStoragePathName.size() >= _storagePath.size()
						&& 0 == externalStoragePathName.compare(0, _storagePath.size(), _storagePath))
				{
					string errorMessage = __FILEREF__ + "'SourceURL' cannot be within the dedicated storage managed by MMS"
						+ ", Field: " + field
						+ ", sourceURL: " + sourceURL
						+ ", label: " + label
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
	}

    field = "CrossReference";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		Json::Value crossReferenceRoot = parametersRoot[field];

		// in AddContent MediaItemKey has to be present
		bool mediaItemKeyMandatory = true;
		validateCrossReference(label, crossReferenceRoot, mediaItemKeyMandatory);
	}

    /*
    // Territories
    {
        field = "Territories";
        if (JSONUtils::isMetadataPresent(parametersRoot, field))
        {
            const Json::Value territories = parametersRoot[field];
            
            for( Json::ValueIterator itr = territories.begin() ; itr != territories.end() ; itr++ ) 
            {
                Json::Value territory = territories[territoryIndex];
            }
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
    }
}

void Validator::validateEncodeMetadata(int64_t workspaceKey, string label,
	Json::Value parametersRoot,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
	string field = "EncodingPriority";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string encodingPriority = parametersRoot.get(field, "").asString();
        try
        {
			// it generate an exception in case of wrong string
            MMSEngineDBFacade::toEncodingPriority(encodingPriority);
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
    if (!JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetKeyField)
            && !JSONUtils::isMetadataPresent(parametersRoot, encodingProfilesSetLabelField)
            && !JSONUtils::isMetadataPresent(parametersRoot, encodingProfileLabelField)
            && !JSONUtils::isMetadataPresent(parametersRoot, encodingProfileKeyField))
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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
    if (!JSONUtils::isMetadataPresent(parametersRoot, field))
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
    if (!JSONUtils::isMetadataPresent(parametersRoot, endTimeInSecondsField)
            && !JSONUtils::isMetadataPresent(parametersRoot, framesNumberField))
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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
    
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
		// before the check was
		//	if (referencesRoot.size() != 2)
		// This was changed to > 2 because it could be used
		// the "DependOnIngestionJobKeysToBeAddedToReferences" thg
        if (referencesRoot.size() > 2)
        {
            string errorMessage = __FILEREF__ + "Field is present but it has more than two elements"
                    + ", Field: " + field
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string fontType = parametersRoot.get(field, "").asString();
                        
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string fontColor = parametersRoot.get(field, "").asString();
                        
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        int textPercentageOpacity = JSONUtils::asInt(parametersRoot, field, 200);
                        
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        bool boxEnable = JSONUtils::asBool(parametersRoot, field, true);                        
    }

    field = "BoxPercentageOpacity";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        int boxPercentageOpacity = JSONUtils::asInt(parametersRoot, field, 200);
                        
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string encodingPriority = parametersRoot.get(field, "").asString();
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string boxColor = parametersRoot.get(field, "").asString();
                        
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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

void Validator::validateEmailNotificationMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "ConfigurationLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		Json::Value referencesRoot = parametersRoot[field];
		/*
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
		*/
		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
		bool encodingProfileFieldsToBeManaged = false;
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
			/*
            for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
            {
                Json::Value referenceRoot = referencesRoot[referenceIndex];

                int64_t referenceIngestionJobKey = -1;
                bool referenceLabel = false;

                field = "ReferenceIngestionJobKey";
                if (!JSONUtils::isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceLabel";
                    if (!JSONUtils::isMetadataPresent(referenceRoot, field))
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
                    referenceIngestionJobKey = referenceRoot.get(field, "").asInt64();
                }        

                if (referenceIngestionJobKey != -1)
                {
                    MMSEngineDBFacade::ContentType      referenceContentType;

                    dependencies.push_back(make_tuple(referenceIngestionJobKey, referenceContentType, DependencyType::IngestionJobKey));
                }
            }
			*/
        } 
    }
}

void Validator::validateMediaCrossReferenceMetadata(int64_t workspaceKey, string label,
	Json::Value parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    // see sample in directory samples

	// in MediaCrossReference, MediaItemKey may not be present because inherit from parent Task
	bool mediaItemKeyMandatory = false;
	validateCrossReference(label, parametersRoot, mediaItemKeyMandatory);

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	string field = "References";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		Json::Value referencesRoot = parametersRoot[field];
		// before the check was
		//	if (referencesRoot.size() != 2)
		// This was changed to > 2 because it could be used
		// the "DependOnIngestionJobKeysToBeAddedToReferences" tag
		// or it is a ReferenceIngestionJob referring a number of contents
        if (referencesRoot.size() > 2)
        {
            string errorMessage = __FILEREF__ + "Field is present but it has more than two elements"
				+ ", Field: " + field
				+ ", referencesRoot.size(): " + to_string(referencesRoot.size())
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
		bool encodingProfileFieldsToBeManaged = false;
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);
		if (validateDependenciesToo)
		{
            if (dependencies.size() != 2)
			{
				string errorMessage = __FILEREF__ + "Field is present but it has a wrong number of elements"
					+ ", Field: " + field
					+ ", dependencies.size(): " + to_string(dependencies.size())
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
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
        "ConfigurationLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);
		if (validateDependenciesToo)
		{
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
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string method = parametersRoot.get(field, "").asString();
                        
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

	/*
	 * 2020-03-07: headers is now a semicolon string
    field = "Headers";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
	*/
        
	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	field = "References";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);
		if (validateDependenciesToo)
		{
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
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
    string localPath = parametersRoot.get(field, "").asString();
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

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	field = "References";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);
		if (validateDependenciesToo)
		{
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
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
        if (!JSONUtils::isMetadataPresent(trackRoot, field))
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
        string trackType = trackRoot.get(field, "").asString();
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
    string outputFileFormat = parametersRoot.get(field, "").asString();
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

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	field = "References";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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
        "ConfigurationLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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

void Validator::validatePostOnYouTubeMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{     
    vector<string> mandatoryFields = {
        "ConfigurationLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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

    string field = "Privacy";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string youTubePrivacy = parametersRoot.get(field, "").asString();

        if (youTubePrivacy != "private" && youTubePrivacy != "public")
        {
            string errorMessage = __FILEREF__ + field + " is wrong (it could be only 'private' or 'public'"
                    + ", Field: " + field
                    + ", youTubePrivacy: " + youTubePrivacy
                    + ", label: " + label
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	field = "References";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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

void Validator::validateFaceRecognitionMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
        
    vector<string> mandatoryFields = {
        "CascadeName",
        "Output"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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

    string field = "CascadeName";
    string faceRecognitionCascadeName = parametersRoot.get(field, "").asString();
    if (!isFaceRecognitionCascadeNameValid(faceRecognitionCascadeName))
    {
        string errorMessage = __FILEREF__ + field + " is wrong (it could be only "
                + "haarcascade_frontalface_alt, haarcascade_frontalface_alt2, "
                + "haarcascade_frontalface_alt_tree or haarcascade_frontalface_default"
                + ")"
                + ", Field: " + field
                + ", cascadeName: " + faceRecognitionCascadeName
                + ", label: " + label
                ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }

    field = "Output";
    string faceRecognitionOutput = parametersRoot.get(field, "").asString();
    if (!isFaceRecognitionOutputValid(faceRecognitionOutput))
    {
        string errorMessage = __FILEREF__ + field + " is wrong (it could be only "
                + "VideoWithHighlightedFaces, ImagesToBeUsedInDeepLearnedModel or FrameContainingFace"
                + ")"
                + ", Field: " + field
                + ", Output: " + faceRecognitionOutput
                + ", label: " + label
                ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	field = "References";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);

		if (validateDependenciesToo)
		{
			if (dependencies.size() != 1)
			{
				string errorMessage = __FILEREF__ + "No dependencies found"
					+ ", dependencies.size: " + to_string(dependencies.size())
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

            // for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
            tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>&
					keyAndDependencyType	=  dependencies[0];
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

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

void Validator::validateFaceIdentificationMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
        
    vector<string> mandatoryFields = {
        "CascadeName",
        "DeepLearnedModelTags"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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

    string field = "CascadeName";
    string faceIdentificationCascadeName = parametersRoot.get(field, "").asString();
    if (!isFaceRecognitionCascadeNameValid(faceIdentificationCascadeName))
    {
        string errorMessage = __FILEREF__ + field + " is wrong (it could be only "
                + "haarcascade_frontalface_alt, haarcascade_frontalface_alt2, "
                + "haarcascade_frontalface_alt_tree or haarcascade_frontalface_default"
                + ")"
                + ", Field: " + field
                + ", cascadeName: " + faceIdentificationCascadeName
                + ", label: " + label
                ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }

    field = "DeepLearnedModelTags";
    if (!parametersRoot[field].isArray()
			|| parametersRoot[field].size() == 0)
    {
        string errorMessage = __FILEREF__ + field + " is not an array or the array is empty"
                + ", Field: " + field
                + ", label: " + label
                ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	field = "References";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);

		if (validateDependenciesToo)
		{
			if (dependencies.size() != 1)
			{
				string errorMessage = __FILEREF__ + "No Dependencies found"
					+ ", dependencies.size: " + to_string(dependencies.size())
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

            // for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
            tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>&
					keyAndDependencyType	=  dependencies[0];
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

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

void Validator::validateLiveRecorderMetadata(int64_t workspaceKey, string label,
	Json::Value parametersRoot,
	bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
        
	vector<string> mandatoryFields = {
		"ConfigurationLabel",
		"RecordingPeriod",
		"SegmentDuration"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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

    string field = "SegmentDuration";
	int segmentDuration = JSONUtils::asInt(parametersRoot, field, 1);
	if (segmentDuration % 2 != 0 || segmentDuration < 10)
	{
		Json::StreamWriterBuilder wbuilder;
		string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

		string errorMessage = __FILEREF__ + "Field has a wrong value (it is not even or slower than 10)"
			+ ", Field: " + field
			+ ", value: " + to_string(segmentDuration)
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

    field = "UniqueName";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		Json::StreamWriterBuilder wbuilder;
		string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

		string errorMessage = __FILEREF__ + "Field cannot be present in this Task"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

    field = "RecordingPeriod";
	Json::Value recordingPeriodRoot = parametersRoot[field];
    field = "Start";
	if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
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
	// next code is the same in the MMSEngineProcessor class
	time_t utcRecordingPeriodStart;
	{
		string recordingPeriodStart = recordingPeriodRoot.get(field, "").asString();

		unsigned long       ulUTCYear;
		unsigned long       ulUTCMonth;
		unsigned long       ulUTCDay;
		unsigned long       ulUTCHour;
		unsigned long       ulUTCMinutes;
		unsigned long       ulUTCSeconds;
		tm                  tmRecordingPeriodStart;
		int                 sscanfReturn;


		// _logger->error(__FILEREF__ + "recordingPeriodStart 1: " + recordingPeriodStart);
		// recordingPeriodStart.replace(10, 1, string(" "), 0, 1);
		// _logger->error(__FILEREF__ + "recordingPeriodStart 2: " + recordingPeriodStart);
		if ((sscanfReturn = sscanf (recordingPeriodStart.c_str(),
			"%4lu-%2lu-%2luT%2lu:%2lu:%2luZ",
			&ulUTCYear,
			&ulUTCMonth,
			&ulUTCDay,
			&ulUTCHour,
			&ulUTCMinutes,
			&ulUTCSeconds)) != 6)
		{
			Json::StreamWriterBuilder wbuilder;
			string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

			string errorMessage = __FILEREF__ + "Field has a wrong format (sscanf failed)"
				+ ", Field: " + field
				+ ", sscanfReturn: " + to_string(sscanfReturn)
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		time (&utcRecordingPeriodStart);
		gmtime_r(&utcRecordingPeriodStart, &tmRecordingPeriodStart);

		tmRecordingPeriodStart.tm_year      = ulUTCYear - 1900;
		tmRecordingPeriodStart.tm_mon       = ulUTCMonth - 1;
		tmRecordingPeriodStart.tm_mday      = ulUTCDay;
		tmRecordingPeriodStart.tm_hour      = ulUTCHour;                       
		tmRecordingPeriodStart.tm_min       = ulUTCMinutes;
		tmRecordingPeriodStart.tm_sec       = ulUTCSeconds;

		utcRecordingPeriodStart = timegm(&tmRecordingPeriodStart);
	}

    field = "End";
	if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
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
	// next code is the same in the MMSEngineProcessor class
	time_t utcRecordingPeriodEnd;
	{
		string recordingPeriodEnd = recordingPeriodRoot.get(field, "").asString();

		unsigned long       ulUTCYear;
		unsigned long       ulUTCMonth;
		unsigned long       ulUTCDay;
		unsigned long       ulUTCHour;
		unsigned long       ulUTCMinutes;
		unsigned long       ulUTCSeconds;
		tm                  tmRecordingPeriodEnd;
		int                 sscanfReturn;


		// _logger->error(__FILEREF__ + "recordingPeriodStart 1: " + recordingPeriodStart);
		// recordingPeriodStart.replace(10, 1, string(" "), 0, 1);
		// _logger->error(__FILEREF__ + "recordingPeriodStart 2: " + recordingPeriodStart);
		if ((sscanfReturn = sscanf (recordingPeriodEnd.c_str(),
			"%4lu-%2lu-%2luT%2lu:%2lu:%2luZ",
			&ulUTCYear,
			&ulUTCMonth,
			&ulUTCDay,
			&ulUTCHour,
			&ulUTCMinutes,
			&ulUTCSeconds)) != 6)
		{
			Json::StreamWriterBuilder wbuilder;
			string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

			string errorMessage = __FILEREF__ + "Field has a wrong format (sscanf failed)"
				+ ", Field: " + field
				+ ", sscanfReturn: " + to_string(sscanfReturn)
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		time (&utcRecordingPeriodEnd);
		gmtime_r(&utcRecordingPeriodEnd, &tmRecordingPeriodEnd);

		tmRecordingPeriodEnd.tm_year      = ulUTCYear - 1900;
		tmRecordingPeriodEnd.tm_mon       = ulUTCMonth - 1;
		tmRecordingPeriodEnd.tm_mday      = ulUTCDay;
		tmRecordingPeriodEnd.tm_hour      = ulUTCHour;                       
		tmRecordingPeriodEnd.tm_min       = ulUTCMinutes;
		tmRecordingPeriodEnd.tm_sec       = ulUTCSeconds;

		utcRecordingPeriodEnd = timegm(&tmRecordingPeriodEnd);
	}
	if (utcRecordingPeriodStart >= utcRecordingPeriodEnd)
	{
		Json::StreamWriterBuilder wbuilder;
		string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

		string errorMessage = __FILEREF__
			+ "RecordingPeriodStart cannot be bigger than RecordingPeriodEnd"
			+ ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
			+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);
        
		throw runtime_error(errorMessage);
	}

    field = "OutputFormat";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string liveRecorderOutputFormat = parametersRoot.get(field, "").asString();
		if (!isLiveRecorderOutputValid(liveRecorderOutputFormat))
		{
			string errorMessage = __FILEREF__ + field + " is wrong (it could be only "
                + "ts"
                + ")"
                + ", Field: " + field
                + ", liveRecorderOutputFormat: " + liveRecorderOutputFormat
                + ", label: " + label
                ;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}
}

void Validator::validateWorkflowAsLibraryMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    vector<string> mandatoryFields = {
        "WorkflowAsLibraryType",
        "WorkflowAsLibraryLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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

    string field = "WorkflowAsLibraryType";
    string workflowAsLibraryType = parametersRoot.get(field, "").asString();

    if (!isWorkflowAsLibraryTypeValid(workflowAsLibraryType))
    {
        string errorMessage = string("Unknown WorkflowAsLibraryType")
            + ", workflowAsLibraryType: " + workflowAsLibraryType
            + ", label: " + label
        ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

void Validator::validateChangeFileFormatMetadata(int64_t workspaceKey, string label,
	Json::Value parametersRoot, 
	bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    vector<string> mandatoryFields = {
        "OutputFileFormat"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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

	bool isVideoOrAudio = false;
	bool isImage = false;

    string field = "OutputFileFormat";
    string outputFileFormat = parametersRoot.get(field, "").asString();
    if (isVideoAudioFileFormat(outputFileFormat))
		isVideoOrAudio = true;
    else if (isImageFileFormat(outputFileFormat))
		isImage = true;
	else
    {
        string errorMessage = __FILEREF__ + field + " is wrong"
                + ", Field: " + field
                + ", outputFileFormat: " + outputFileFormat
                + ", label: " + label
                ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	field = "References";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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

				if (isImage)
				{
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
				else if (isVideoOrAudio)
				{
					if (referenceContentType != MMSEngineDBFacade::ContentType::Video && referenceContentType != MMSEngineDBFacade::ContentType::Audio)
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
}

void Validator::validateVideoSpeedMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
        
	/*
    vector<string> mandatoryFields = {
        "Speed"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
	*/

    string field = "SpeedType";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string speedType = parametersRoot.get(field, "").asString();
		if (!isVideoSpeedTypeValid(speedType))
		{
			string errorMessage = __FILEREF__ + field + " is wrong (it could be only "
                + "SlowDown, or SpeedUp"
                + ")"
                + ", Field: " + field
                + ", speed:Type " + speedType
                + ", label: " + label
                ;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

    field = "SpeedSize";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		int speedSize = JSONUtils::asInt(parametersRoot, field, 3);
		if (speedSize < 1 || speedSize > 10)
		{
			string errorMessage = __FILEREF__ + field + " is wrong (it could be between 1 and 10)"
                + ", Field: " + field
                + ", speedSize: " + to_string(speedSize)
                + ", label: " + label
                ;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	field = "References";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);

		if (validateDependenciesToo)
		{
            if (dependencies.size() != 1)
            {
                string errorMessage = __FILEREF__ + "Dependencies were not found"
                        + ", dependencies.size: " + to_string(dependencies.size())
                        + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            // for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
            tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>&
					keyAndDependencyType	=  dependencies[0];
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

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

void Validator::validatePictureInPictureMetadata(int64_t workspaceKey, string label,
	Json::Value parametersRoot, 
        bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "References";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        Json::Value referencesRoot = parametersRoot[field];
		// before the check was
		//	if (referencesRoot.size() != 2)
		// This was changed to > 2 because it could be used
		// the "DependOnIngestionJobKeysToBeAddedToReferences" thg
        if (referencesRoot.size() > 2)
        {
            string errorMessage = __FILEREF__ + "Field is present but it has more than two elements"
                    + ", Field: " + field
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
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

                if (referenceContentType_1 != MMSEngineDBFacade::ContentType::Video
                        || referenceContentType_2 != MMSEngineDBFacade::ContentType::Video)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer both a video content"
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

void Validator::validateLiveProxyMetadata(int64_t workspaceKey, string label,
	Json::Value parametersRoot,
	bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
        
	vector<string> mandatoryFields = {
		"ConfigurationLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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

    string field = "OutputType";
	string liveProxyOutputType;
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		liveProxyOutputType = parametersRoot.get(field, "").asString();
		if (!isLiveProxyOutputTypeValid(liveProxyOutputType))
		{
			string errorMessage = __FILEREF__ + field + " is wrong (it could be CDN77 or HLS or DASH)"
                + ", Field: " + field
                + ", liveProxyOutputType: " + liveProxyOutputType
                + ", label: " + label
                ;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

	if (liveProxyOutputType == "CDN77")
	{
		vector<string> mandatoryFields = {
			"CDN_URL"
		};
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
	}
}

void Validator::validateLiveGridMetadata(int64_t workspaceKey, string label,
	Json::Value parametersRoot, bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
        
	vector<string> mandatoryFields = {
		"InputConfigurationLabels",
		"Columns",
		"GridWidth",
		"GridHeigth"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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

    string encodingProfileKeyField = "EncodingProfileKey";
    string encodingProfileLabelField = "EncodingProfileLabel";
    if (!JSONUtils::isMetadataPresent(parametersRoot, encodingProfileLabelField)
		&& !JSONUtils::isMetadataPresent(parametersRoot, encodingProfileKeyField))
    {
        string errorMessage = __FILEREF__ + "Neither of the following fields are present"
			+ ", Field: " + encodingProfileLabelField
			+ ", Field: " + encodingProfileKeyField
			+ ", label: " + label
			;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string field = "InputConfigurationLabels";
    Json::Value inputConfigurationLabelsRoot = parametersRoot[field];
	if (inputConfigurationLabelsRoot.size() < 2)
	{
		string errorMessage = __FILEREF__ + field + " is wrong, it should contains at least 2 configuration labels"
			+ ", Field: " + field
			+ ", inputConfigurationLabelsRoot.size: " + to_string(inputConfigurationLabelsRoot.size())
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);
       
		throw runtime_error(errorMessage);
	}

    field = "Columns";
	int columns = JSONUtils::asInt(parametersRoot, field, 0);
	if (columns < 1)
	{
		string errorMessage = __FILEREF__ + field + " is wrong (it has to be major than 0)"
			+ ", Field: " + field
			+ ", columns: " + to_string(columns)
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);
       
		throw runtime_error(errorMessage);
	}

    field = "GridWidth";
	int gridWidth = JSONUtils::asInt(parametersRoot, field, 0);
	if (gridWidth < 1)
	{
		string errorMessage = __FILEREF__ + field + " is wrong (it has to be major than 0)"
			+ ", Field: " + field
			+ ", gridWidth: " + to_string(gridWidth)
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);
       
		throw runtime_error(errorMessage);
	}

    field = "GridHeigth";
	int gridHeigth = JSONUtils::asInt(parametersRoot, field, 0);
	if (gridHeigth < 1)
	{
		string errorMessage = __FILEREF__ + field + " is wrong (it has to be major than 0)"
			+ ", Field: " + field
			+ ", gridHeigth: " + to_string(gridHeigth)
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);
       
		throw runtime_error(errorMessage);
	}

    field = "OutputType";
	string liveGridOutputType;
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		liveGridOutputType = parametersRoot.get(field, "").asString();
		if (!isLiveGridOutputTypeValid(liveGridOutputType))
		{
			string errorMessage = __FILEREF__ + field + " is wrong (it could be CDN77 or HLS)"
                + ", Field: " + field
                + ", liveGridOutputType: " + liveGridOutputType
                + ", label: " + label
                ;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

	if (liveGridOutputType == "CDN77")
	{
		vector<string> mandatoryFields = {
			"CDN_URL"
		};
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
	}
	else if (liveGridOutputType == "HLS")
	{
		vector<string> mandatoryFields = {
			"OutputConfigurationLabel"
		};
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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
	}
}

void Validator::validateLiveCutMetadata(int64_t workspaceKey, string label,
    Json::Value parametersRoot, bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies)
{
    // see sample in directory samples

	vector<string> mandatoryFields = {
		"ConfigurationLabel",
		"CutPeriod"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
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

    string field = "CutPeriod";
	Json::Value cutPeriodRoot = parametersRoot[field];
    field = "Start";
	if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
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
	// next code is the same in the MMSEngineProcessor class
	time_t utcCutPeriodStart;
	{
		string cutPeriodStart = cutPeriodRoot.get(field, "").asString();

		unsigned long       ulUTCYear;
		unsigned long       ulUTCMonth;
		unsigned long       ulUTCDay;
		unsigned long       ulUTCHour;
		unsigned long       ulUTCMinutes;
		unsigned long       ulUTCSeconds;
		tm                  tmCutPeriodStart;
		int                 sscanfReturn;


		// _logger->error(__FILEREF__ + "recordingPeriodStart 1: " + recordingPeriodStart);
		// recordingPeriodStart.replace(10, 1, string(" "), 0, 1);
		// _logger->error(__FILEREF__ + "recordingPeriodStart 2: " + recordingPeriodStart);
		if ((sscanfReturn = sscanf (cutPeriodStart.c_str(),
			"%4lu-%2lu-%2luT%2lu:%2lu:%2luZ",
			&ulUTCYear,
			&ulUTCMonth,
			&ulUTCDay,
			&ulUTCHour,
			&ulUTCMinutes,
			&ulUTCSeconds)) != 6)
		{
			Json::StreamWriterBuilder wbuilder;
			string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

			string errorMessage = __FILEREF__ + "Field has a wrong format (sscanf failed)"
				+ ", Field: " + field
				+ ", sscanfReturn: " + to_string(sscanfReturn)
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		time (&utcCutPeriodStart);
		gmtime_r(&utcCutPeriodStart, &tmCutPeriodStart);

		tmCutPeriodStart.tm_year      = ulUTCYear - 1900;
		tmCutPeriodStart.tm_mon       = ulUTCMonth - 1;
		tmCutPeriodStart.tm_mday      = ulUTCDay;
		tmCutPeriodStart.tm_hour      = ulUTCHour;                       
		tmCutPeriodStart.tm_min       = ulUTCMinutes;
		tmCutPeriodStart.tm_sec       = ulUTCSeconds;

		utcCutPeriodStart = timegm(&tmCutPeriodStart);
	}

    field = "End";
	if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
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
	// next code is the same in the MMSEngineProcessor class
	time_t utcCutPeriodEnd;
	{
		string cutPeriodEnd = cutPeriodRoot.get(field, "").asString();

		unsigned long       ulUTCYear;
		unsigned long       ulUTCMonth;
		unsigned long       ulUTCDay;
		unsigned long       ulUTCHour;
		unsigned long       ulUTCMinutes;
		unsigned long       ulUTCSeconds;
		tm                  tmCutPeriodEnd;
		int                 sscanfReturn;


		// _logger->error(__FILEREF__ + "cutPeriodStart 1: " + cutPeriodStart);
		// recordingPeriodStart.replace(10, 1, string(" "), 0, 1);
		// _logger->error(__FILEREF__ + "cutPeriodStart 2: " + cutPeriodStart);
		if ((sscanfReturn = sscanf (cutPeriodEnd.c_str(),
			"%4lu-%2lu-%2luT%2lu:%2lu:%2luZ",
			&ulUTCYear,
			&ulUTCMonth,
			&ulUTCDay,
			&ulUTCHour,
			&ulUTCMinutes,
			&ulUTCSeconds)) != 6)
		{
			Json::StreamWriterBuilder wbuilder;
			string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

			string errorMessage = __FILEREF__ + "Field has a wrong format (sscanf failed)"
				+ ", Field: " + field
				+ ", sscanfReturn: " + to_string(sscanfReturn)
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		time (&utcCutPeriodEnd);
		gmtime_r(&utcCutPeriodEnd, &tmCutPeriodEnd);

		tmCutPeriodEnd.tm_year      = ulUTCYear - 1900;
		tmCutPeriodEnd.tm_mon       = ulUTCMonth - 1;
		tmCutPeriodEnd.tm_mday      = ulUTCDay;
		tmCutPeriodEnd.tm_hour      = ulUTCHour;                       
		tmCutPeriodEnd.tm_min       = ulUTCMinutes;
		tmCutPeriodEnd.tm_sec       = ulUTCSeconds;

		utcCutPeriodEnd = timegm(&tmCutPeriodEnd);
	}
	if (utcCutPeriodStart >= utcCutPeriodEnd)
	{
		Json::StreamWriterBuilder wbuilder;
		string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

		string errorMessage = __FILEREF__
			+ "CutPeriodStart cannot be bigger than CutPeriodEnd"
			+ ", utcCutPeriodStart: " + to_string(utcCutPeriodStart)
			+ ", utcCutPeriodEnd: " + to_string(utcCutPeriodEnd)
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);
        
		throw runtime_error(errorMessage);
	}
}

void Validator::fillDependencies(int64_t workspaceKey, string label, Json::Value parametersRoot, 
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies,
        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
        bool encodingProfileFieldsToBeManaged)
{
    string field = "References";
    Json::Value referencesRoot = parametersRoot[field];

    for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
    {
        Json::Value referenceRoot = referencesRoot[referenceIndex];

		bool errorIfContentNotFound = true;
        field = "ErrorIfContentNotFound";
        if (JSONUtils::isMetadataPresent(referenceRoot, field))
			errorIfContentNotFound = JSONUtils::asBool(referenceRoot, field, true);

        int64_t referenceMediaItemKey = -1;
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        bool referenceLabel = false;

        field = "ReferenceMediaItemKey";
        if (!JSONUtils::isMetadataPresent(referenceRoot, field))
        {
            field = "ReferencePhysicalPathKey";
            if (!JSONUtils::isMetadataPresent(referenceRoot, field))
            {
                field = "ReferenceIngestionJobKey";
                if (!JSONUtils::isMetadataPresent(referenceRoot, field))
                {
                    field = "ReferenceUniqueName";
                    if (!JSONUtils::isMetadataPresent(referenceRoot, field))
                    {
                        field = "ReferenceLabel";
                        if (!JSONUtils::isMetadataPresent(referenceRoot, field))
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
                        referenceUniqueName = referenceRoot.get(field, "").asString();
                    }        
                }
                else
                {
                    referenceIngestionJobKey = JSONUtils::asInt64(referenceRoot, field, 0);
                }
            }
            else
            {
                referencePhysicalPathKey = JSONUtils::asInt64(referenceRoot, field, 0);
            }
        }
        else
        {
            referenceMediaItemKey = JSONUtils::asInt64(referenceRoot, field, 0);    
        }

        MMSEngineDBFacade::ContentType	referenceContentType;
        try
        {
			_logger->debug(__FILEREF__ + "fillDependencies"
				+ ", label: " + label
				+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
				+ ", referencePhysicalPathKey: " + to_string(referencePhysicalPathKey)
				+ ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
				+ ", referenceUniqueName: " + referenceUniqueName
				+ ", referenceLabel: " + to_string(referenceLabel)
			);

            bool warningIfMissing = true;
            if (referenceMediaItemKey != -1)
            {
                tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
					contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey = 
					_mmsEngineDBFacade->getMediaItemKeyDetails(workspaceKey, referenceMediaItemKey,
					warningIfMissing); 
                tie(referenceContentType, ignore, ignore, ignore, ignore, ignore)
					= contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
            }
            else if (referencePhysicalPathKey != -1)
            {
                tuple<int64_t,MMSEngineDBFacade::ContentType,string,string,string,int64_t, string>
					mediaItemKeyDetails = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        workspaceKey, referencePhysicalPathKey, warningIfMissing);  

                tie(referenceMediaItemKey,referenceContentType, ignore, ignore, ignore, ignore,
					ignore) = mediaItemKeyDetails;
            }
            else if (referenceIngestionJobKey != -1)
            {
                // the difference with the other if is that here, associated to the ingestionJobKey,
                // we may have a list of mediaItems (i.e.: periodic-frame)
                vector<tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType>> mediaItemsDetails;

                _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                        workspaceKey, referenceIngestionJobKey, -1,
						mediaItemsDetails, warningIfMissing);  

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
						// scenario: user adds OnSuccess on the Encode Task. In this case the user wants
						// to apply the current task to the profile by the encode and not to the source media item.
						// So the generic rule s that, if referenceIngestionJobKey refers a Task generating
						// just one profile of a media item already present, so do not generates the media item,
						// (i.e.: Encode Task), it means the user asked implicitely to use
						// the generated profile and not the source media item
						bool isIngestionTaskGeneratingAProfile = false;
						{
							MMSEngineDBFacade::IngestionType ingestionType;

							tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string>
								labelIngestionTypeAndErrorMessage =
								_mmsEngineDBFacade->getIngestionJobDetails(referenceIngestionJobKey);
							tie(ignore, ingestionType, ignore, ignore, ignore) = labelIngestionTypeAndErrorMessage;

							if (ingestionType == MMSEngineDBFacade::IngestionType::Encode)
								isIngestionTaskGeneratingAProfile = true;
						}

                        if (priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey
							|| isIngestionTaskGeneratingAProfile)
                        {
                            tie(referenceMediaItemKey, referencePhysicalPathKey, referenceContentType) =
                                mediaItemKeyPhysicalPathKeyAndContentType;

                            if (referencePhysicalPathKey != -1)
							{
								_logger->debug(__FILEREF__ + "fillDependencies"
									+ ", label: " + label
									+ ", referencePhysicalPathKey: " + to_string(referencePhysicalPathKey)
									+ ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
									+ ", DependencyType::PhysicalPathKey"
								);

                                dependencies.push_back(make_tuple(referencePhysicalPathKey, referenceContentType, DependencyType::PhysicalPathKey));
							}
                            else if (referenceMediaItemKey != -1)
							{
								_logger->debug(__FILEREF__ + "fillDependencies"
									+ ", label: " + label
									+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
									+ ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
									+ ", DependencyType::MediaItemKey"
								);

                                dependencies.push_back(make_tuple(referenceMediaItemKey, referenceContentType, DependencyType::MediaItemKey));
							}
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
							{
								_logger->debug(__FILEREF__ + "fillDependencies"
									+ ", label: " + label
									+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
									+ ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
									+ ", DependencyType::MediaItemKey"
								);

                                dependencies.push_back(make_tuple(referenceMediaItemKey, referenceContentType, DependencyType::MediaItemKey));
							}
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
        catch(MediaItemKeyNotFound e)
        {
            string errorMessage = __FILEREF__ + "Reference... was not found"
				+ ", label: " + label
				+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
				+ ", referencePhysicalPathKey: " + to_string(referencePhysicalPathKey)
				+ ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
				+ ", referenceUniqueName: " + referenceUniqueName
				+ ", referenceLabel: " + to_string(referenceLabel)
                    ;
			if (errorIfContentNotFound)
			{
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			else
			{
				_logger->warn(errorMessage);

				continue;
			}
        }
        catch(runtime_error e)
        {
            string errorMessage = __FILEREF__ + "Reference... was not found"
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getMediaItemKeyDetails failed"
                    + ", workspaceKey,: " + to_string(workspaceKey)
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
                    if (JSONUtils::isMetadataPresent(referenceRoot, field))
                    {
                        int64_t encodingProfileKey = JSONUtils::asInt64(referenceRoot, field, 0);

						bool warningIfMissing = false;
                        referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
                                referenceMediaItemKey, encodingProfileKey, warningIfMissing);
                    }  
                    else
                    {
                        field = "EncodingProfileLabel";
                        if (JSONUtils::isMetadataPresent(referenceRoot, field))
                        {
                            string encodingProfileLabel = referenceRoot.get(field, "0").asString();

							bool warningIfMissing = false;
                            referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
									workspaceKey, referenceMediaItemKey, referenceContentType,
									encodingProfileLabel, warningIfMissing);
                        }        
                    }
                }
            }
            
            if (referencePhysicalPathKey != -1)
			{
				_logger->debug(__FILEREF__ + "fillDependencies"
					+ ", label: " + label
					+ ", referencePhysicalPathKey: " + to_string(referencePhysicalPathKey)
					+ ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
					+ ", DependencyType::PhysicalPathKey"
				);

                dependencies.push_back(make_tuple(referencePhysicalPathKey, referenceContentType, DependencyType::PhysicalPathKey));
			}
            else if (referenceMediaItemKey != -1)
			{
				_logger->debug(__FILEREF__ + "fillDependencies"
					+ ", label: " + label
					+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
					+ ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
					+ ", DependencyType::MediaItemKey"
				);
                dependencies.push_back(make_tuple(referenceMediaItemKey, referenceContentType, DependencyType::MediaItemKey));
			}
            else    // referenceLabel
                ;
        }
    }
}

void Validator::fillReferencesOutput(
	int64_t workspaceKey, Json::Value parametersRoot,
	vector<pair<int64_t, int64_t>>& referencesOutput)
{

    string field = "ReferencesOutput";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		Json::StreamWriterBuilder wbuilder;
		string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
		;
		_logger->warn(errorMessage);

		return;
	}
    Json::Value referencesOutputRoot = parametersRoot[field];

    for (int referenceIndex = 0; referenceIndex < referencesOutputRoot.size(); referenceIndex++)
    {
        Json::Value referenceOutputRoot = referencesOutputRoot[referenceIndex];

        int64_t referenceMediaItemKey = -1;
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";

        field = "ReferenceMediaItemKey";
        if (!JSONUtils::isMetadataPresent(referenceOutputRoot, field))
        {
            field = "ReferencePhysicalPathKey";
            if (!JSONUtils::isMetadataPresent(referenceOutputRoot, field))
            {
                field = "ReferenceIngestionJobKey";
                if (!JSONUtils::isMetadataPresent(referenceOutputRoot, field))
                {
                    field = "ReferenceUniqueName";
                    if (!JSONUtils::isMetadataPresent(referenceOutputRoot, field))
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
                        referenceUniqueName = referenceOutputRoot.get(field, "").asString();
                    }        
                }
                else
                {
                    referenceIngestionJobKey = JSONUtils::asInt64(referenceOutputRoot, field, 0);
                }
            }
            else
            {
                referencePhysicalPathKey = JSONUtils::asInt64(referenceOutputRoot, field, 0);
            }
        }
        else
        {
            referenceMediaItemKey = JSONUtils::asInt64(referenceOutputRoot, field, 0);    
        }

        try
        {
            bool warningIfMissing = true;
            if (referenceMediaItemKey != -1)
            {
				try
				{
					bool warningIfMissing = true;
					int64_t localPhysicalPathKey =
						_mmsEngineDBFacade->getPhysicalPathDetails(
						referenceMediaItemKey, -1, warningIfMissing);

					referencesOutput.push_back(make_pair(referenceMediaItemKey, localPhysicalPathKey));
				}
				catch(MediaItemKeyNotFound e)
				{
					_logger->warn(__FILEREF__
						+ "fillReferencesOutput. getMediaItemKeyDetailsByPhysicalPathKey failed"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
					);
				}
            }
			else if (referencePhysicalPathKey != -1)
            {
				try
				{
					bool warningIfMissing = true;
					tuple<int64_t,MMSEngineDBFacade::ContentType,string,string,string,int64_t, string>
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
						workspaceKey, referencePhysicalPathKey, warningIfMissing);  

					int64_t localMediaItemKey;
					tie(localMediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore) =
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;

					referencesOutput.push_back(make_pair(localMediaItemKey, referencePhysicalPathKey));
				}
				catch(MediaItemKeyNotFound e)
				{
					_logger->warn(__FILEREF__
						+ "fillReferencesOutput. getMediaItemKeyDetailsByPhysicalPathKey failed"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", referencePhysicalPathKey: " + to_string(referencePhysicalPathKey)
					);
				}
            }
            else if (referenceIngestionJobKey != -1)
            {
                // the difference with the other if is that here, associated to the ingestionJobKey,
                // we may have a list of mediaItems (i.e.: periodic-frame)
                vector<tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType>> mediaItemsDetails;

                _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
                        workspaceKey, referenceIngestionJobKey, -1,
						mediaItemsDetails, warningIfMissing);  

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
					int64_t localMediaItemKey;
					int64_t localPhysicalPathKey;
                    for (tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> 
                            mediaItemKeyPhysicalPathKeyAndContentType: mediaItemsDetails)
                    {
						tie(localMediaItemKey, localPhysicalPathKey, ignore)
							= mediaItemKeyPhysicalPathKeyAndContentType;

						referencesOutput.push_back(make_pair(localMediaItemKey, localPhysicalPathKey));
                    }
                }
            }
            else if (referenceUniqueName != "")
            {
				try
				{
					int64_t localMediaItemKey;
					int64_t localPhysicalPathKey;

					pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType = 
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
                        workspaceKey, referenceUniqueName, warningIfMissing);  

					tie(localMediaItemKey, ignore) = mediaItemKeyAndContentType;

					bool warningIfMissing = true;
					localPhysicalPathKey =
						_mmsEngineDBFacade->getPhysicalPathDetails(
						localMediaItemKey, -1, warningIfMissing);

					referencesOutput.push_back(make_pair(localMediaItemKey, localPhysicalPathKey));
				}
				catch(MediaItemKeyNotFound e)
				{
					_logger->warn(__FILEREF__
						+ "fillReferencesOutput. getMediaItemKeyDetailsByPhysicalPathKey failed"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", referenceUniqueName: " + referenceUniqueName
					);
				}
            }
        }
        catch(runtime_error e)
        {
			Json::StreamWriterBuilder wbuilder;
			string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

            string errorMessage = __FILEREF__ + "fillReferencesOutput failed"
                    + ", sParametersRoot: " + sParametersRoot
					+ ", e.what(): " + e.what()
                    ;
            _logger->error(errorMessage);

            throw e;
        }
        catch(exception e)
        {
			Json::StreamWriterBuilder wbuilder;
			string sParametersRoot = Json::writeString(wbuilder, parametersRoot);

            string errorMessage = __FILEREF__ + "fillReferencesOutput failed"
                    + ", sParametersRoot: " + sParametersRoot
					+ ", e.what(): " + e.what()
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
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
        "m3u8",
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
        "png",
        "tga"
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

bool Validator::isFaceRecognitionCascadeNameValid(string faceRecognitionCascadeName)
{
    vector<string> validCascadeNames = {
        "haarcascade_frontalface_alt",
        "haarcascade_frontalface_alt2",
        "haarcascade_frontalface_alt_tree",
        "haarcascade_frontalface_default"
    };

    for (string validCascadeName: validCascadeNames)
    {
        if (faceRecognitionCascadeName == validCascadeName) 
            return true;
    }
    
    return false;
}

bool Validator::isFaceRecognitionOutputValid(string faceRecognitionOutput)
{
    vector<string> validOutputs = {
        "VideoWithHighlightedFaces",
        "ImagesToBeUsedInDeepLearnedModel",
        "FrameContainingFace"
    };

    for (string validOutput: validOutputs)
    {
        if (faceRecognitionOutput == validOutput) 
            return true;
    }
    
    return false;
}

bool Validator::isLiveRecorderOutputValid(string liveRecorderOutputFormat)
{
    vector<string> outputFormats = {
        "ts"
    };

    for (string outputFormat: outputFormats)
    {
        if (liveRecorderOutputFormat == outputFormat) 
            return true;
    }
    
    return false;
}

bool Validator::isLiveProxyOutputTypeValid(string liveProxyOutputType)
{
    vector<string> outputTypes = {
        "CDN77",
        "HLS",
        "DASH"
    };

    for (string outputType: outputTypes)
    {
        if (liveProxyOutputType == outputType) 
            return true;
    }
    
    return false;
}

bool Validator::isLiveGridOutputTypeValid(string liveGridOutputType)
{
    vector<string> outputTypes = {
        "CDN77",
        "HLS"
    };

    for (string outputType: outputTypes)
    {
        if (liveGridOutputType == outputType) 
            return true;
    }
    
    return false;
}

bool Validator::isVideoSpeedTypeValid(string speedType)
{
    vector<string> suffixes = {
        "SlowDown",
        "SpeedUp"
    };

    string lowerCaseFileFormat;
    lowerCaseFileFormat.resize(speedType.size());
    transform(speedType.begin(), speedType.end(), lowerCaseFileFormat.begin(), [](unsigned char c){return tolower(c); } );
    for (string suffix: suffixes)
    {
        if (lowerCaseFileFormat == suffix) 
            return true;
    }
    
    return false;
}

bool Validator::isWorkflowAsLibraryTypeValid(string workflowAsLibraryType)
{
    vector<string> types = {
		"MMS",
		"User"
    };

    for (string type: types)
    {
        if (workflowAsLibraryType == type) 
            return true;
    }
    
    return false;
}

void Validator::validateCrossReference(
    string label, Json::Value crossReferenceRoot,
	bool mediaItemKeyMandatory)
{
	if (mediaItemKeyMandatory)
	{
		vector<string> crossReferenceMandatoryFields = {
			"Type",
			"MediaItemKey"
		};
		for (string mandatoryField: crossReferenceMandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(crossReferenceRoot, mandatoryField))
			{
				Json::StreamWriterBuilder wbuilder;
				string sCrossReferenceRoot = Json::writeString(wbuilder, crossReferenceRoot);
           
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
                   + ", Field: " + mandatoryField
                   + ", sCrossReferenceRoot: " + sCrossReferenceRoot
                   + ", label: " + label
                   ;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	else
	{
		vector<string> crossReferenceMandatoryFields = {
			"Type",
		};
		for (string mandatoryField: crossReferenceMandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(crossReferenceRoot, mandatoryField))
			{
				Json::StreamWriterBuilder wbuilder;
				string sCrossReferenceRoot = Json::writeString(wbuilder, crossReferenceRoot);
           
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
                   + ", Field: " + mandatoryField
                   + ", sCrossReferenceRoot: " + sCrossReferenceRoot
                   + ", label: " + label
                   ;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}

	string field = "Type";
	string sCrossReferenceType = crossReferenceRoot.get(field, "").asString();
	MMSEngineDBFacade::CrossReferenceType crossReferenceType;
	try
	{
		crossReferenceType = MMSEngineDBFacade::toCrossReferenceType(sCrossReferenceType);
	}
	catch(exception e)
	{
		Json::StreamWriterBuilder wbuilder;
		string sCrossReferenceRoot = Json::writeString(wbuilder, crossReferenceRoot);
           
		string errorMessage = __FILEREF__ + "Field 'CrossReferenceType' is wrong"
			+ ", CrossReferenceType: " + sCrossReferenceType
			+ ", label: " + label
			+ ", sCrossReferenceRoot: " + sCrossReferenceRoot
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::CutOfVideo
		|| crossReferenceType == MMSEngineDBFacade::CrossReferenceType::CutOfAudio)
	{
		field = "Parameters";
		if (!JSONUtils::isMetadataPresent(crossReferenceRoot, field))
		{
			Json::StreamWriterBuilder wbuilder;
			string sCrossReferenceRoot = Json::writeString(wbuilder, crossReferenceRoot);
           
			string errorMessage = __FILEREF__ + "Field 'CrossReference->Parameters' is missing"
				+ ", label: " + label
				+ ", sCrossReferenceRoot: " + sCrossReferenceRoot
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		Json::Value crossReferenceParameters = crossReferenceRoot[field];

		vector<string> crossReferenceCutMandatoryFields = {
			"StartTimeInSeconds",
			"EndTimeInSeconds"
		};
		for (string mandatoryField: crossReferenceCutMandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(crossReferenceParameters, mandatoryField))
			{
				Json::StreamWriterBuilder wbuilder;
				string sCrossReferenceRoot = Json::writeString(wbuilder, crossReferenceRoot);
           
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + mandatoryField
					+ ", sCrossReferenceRoot: " + sCrossReferenceRoot
					+ ", label: " + label
                   ;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
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
        if (!JSONUtils::isMetadataPresent(encodingProfilesSetRoot, mandatoryField))
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
    if (JSONUtils::isMetadataPresent(encodingProfilesSetRoot, field))
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
            if (!JSONUtils::isMetadataPresent(encodingProfileRoot, mandatoryField))
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
        string label = encodingProfileRoot.get(field, "").asString();
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
            if (!JSONUtils::isMetadataPresent(encodingProfileVideoRoot, mandatoryField))
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
            if (!JSONUtils::isMetadataPresent(encodingProfileAudioRoot, mandatoryField))
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
            if (!JSONUtils::isMetadataPresent(encodingProfileRoot, mandatoryField))
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
        string label = encodingProfileRoot.get(field, "").asString();
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
            if (!JSONUtils::isMetadataPresent(encodingProfileAudioRoot, mandatoryField))
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
            if (!JSONUtils::isMetadataPresent(encodingProfileRoot, mandatoryField))
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
        string label = encodingProfileRoot.get(field, "").asString();
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
            if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, mandatoryField))
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

