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
#include "catralibraries/DateTime.h"
#include "Validator.h"

Validator::Validator(
        shared_ptr<spdlog::logger> logger, 
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        json configuration
) 
{
    _logger             = logger;
    _mmsEngineDBFacade  = mmsEngineDBFacade;

    _storagePath = JSONUtils::asString(configuration["storage"], "path", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->path: " + _storagePath
    );
}

Validator::Validator(const Validator& orig) {
}

Validator::~Validator() {
}

void Validator::validateIngestedRootMetadata(int64_t workspaceKey, json root)
{
    string field = "type";
    if (!JSONUtils::isMetadataPresent(root, field))
    {
        string sRoot = JSONUtils::toString(root);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + 
                + ", sRoot: " + sRoot
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }    
    string type = JSONUtils::asString(root, field, "");
    if (type != "Workflow")
    {
        string errorMessage = __FILEREF__ + "Type field is wrong"
                + ", Type: " + type;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    field = "task";
    if (!JSONUtils::isMetadataPresent(root, field))
    {
        string sRoot = JSONUtils::toString(root);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sRoot: " + sRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }    
    json taskRoot = root[field];                        

    field = "type";
    if (!JSONUtils::isMetadataPresent(taskRoot, field))
    {
        string sRoot = JSONUtils::toString(root);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sRoot: " + sRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }    
    string taskType = JSONUtils::asString(taskRoot, field, "");

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
	json groupOfTasksRoot, bool validateDependenciesToo)
{
    string field = "parameters";
    if (!JSONUtils::isMetadataPresent(groupOfTasksRoot, field))
    {
        string sGroupOfTasksRoot = JSONUtils::toString(groupOfTasksRoot);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sGroupOfTasksRoot: " + sGroupOfTasksRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    json parametersRoot = groupOfTasksRoot[field];
    
	validateGroupOfTasksMetadata(workspaceKey, parametersRoot);

    field = "tasks";
    if (!JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string sParametersRoot = JSONUtils::toString(parametersRoot);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sParametersRoot: " + sParametersRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    json tasksRoot = parametersRoot[field];

	/* 2021-02-20: A group that does not have any Task couls be a scenario,
	 * so we do not have to raise an error. Same check commented in API_Ingestion.cpp
    if (tasksRoot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No Tasks are present inside the GroupOfTasks item";
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
	*/

    for (int taskIndex = 0; taskIndex < tasksRoot.size(); ++taskIndex)
    {
        json taskRoot = tasksRoot[taskIndex];
        
        field = "type";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
			string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", Field: " + field
                + ", sParametersRoot: " + sParametersRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = JSONUtils::asString(taskRoot, field, "");

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
	json parametersRoot)
{
    string field = "executionType";
    if (!JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string sParametersRoot = JSONUtils::toString(parametersRoot);
        
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sParametersRoot: " + sParametersRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string executionType = JSONUtils::asString(parametersRoot, field, "");
    if (executionType != "parallel" 
            && executionType != "sequential")
    {
        string errorMessage = __FILEREF__ + "executionType field is wrong"
                + ", executionType: " + executionType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
}

void Validator::validateEvents(int64_t workspaceKey, json taskOrGroupOfTasksRoot,
	bool validateDependenciesToo)
{
    string field = "onSuccess";
    if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        json onSuccessRoot = taskOrGroupOfTasksRoot[field];
        
        field = "task";
        if (!JSONUtils::isMetadataPresent(onSuccessRoot, field))
        {
            string sTaskOrGroupOfTasksRoot = JSONUtils::toString(taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        json taskRoot = onSuccessRoot[field];                        

        string field = "type";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskOrGroupOfTasksRoot = JSONUtils::toString(taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = JSONUtils::asString(taskRoot, field, "");

        if (taskType == "GroupOfTasks")
        {
            validateGroupOfTasksMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }
    }

    field = "onError";
    if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        json onErrorRoot = taskOrGroupOfTasksRoot[field];
        
        field = "task";
        if (!JSONUtils::isMetadataPresent(onErrorRoot, field))
        {
            string sTaskOrGroupOfTasksRoot = JSONUtils::toString(taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        json taskRoot = onErrorRoot[field];                        

        string field = "type";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskOrGroupOfTasksRoot = JSONUtils::toString(taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = JSONUtils::asString(taskRoot, field, "");

        if (taskType == "GroupOfTasks")
        {
            validateGroupOfTasksMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }
        else
        {
            validateSingleTaskMetadata(workspaceKey, taskRoot, validateDependenciesToo);
        }
    }    
    
    field = "onComplete";
    if (JSONUtils::isMetadataPresent(taskOrGroupOfTasksRoot, field))
    {
        json onCompleteRoot = taskOrGroupOfTasksRoot[field];
        
        field = "task";
        if (!JSONUtils::isMetadataPresent(onCompleteRoot, field))
        {
            string sTaskOrGroupOfTasksRoot = JSONUtils::toString(taskOrGroupOfTasksRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        json taskRoot = onCompleteRoot[field];                        

        string field = "type";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskOrGroupOfTasksRoot = JSONUtils::toString(taskOrGroupOfTasksRoot);

            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskOrGroupOfTasksRoot: " + sTaskOrGroupOfTasksRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }    
        string taskType = JSONUtils::asString(taskRoot, field, "");

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

vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
	Validator::validateSingleTaskMetadata(
	int64_t workspaceKey, json taskRoot, bool validateDependenciesToo)
{
    MMSEngineDBFacade::IngestionType    ingestionType;
    vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>
		dependencies;

    string field = "type";
    if (!JSONUtils::isMetadataPresent(taskRoot, field))
    {
        string sTaskRoot = JSONUtils::toString(taskRoot);

        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field
                + ", sTaskRoot: " + sTaskRoot;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string label;
    field = "label";
	label = JSONUtils::asString(taskRoot, field, "");

    string type = JSONUtils::asString(taskRoot, "type", "");
    if (type == "Add-Content")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::AddContent;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateAddContentMetadata(label, parametersRoot);
    }
    else if (type == "Add-Silent-Audio")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::AddSilentAudio;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateAddSilentAudioMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Remove-Content")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::RemoveContent;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateRemoveContentMetadata(workspaceKey, label, parametersRoot,
			dependencies);
    }
    else if (type == "Encode")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Encode;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateEncodeMetadata(workspaceKey, label, parametersRoot,
			dependencies);
    }
    else if (type == "Frame")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Frame;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateFrameMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);        
    }
    else if (type == "Periodical-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::PeriodicalFrames;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validatePeriodicalFramesMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Motion-JPEG-by-Periodical-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validatePeriodicalFramesMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "I-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::IFrames;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateIFramesMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Motion-JPEG-by-I-Frames")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateIFramesMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Slideshow")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Slideshow;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateSlideshowMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Concat-Demuxer")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::ConcatDemuxer;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateConcatDemuxerMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Cut")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Cut;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateCutMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Overlay-Image-On-Video")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::OverlayImageOnVideo;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateOverlayImageOnVideoMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);        
    }
    else if (type == "Overlay-Text-On-Video")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::OverlayTextOnVideo;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateOverlayTextOnVideoMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);        
    }
    else if (type == "Email-Notification")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::EmailNotification;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateEmailNotificationMetadata(workspaceKey, label, parametersRoot,
				validateDependenciesToo, dependencies);
    }
    else if (type == "Check-Streaming")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::CheckStreaming;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateCheckStreamingMetadata(workspaceKey, label, parametersRoot);
    }
    else if (type == "Media-Cross-Reference")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::MediaCrossReference;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateMediaCrossReferenceMetadata(workspaceKey, label, parametersRoot,
				validateDependenciesToo, dependencies);
    }
    else if (type == "FTP-Delivery")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::FTPDelivery;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateFTPDeliveryMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "HTTP-Callback")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::HTTPCallback;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateHTTPCallbackMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);
    }
    else if (type == "Local-Copy")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LocalCopy;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateLocalCopyMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Extract-Tracks")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::ExtractTracks;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateExtractTracksMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Post-On-Facebook")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::PostOnFacebook;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validatePostOnFacebookMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Post-On-YouTube")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::PostOnYouTube;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validatePostOnYouTubeMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Face-Recognition")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::FaceRecognition;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateFaceRecognitionMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Face-Identification")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::FaceIdentification;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateFaceIdentificationMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);
    }
    else if (type == "Live-Recorder")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LiveRecorder;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateLiveRecorderMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Change-File-Format")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::ChangeFileFormat;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateChangeFileFormatMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Video-Speed")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::VideoSpeed;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateVideoSpeedMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Picture-In-Picture")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::VideoSpeed;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validatePictureInPictureMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Intro-Outro-Overlay")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::IntroOutroOverlay;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateIntroOutroOverlayMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);
    }
    else if (type == "Live-Proxy")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LiveProxy;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateLiveProxyMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "YouTube-Live-Broadcast")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::YouTubeLiveBroadcast;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateYouTubeLiveBroadcastMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);
    }
    else if (type == "Facebook-Live-Broadcast")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::FacebookLiveBroadcast;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateFacebookLiveBroadcastMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);
    }
    else if (type == "VOD-Proxy")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::VODProxy;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateVODProxyMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Countdown")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::Countdown;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateCountdownMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);
    }
    else if (type == "Live-Grid")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LiveGrid;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateLiveGridMetadata(workspaceKey, label, parametersRoot,
			validateDependenciesToo, dependencies);
    }
    else if (type == "Live-Cut")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::LiveCut;
        
        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateLiveCutMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
    }
    else if (type == "Workflow-As-Library")
    {
        ingestionType = MMSEngineDBFacade::IngestionType::WorkflowAsLibrary;

        field = "parameters";
        if (!JSONUtils::isMetadataPresent(taskRoot, field))
        {
            string sTaskRoot = JSONUtils::toString(taskRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTaskRoot: " + sTaskRoot;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        json parametersRoot = taskRoot[field]; 
        validateWorkflowAsLibraryMetadata(workspaceKey, label, parametersRoot, validateDependenciesToo,
			dependencies);
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

vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>
	Validator::validateSingleTaskMetadata(int64_t workspaceKey,
	MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot)
{
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>
		dependencies;

    // we can validate dependencies too because this method is called by the processor
    // when the dependencies would be already generated
    bool validateDependenciesToo = true;

    string label;
    
    if (ingestionType == MMSEngineDBFacade::IngestionType::AddContent)
    {
        validateAddContentMetadata(label, parametersRoot);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::AddSilentAudio)
    {
        validateAddSilentAudioMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::RemoveContent)
    {
        validateRemoveContentMetadata(workspaceKey, label, parametersRoot,
			dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Encode)
    {
        validateEncodeMetadata(workspaceKey, label, parametersRoot,
			dependencies);
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
	else if (ingestionType == MMSEngineDBFacade::IngestionType::CheckStreaming)
	{
		validateCheckStreamingMetadata(workspaceKey, label, parametersRoot);
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
    else if (ingestionType == MMSEngineDBFacade::IngestionType::IntroOutroOverlay)
    {
        validateIntroOutroOverlayMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::LiveProxy)
    {
        validateLiveProxyMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::YouTubeLiveBroadcast)
    {
        validateYouTubeLiveBroadcastMetadata(workspaceKey, label, parametersRoot, 
			validateDependenciesToo, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::FacebookLiveBroadcast)
    {
        validateFacebookLiveBroadcastMetadata(workspaceKey, label, parametersRoot, 
			validateDependenciesToo, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::VODProxy)
    {
        validateVODProxyMetadata(workspaceKey, label, parametersRoot, 
                validateDependenciesToo, dependencies);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Countdown)
    {
        validateCountdownMetadata(workspaceKey, label, parametersRoot, 
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
    string label, json parametersRoot)
{
    vector<string> mandatoryFields = {
        // "sourceURL",     it is optional in case of push
        "fileFormat"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    string field = "fileFormat";
    string fileFormat = JSONUtils::asString(parametersRoot, field, "");

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
        field = "sourceURL";
        if (JSONUtils::isMetadataPresent(parametersRoot, field))
        {
			string sourceURL = JSONUtils::asString(parametersRoot, field, "");

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

    field = "crossReference";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		json crossReferenceRoot = parametersRoot[field];

		// in AddContent MediaItemKey has to be present
		bool mediaItemKeyMandatory = true;
		validateCrossReference(label, crossReferenceRoot, mediaItemKeyMandatory);
	}

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}

    /*
    // Territories
    {
        field = "Territories";
        if (JSONUtils::isMetadataPresent(parametersRoot, field))
        {
            const json territories = parametersRoot[field];
            
            for( Json::ValueIterator itr = territories.begin() ; itr != territories.end() ; itr++ ) 
            {
                json territory = territories[territoryIndex];
            }
        }
        
    }
    */            
}

void Validator::validateAddSilentAudioMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{    

	string field = "addType";
	string addType = JSONUtils::asString(parametersRoot, field, "entireTrack");
	if (!isAddSilentTypeValid(addType))
	{
		string errorMessage = string("Unknown addType")
			+ ", addType: " + addType
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}

	if (addType == "begin" || addType == "end")
	{
		string mandatoryField = "seconds";
		if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);

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
    field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() < 1)
        {
            string errorMessage = __FILEREF__ + "Field is present but it does not have enough elements"
                    + ", Field: " + field
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
    }

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateRemoveContentMetadata(int64_t workspaceKey, string label,
    json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies
	)
{
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() < 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
		*/

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
    }

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateEncodeMetadata(int64_t workspaceKey, string label,
	json parametersRoot,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{
	string field = "encodingPriority";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string encodingPriority = JSONUtils::asString(parametersRoot, field, "");
        try
        {
			// it generate an exception in case of wrong string
            MMSEngineDBFacade::toEncodingPriority(encodingPriority);
        }
        catch(exception& e)
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
    string encodingProfileKeyField = "encodingProfileKey";
    string encodingProfileLabelField = "encodingProfileLabel";
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
    field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		// 2021-08-26: removed the check because we are adding the option to manage
		// several contents
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
		*/

        // json referenceRoot = referencesRoot[0];

        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = true;
        fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);
    }

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateFrameMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples
     
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References (1)"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    + ", dependencies.size: " + to_string(dependencies.size())
                    + ", parametersRoot: " + JSONUtils::toString(parametersRoot)
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
            if (dependencies.size() == 1)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= dependencies[0];

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validatePeriodicalFramesMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies
	)
{
    vector<string> mandatoryFields = {
        // "SourceFileName",
        "PeriodInSeconds"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
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
            if (dependencies.size() == 1)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= dependencies[0];

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateIFramesMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples
        
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
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
            if (dependencies.size() == 1)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= dependencies[0];

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateSlideshowMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{    
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() < 1)
        {
            string errorMessage = __FILEREF__ + "Field is present but it does not have enough elements"
                    + ", Field: " + field
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
			int picturesNumber = 0;
			int audiosNumber = 0;

            for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>&
				keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= keyAndDependencyType;

				if (referenceContentType == MMSEngineDBFacade::ContentType::Image)
					picturesNumber++;
				else if (referenceContentType == MMSEngineDBFacade::ContentType::Audio)
					audiosNumber++;
				else
				{
					string errorMessage = __FILEREF__ + "Reference... does not refer an image-audio content"
						+ ", dependencyType: " + to_string(static_cast<int>(dependencyType))
						+ ", referenceMediaItemKey: " + to_string(key)
						+ ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
						+ ", label: " + label
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
            }

			if (picturesNumber == 0)
			{
				string errorMessage = __FILEREF__ + "Reference does not refer an image content"
					+ ", picturesNumber: " + to_string(picturesNumber)
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
        }
    }

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateConcatDemuxerMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples
            
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
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
            for (tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>&
				keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= keyAndDependencyType;

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateCutMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples
        
    string field = "startTime";
    if (!JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string sParametersRoot = JSONUtils::toString(parametersRoot);
                        
        string errorMessage = __FILEREF__ + "field is not present or it is null"
                + ", Field: " + field
                + ", sParametersRoot: " + sParametersRoot
                + ", label: " + label
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string endTimeField = "endTime";
    string framesNumberField = "framesNumber";
    if (!JSONUtils::isMetadataPresent(parametersRoot, endTimeField)
            && !JSONUtils::isMetadataPresent(parametersRoot, framesNumberField))
    {
        string errorMessage = __FILEREF__ + "Both fields are not present or it is null"
                + ", Field: " + endTimeField
                + ", Field: " + framesNumberField
                + ", label: " + label
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    field = "cutType";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		string cutType = JSONUtils::asString(parametersRoot, field, "");

        if (!isCutTypeValid(cutType))
        {
            string errorMessage = string("Unknown cutType")
                + ", cutType: " + cutType
                + ", label: " + label
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }

		if (cutType == "KeyFrameSeekingInterval")
		{
			field = "startKeyFrameSeekingInterval";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string sParametersRoot = JSONUtils::toString(parametersRoot);
                        
				string errorMessage = __FILEREF__ + "field is not present or it is null"
						+ ", Field: " + field
						+ ", sParametersRoot: " + sParametersRoot
						+ ", label: " + label
						;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			field = "endKeyFrameSeekingInterval";
			if (!JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				string sParametersRoot = JSONUtils::toString(parametersRoot);
                        
				string errorMessage = __FILEREF__ + "field is not present or it is null"
						+ ", Field: " + field
						+ ", sParametersRoot: " + sParametersRoot
						+ ", label: " + label
						;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
    }
	
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
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
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= dependencies[0];

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateOverlayImageOnVideoMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{
    
    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        json referencesRoot = parametersRoot[field];
		// before the check was
		//	if (referencesRoot.size() != 2)
		// This was changed to > 2 because it could be used
		// the "DependOnIngestionJobKeysToBeAddedToReferences" thg
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        if (referencesRoot.size() > 2)
        {
            string errorMessage = __FILEREF__ + "Field is present but it has more than two elements"
                    + ", Field: " + field
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
            if (dependencies.size() == 2)
            {
                int64_t key_1;
                MMSEngineDBFacade::ContentType referenceContentType_1;
                Validator::DependencyType dependencyType_1;
				bool stopIfReferenceProcessingError_1;

                tie(key_1, referenceContentType_1, dependencyType_1, stopIfReferenceProcessingError_1)
					= dependencies[0];

                int64_t key_2;
                MMSEngineDBFacade::ContentType referenceContentType_2;
                Validator::DependencyType dependencyType_2;
				bool stopIfReferenceProcessingError_2;

                tie(key_2, referenceContentType_2, dependencyType_2, stopIfReferenceProcessingError_2)
					= dependencies[1];

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateOverlayTextOnVideoMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
		"drawTextDetails"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

	{
		json drawTextDetailsRoot = parametersRoot["drawTextDetails"];

		vector<string> mandatoryFields = {
			"text"
		};
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(drawTextDetailsRoot, mandatoryField))
			{
				string sParametersRoot = JSONUtils::toString(drawTextDetailsRoot);
            
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string field = "fontType";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			string fontType = JSONUtils::asString(drawTextDetailsRoot, field, "");

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

		field = "fontColor";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			string fontColor = JSONUtils::asString(drawTextDetailsRoot, field, "");

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

		field = "textPercentageOpacity";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			int textPercentageOpacity = JSONUtils::asInt(drawTextDetailsRoot, field, 200);

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

		field = "boxEnable";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			bool boxEnable = JSONUtils::asBool(drawTextDetailsRoot, field, true);                        
		}

		field = "boxPercentageOpacity";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			int boxPercentageOpacity = JSONUtils::asInt(drawTextDetailsRoot, field, 200);

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
	}

    string field = "encodingPriority";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string encodingPriority = JSONUtils::asString(parametersRoot, field, "");
        try
        {
            MMSEngineDBFacade::toEncodingPriority(encodingPriority);    // it generate an exception in case of wrong string
        }
        catch(exception& e)
        {
            string errorMessage = __FILEREF__ + "Field 'EncodingPriority' is wrong"
                    + ", EncodingPriority: " + encodingPriority
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "boxColor";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string boxColor = JSONUtils::asString(parametersRoot, field, "");
                        
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
    field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
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
            if (dependencies.size() == 1)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= dependencies[0];

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateEmailNotificationMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "configurationLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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
	string field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
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
                json referenceRoot = referencesRoot[referenceIndex];

                int64_t referenceIngestionJobKey = -1;
                bool referenceLabel = false;

                field = "ingestionJobKey";
                if (!JSONUtils::isMetadataPresent(referenceRoot, field))
                {
                    field = "label";
                    if (!JSONUtils::isMetadataPresent(referenceRoot, field))
                    {
                        string sParametersRoot = JSONUtils::toString(parametersRoot);

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateCheckStreamingMetadata(int64_t workspaceKey, string label,
    json parametersRoot)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "inputType"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
	string field = "inputType";
	string inputType = JSONUtils::asString(parametersRoot, field, "");

	if (inputType == "Stream")
	{
		vector<string> mandatoryFields = {
			"configurationLabel"
		};
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
			{
				string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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
	else if (inputType == "URL")
	{
		vector<string> mandatoryFields = {
			"streamingName",
			"streamingUrl"
		};
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
			{
				string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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
	else
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "inputType Field is wrong, it is neither Stream nor URL"
			+ ", inputType: " + inputType
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
    
    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateMediaCrossReferenceMetadata(int64_t workspaceKey, string label,
	json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies
)
{
    // see sample in directory samples

	// in MediaCrossReference, MediaItemKey may not be present because inherit from parent Task
	bool mediaItemKeyMandatory = false;
	validateCrossReference(label, parametersRoot, mediaItemKeyMandatory);

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	string field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
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
		*/

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateFTPDeliveryMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "configurationLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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
	string field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
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

		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
		bool encodingProfileFieldsToBeManaged = false;
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);
		if (validateDependenciesToo)
		{
        } 
    }

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateHTTPCallbackMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "hostName",
        "uri"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
    string field = "method";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string method = JSONUtils::asString(parametersRoot, field, "");
                        
        if (method != "GET" && method != "POST" && method != "PUT")
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
        json headersRoot = parametersRoot[field];
        
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
	field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
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

		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
		bool encodingProfileFieldsToBeManaged = false;
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);
		if (validateDependenciesToo)
		{
        } 
    }

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateLocalCopyMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples
        
    vector<string> mandatoryFields = {
        "LocalPath"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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
    string localPath = JSONUtils::asString(parametersRoot, field, "");
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
	field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
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

		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
		bool encodingProfileFieldsToBeManaged = false;
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);
		if (validateDependenciesToo)
		{
        }  
    }

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateExtractTracksMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    vector<string> mandatoryFields = {
        "Tracks",
        "outputFileFormat"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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
    json tracksRoot = parametersRoot[field];
    if (tracksRoot.size() == 0)
    {
        string errorMessage = __FILEREF__ + "No correct number of Tracks"
                + ", tracksRoot.size: " + to_string(tracksRoot.size())
                    + ", label: " + label
                ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    for (int trackIndex = 0; trackIndex < tracksRoot.size(); trackIndex++)
    {
        json trackRoot = tracksRoot[trackIndex];
        
        field = "TrackType";
        if (!JSONUtils::isMetadataPresent(trackRoot, field))
        {
            string sTrackRoot = JSONUtils::toString(trackRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field
                    + ", sTrackRoot: " + sTrackRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string trackType = JSONUtils::asString(trackRoot, field, "");
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

    field = "outputFileFormat";
    string outputFileFormat = JSONUtils::asString(parametersRoot, field, "");
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
	field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
		if (referencesRoot.size() == 0)
		{
			string errorMessage = __FILEREF__ + "No References"
				+ ", referencesRoot.size: " + to_string(referencesRoot.size())
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
            for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>&
				keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= keyAndDependencyType;

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validatePostOnFacebookMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    vector<string> mandatoryFields = {
		"facebookNodeType",	// Page, User, Event or Group
        "facebookNodeId",
        "facebookConfigurationLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

	if (!isFacebookNodeTypeValid(JSONUtils::asString(parametersRoot, "facebookNodeType")))
	{
		string errorMessage = __FILEREF__ + "FacebookNodeType is not valid"
			+ ", parametersRoot: " + JSONUtils::toString(parametersRoot);
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	string field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
		if (referencesRoot.size() < 1)
		{
			string errorMessage = __FILEREF__ + "No correct number of References"
				+ ", referencesRoot.size: " + to_string(referencesRoot.size())
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		*/

		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
		bool encodingProfileFieldsToBeManaged = false;
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);

		if (validateDependenciesToo)
		{
            for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>&
				keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= keyAndDependencyType;

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validatePostOnYouTubeMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    vector<string> mandatoryFields = {
        "configurationLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "privacyStatus";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string youTubePrivacyStatus = JSONUtils::asString(parametersRoot, field, "");

        if (!isYouTubePrivacyStatusValid(youTubePrivacyStatus))
        {
            string errorMessage = __FILEREF__ + field
				+ " is wrong (it could be only 'private', 'public' or unlisted"
				+ ", Field: " + field
				+ ", youTubePrivacyStatus: " + youTubePrivacyStatus
				+ ", label: " + label
			;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }


	// References is optional because in case of dependency managed automatically
	// by MMS (i.e.: onSuccess)
	field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
		if (referencesRoot.size() < 1)
		{
			string errorMessage = __FILEREF__ + "No correct number of References"
				+ ", referencesRoot.size: " + to_string(referencesRoot.size())
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		*/

		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
		bool encodingProfileFieldsToBeManaged = false;
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);

		if (validateDependenciesToo)
		{
            for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>&
				keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= keyAndDependencyType;

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateFaceRecognitionMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
        
    vector<string> mandatoryFields = {
        "cascadeName",
        "output"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "cascadeName";
    string faceRecognitionCascadeName = JSONUtils::asString(parametersRoot, field, "");
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

    field = "output";
    string faceRecognitionOutput = JSONUtils::asString(parametersRoot, field, "");
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
	field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
		if (referencesRoot.size() != 1)
		{
			string errorMessage = __FILEREF__ + "No correct number of References"
				+ ", referencesRoot.size: " + to_string(referencesRoot.size())
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
            tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>&
					keyAndDependencyType	=  dependencies[0];
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= keyAndDependencyType;

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateFaceIdentificationMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
        
    vector<string> mandatoryFields = {
        "cascadeName",
        "deepLearnedModelTags"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "cascadeName";
    string faceIdentificationCascadeName = JSONUtils::asString(parametersRoot, field, "");
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

    field = "deepLearnedModelTags";
    if (parametersRoot[field].type() != json::value_t::array
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
	field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
		if (referencesRoot.size() != 1)
		{
			string errorMessage = __FILEREF__ + "No correct number of References"
				+ ", referencesRoot.size: " + to_string(referencesRoot.size())
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
            tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>&
				keyAndDependencyType	=  dependencies[0];
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= keyAndDependencyType;

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateLiveRecorderMetadata(int64_t workspaceKey, string label,
	json parametersRoot,
	bool validateDependenciesToo, vector<tuple<int64_t,MMSEngineDBFacade::ContentType,
		Validator::DependencyType, bool>>& dependencies)
{

	{
		vector<string> mandatoryFields = {
			"configurationLabel",
			"recordingCode",
			"schedule",
			"segmentDuration"
		};
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
			{
				string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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

    string field = "segmentDuration";
	int segmentDuration = JSONUtils::asInt(parametersRoot, field, 1);
	if (segmentDuration % 2 != 0 || segmentDuration < 10)
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field has a wrong value (it is not even or slower than 10)"
			+ ", Field: " + field
			+ ", value: " + to_string(segmentDuration)
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

    field = "uniqueName";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field cannot be present in this Task"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

    field = "schedule";
	json recordingPeriodRoot = parametersRoot[field];
    field = "start";
	if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);
            
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	// next code is the same in the MMSEngineProcessor class
	string recordingPeriodStart = JSONUtils::asString(recordingPeriodRoot, field, "");
	time_t utcRecordingPeriodStart = DateTime::sDateSecondsToUtc(recordingPeriodStart);

    field = "end";
	if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);
            
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	// next code is the same in the MMSEngineProcessor class
	string recordingPeriodEnd = JSONUtils::asString(recordingPeriodRoot, field, "");
	time_t utcRecordingPeriodEnd = DateTime::sDateSecondsToUtc(recordingPeriodEnd);

	if (utcRecordingPeriodStart >= utcRecordingPeriodEnd)
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

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
		string liveRecorderOutputFormat = JSONUtils::asString(parametersRoot, field, "");
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

	field = "outputs";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		json outputsRoot;
		if (JSONUtils::isMetadataPresent(parametersRoot, "outputs"))
			outputsRoot = parametersRoot["outputs"];
		else // if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs", false))
			outputsRoot = parametersRoot["Outputs"];


		for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
		{
			json outputRoot = outputsRoot[outputIndex];

			validateOutputRootMetadata(workspaceKey, label, outputRoot, false);
		}
	}

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateLiveProxyMetadata(int64_t workspaceKey, string label,
	json parametersRoot,
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{
        
	string field = "configurationLabel";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);
          
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	bool timePeriod = false;
	field = "timePeriod";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
		timePeriod = JSONUtils::asBool(parametersRoot, field, false);

    field = "schedule";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		if (timePeriod)
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);
         
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", Field: " + field
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
				;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else
	{
		json proxyPeriodRoot = parametersRoot[field];

		time_t utcProxyPeriodStart = -1;
		time_t utcProxyPeriodEnd = -1;

		field = "start";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);
		}

		field = "end";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
		}

		if (utcProxyPeriodStart != -1 && utcProxyPeriodEnd != -1
			&& utcProxyPeriodStart >= utcProxyPeriodEnd)
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);

			string errorMessage = __FILEREF__
				+ "ProxyPeriodStart cannot be bigger than ProxyPeriodEnd"
				+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
				+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

	field = "outputs";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	json outputsRoot;
	if (JSONUtils::isMetadataPresent(parametersRoot, "outputs"))
		outputsRoot = parametersRoot["outputs"];
	else // if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs", false))
		outputsRoot = parametersRoot["Outputs"];

	if (outputsRoot.size() == 0)
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
	{
		json outputRoot = outputsRoot[outputIndex];

		validateOutputRootMetadata(workspaceKey, label, outputRoot, false);
	}

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateYouTubeLiveBroadcastMetadata(int64_t workspaceKey, string label,
	json parametersRoot,
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{

	string sourceType;
    string field = "SourceType";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		sourceType = JSONUtils::asString(parametersRoot, field, "");

        if (!isYouTubeLiveBroadcastSourceTypeValid(sourceType))
        {
            string errorMessage = string("Unknown sourceType")
                + ", sourceType: " + sourceType
                + ", label: " + label
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "PrivacyStatus";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string youTubePrivacyStatus = JSONUtils::asString(parametersRoot, field, "");

        if (!isYouTubePrivacyStatusValid(youTubePrivacyStatus))
        {
            string errorMessage = __FILEREF__ + field
				+ " is wrong (it could be only 'private', 'public' or unlisted"
				+ ", Field: " + field
				+ ", youTubePrivacyStatus: " + youTubePrivacyStatus
				+ ", label: " + label
			;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

	if (sourceType == "Live")
	{
		field = "configurationLabel";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);
          
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", Field: " + field
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else // if (sourceType == "MediaItem")
	{
		// References is optional because in case of dependency managed automatically
		// by MMS (i.e.: onSuccess)
		field = "references";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
			json referencesRoot = parametersRoot[field];
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
				if (dependencies.size() == 0)
				{
					string errorMessage = __FILEREF__ + "No correct number of Media to be broadcast"
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
					bool stopIfReferenceProcessingError;

					for(tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>&
						dependency: dependencies)
					{
						tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
							= dependency;

						if (referenceContentType != MMSEngineDBFacade::ContentType::Video
							&& referenceContentType != MMSEngineDBFacade::ContentType::Audio)
						{
							string errorMessage = __FILEREF__
								+ "Reference... does not refer a video-audio content"
								+ ", dependencyType: " + to_string(static_cast<int>(dependencyType))
								+ ", referenceMediaItemKey: " + to_string(key)
								+ ", referenceContentType: "
									+ MMSEngineDBFacade::toString(referenceContentType)
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

    field = "youTubeSchedule";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);
        
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	{
		json proxyPeriodRoot = parametersRoot[field];

		time_t utcProxyPeriodStart = -1;
		time_t utcProxyPeriodEnd = -1;

		field = "start";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);
		}

		field = "end";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
		}

		if (utcProxyPeriodStart != -1 && utcProxyPeriodEnd != -1
			&& utcProxyPeriodStart >= utcProxyPeriodEnd)
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);

			string errorMessage = __FILEREF__
				+ "ProxyPeriodStart cannot be bigger than ProxyPeriodEnd"
				+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
				+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateFacebookLiveBroadcastMetadata(int64_t workspaceKey, string label,
	json parametersRoot,
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{

    vector<string> mandatoryFields = {
		"facebookLiveType",	// LiveNow or LiveScheduled
		"facebookNodeType",	// Page, User, Event or Group
        "facebookNodeId"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
	if (!isFacebookLiveTypeValid(JSONUtils::asString(parametersRoot, "facebookLiveType")))
	{
		string errorMessage = __FILEREF__ + "FacebookLiveType is not valid"
			+ ", parametersRoot: " + JSONUtils::toString(parametersRoot);
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	if (!isFacebookNodeTypeValid(JSONUtils::asString(parametersRoot, "facebookNodeType")))
	{
		string errorMessage = __FILEREF__ + "facebookNodeType is not valid"
			+ ", parametersRoot: " + JSONUtils::toString(parametersRoot);
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}


	string sourceType;
    string field = "sourceType";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		sourceType = JSONUtils::asString(parametersRoot, field, "");

        if (!isFacebookLiveBroadcastSourceTypeValid(sourceType))
        {
            string errorMessage = string("Unknown sourceType")
                + ", sourceType: " + sourceType
                + ", label: " + label
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }

	if (sourceType == "Live")
	{
		field = "configurationLabel";
		if (!JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);
          
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", Field: " + field
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else // if (sourceType == "MediaItem")
	{
		// References is optional because in case of dependency managed automatically
		// by MMS (i.e.: onSuccess)
		field = "references";
		if (JSONUtils::isMetadataPresent(parametersRoot, field))
		{
			/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
			json referencesRoot = parametersRoot[field];
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
				if (dependencies.size() == 0)
				{
					string errorMessage = __FILEREF__ + "No correct number of Media to be broadcast"
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
					bool stopIfReferenceProcessingError;

					for(tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>&
						dependency: dependencies)
					{
						tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
							= dependency;

						if (referenceContentType != MMSEngineDBFacade::ContentType::Video
							&& referenceContentType != MMSEngineDBFacade::ContentType::Audio)
						{
							string errorMessage = __FILEREF__
								+ "reference... does not refer a video-audio content"
								+ ", dependencyType: " + to_string(static_cast<int>(dependencyType))
								+ ", referenceMediaItemKey: " + to_string(key)
								+ ", referenceContentType: "
									+ MMSEngineDBFacade::toString(referenceContentType)
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

    field = "facebookSchedule";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);
        
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	{
		json proxyPeriodRoot = parametersRoot[field];

		time_t utcProxyPeriodStart = -1;
		time_t utcProxyPeriodEnd = -1;

		field = "start";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);
		}

		field = "end";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
		}

		if (utcProxyPeriodStart != -1 && utcProxyPeriodEnd != -1
			&& utcProxyPeriodStart >= utcProxyPeriodEnd)
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);

			string errorMessage = __FILEREF__
				+ "ProxyPeriodStart cannot be bigger than ProxyPeriodEnd"
				+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
				+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateVODProxyMetadata(int64_t workspaceKey, string label,
	json parametersRoot,
	bool validateDependenciesToo,
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies)
{

	MMSEngineDBFacade::ContentType referenceContentType = MMSEngineDBFacade::ContentType::Video;

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
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

            {
                int64_t key;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= dependencies[0];
            }
        }
    }


	bool timePeriod = false;
	field = "timePeriod";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
		timePeriod = JSONUtils::asBool(parametersRoot, field, false);

    field = "schedule";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		if (timePeriod)
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);
         
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
				+ ", Field: " + field
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
				;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else
	{
		json proxyPeriodRoot = parametersRoot[field];

		time_t utcProxyPeriodStart = -1;
		time_t utcProxyPeriodEnd = -1;

		field = "start";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);
		}

		field = "end";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
		}

		if (utcProxyPeriodStart != -1 && utcProxyPeriodEnd != -1
			&& utcProxyPeriodStart >= utcProxyPeriodEnd)
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);

			string errorMessage = __FILEREF__
				+ "ProxyPeriodStart cannot be bigger than ProxyPeriodEnd"
				+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
				+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

	field = "outputs";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	json outputsRoot;
	if (JSONUtils::isMetadataPresent(parametersRoot, "outputs"))
		outputsRoot = parametersRoot["outputs"];
	else // if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs", false))
		outputsRoot = parametersRoot["Outputs"];

	if (outputsRoot.size() == 0)
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
	{
		json outputRoot = outputsRoot[outputIndex];

		// check that, in case of an Image, the encoding profile is mandatory
		if (referenceContentType == MMSEngineDBFacade::ContentType::Image)
		{
			string keyField = "encodingProfileKey";
			string labelField = "encodingProfileLabel";
			if (!JSONUtils::isMetadataPresent(outputRoot, keyField)
				&& !JSONUtils::isMetadataPresent(outputRoot, labelField))
			{
				string sParametersRoot = JSONUtils::toString(outputRoot);
       
				string errorMessage = __FILEREF__
					+ "In case of Image, the EncodingProfile is mandatory"
					+ ", sParametersRoot: " + sParametersRoot
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		validateOutputRootMetadata(workspaceKey, label, outputRoot, false);
	}

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateCountdownMetadata(int64_t workspaceKey, string label,
	json parametersRoot,
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No correct number of References"
                    + ", referencesRoot.size: " + to_string(referencesRoot.size())
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
		*/

		bool stopIfReferenceProcessingError;
        bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = false;
        bool encodingProfileFieldsToBeManaged = false;
        fillDependencies(workspaceKey, label, parametersRoot,
				dependencies,
                priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
                encodingProfileFieldsToBeManaged);
        if (validateDependenciesToo)
        {
            if (dependencies.size() == 1)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= dependencies[0];

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

    field = "schedule";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);
        
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
			;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	{
		json proxyPeriodRoot = parametersRoot[field];

		time_t utcProxyPeriodStart = -1;
		time_t utcProxyPeriodEnd = -1;

		field = "start";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodStart = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodStart = DateTime::sDateSecondsToUtc(proxyPeriodStart);
		}

		field = "end";
		if (JSONUtils::isMetadataPresent(proxyPeriodRoot, field))
		{
			string proxyPeriodEnd = JSONUtils::asString(proxyPeriodRoot, field, "");
			utcProxyPeriodEnd = DateTime::sDateSecondsToUtc(proxyPeriodEnd);
		}

		if (utcProxyPeriodStart != -1 && utcProxyPeriodEnd != -1
			&& utcProxyPeriodStart >= utcProxyPeriodEnd)
		{
			string sParametersRoot = JSONUtils::toString(parametersRoot);

			string errorMessage = __FILEREF__
				+ "ProxyPeriodStart cannot be bigger than ProxyPeriodEnd"
				+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
				+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
				+ ", sParametersRoot: " + sParametersRoot
				+ ", label: " + label
			;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

	field = "outputs";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	json outputsRoot;
	if (JSONUtils::isMetadataPresent(parametersRoot, "outputs"))
		outputsRoot = parametersRoot["outputs"];
	else // if (JSONUtils::isMetadataPresent(parametersRoot, "Outputs", false))
		outputsRoot = parametersRoot["Outputs"];

	if (outputsRoot.size() == 0)
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
	{
		json outputRoot = outputsRoot[outputIndex];

		validateOutputRootMetadata(workspaceKey, label, outputRoot, false);
	}

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateWorkflowAsLibraryMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    vector<string> mandatoryFields = {
        "workflowAsLibraryType",
        "workflowAsLibraryLabel"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "workflowAsLibraryType";
    string workflowAsLibraryType = JSONUtils::asString(parametersRoot, field, "");

    if (!isWorkflowAsLibraryTypeValid(workflowAsLibraryType))
    {
        string errorMessage = string("Unknown workflowAsLibraryType")
            + ", workflowAsLibraryType: " + workflowAsLibraryType
            + ", label: " + label
        ;
        _logger->error(__FILEREF__ + errorMessage);
        
        throw runtime_error(errorMessage);
    }

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateChangeFileFormatMetadata(int64_t workspaceKey, string label,
	json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    vector<string> mandatoryFields = {
        "outputFileFormat"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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

    string field = "outputFileFormat";
    string outputFileFormat = JSONUtils::asString(parametersRoot, field, "");
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
	field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
		if (referencesRoot.size() < 1)
		{
			string errorMessage = __FILEREF__ + "No correct number of References"
				+ ", referencesRoot.size: " + to_string(referencesRoot.size())
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		*/

		bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey = true;
		bool encodingProfileFieldsToBeManaged = false;
		fillDependencies(workspaceKey, label, parametersRoot, dependencies,
			priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
			encodingProfileFieldsToBeManaged);

		if (validateDependenciesToo)
		{
			for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>&
				keyAndDependencyType: dependencies)
			{
				int64_t key;
				MMSEngineDBFacade::ContentType referenceContentType;
				Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

				tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= keyAndDependencyType;

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateVideoSpeedMetadata(int64_t workspaceKey, string label,
    json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
        
	/*
    vector<string> mandatoryFields = {
        "Speed"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
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

    string field = "speedType";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string speedType = JSONUtils::asString(parametersRoot, field, "");
		if (!isVideoSpeedTypeValid(speedType))
		{
			string errorMessage = __FILEREF__ + field + " is wrong (it could be only "
                + "SlowDown, or SpeedUp"
                + ")"
                + ", Field: " + field
                + ", speedType " + speedType
                + ", label: " + label
                ;
			_logger->error(__FILEREF__ + errorMessage);
        
			throw runtime_error(errorMessage);
		}
	}

    field = "speedSize";
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
	field = "references";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		json referencesRoot = parametersRoot[field];
		if (referencesRoot.size() != 1)
		{
			string errorMessage = __FILEREF__ + "No correct number of References"
				+ ", referencesRoot.size: " + to_string(referencesRoot.size())
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
            tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>&
					keyAndDependencyType	=  dependencies[0];
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;
				bool stopIfReferenceProcessingError;

                tie(key, referenceContentType, dependencyType, stopIfReferenceProcessingError)
					= keyAndDependencyType;

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validatePictureInPictureMetadata(int64_t workspaceKey, string label,
	json parametersRoot, 
	bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
        json referencesRoot = parametersRoot[field];
		// before the check was
		//	if (referencesRoot.size() != 2)
		// This was changed to > 2 because it could be used
		// the "DependOnIngestionJobKeysToBeAddedToReferences" tag. It means now may be we have
		// 1 reference and DependOnIngestionJobKeysToBeAddedToReferences will add more
		// references when the task will be executed
        if (referencesRoot.size() > 2)
        {
            string errorMessage = __FILEREF__ + "Field is present but it has more than two elements"
                    + ", Field: " + field
                    + ", label: " + label
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
            if (dependencies.size() == 2)
            {
                int64_t key_1;
                MMSEngineDBFacade::ContentType referenceContentType_1;
                Validator::DependencyType dependencyType_1;
				bool stopIfReferenceProcessingError_1;

                tie(key_1, referenceContentType_1, dependencyType_1, stopIfReferenceProcessingError_1)
					= dependencies[0];

                int64_t key_2;
                MMSEngineDBFacade::ContentType referenceContentType_2;
                Validator::DependencyType dependencyType_2;
				bool stopIfReferenceProcessingError_2;

                tie(key_2, referenceContentType_2, dependencyType_2, stopIfReferenceProcessingError_2)
					= dependencies[1];

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateIntroOutroOverlayMetadata(int64_t workspaceKey, string label,
	json parametersRoot, bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{

    // References is optional because in case of dependency managed automatically
    // by MMS (i.e.: onSuccess)
    string field = "references";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
		/* 2022-12-20: referencesRoot era composto da 2 ReferenceIngestionJobKey
				Il primo non aveva media items as output
				Il secondo aveva un solo media item as output
				Per cui doveva essere validato ma, il controllo sotto (referencesRoot.size() != 1),
				non validava questo Task.
				Quindi la conclusione è che non bisogna fare il controllo in base al referencesRoot.size
				ma in base ai media items effettivi. Per questo motivo, il controllo l'ho commentato
		// input: 3 videos: intro, outro and main video
		// before the check was
		//	if (referencesRoot.size() != 3)
		// This was changed to > 3 because it could be used
		// the "DependOnIngestionJobKeysToBeAddedToReferences" tag. It means now may be we have
		// 1 reference and DependOnIngestionJobKeysToBeAddedToReferences will add more
		// references when the task will be executed
        json referencesRoot = parametersRoot[field];
        if (referencesRoot.size() > 3)
        {
            string errorMessage = __FILEREF__ + "Field is present but it is not the right number of elements"
                    + ", Field: " + field
                    + ", label: " + label
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
            if (dependencies.size() != 3)
            {
                string errorMessage = __FILEREF__ + "Wrong dependencies number"
                        + ", dependencies.size: " + to_string(dependencies.size())
                        + ", label: " + label
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            // if (dependencies.size() == 3)
            {
                int64_t key_1;
                MMSEngineDBFacade::ContentType referenceContentType_1;
                Validator::DependencyType dependencyType_1;
				bool stopIfReferenceProcessingError_1;

                tie(key_1, referenceContentType_1, dependencyType_1, stopIfReferenceProcessingError_1)
					= dependencies[0];

                int64_t key_2;
                MMSEngineDBFacade::ContentType referenceContentType_2;
                Validator::DependencyType dependencyType_2;
				bool stopIfReferenceProcessingError_2;

                tie(key_2, referenceContentType_2, dependencyType_2, stopIfReferenceProcessingError_2)
					= dependencies[1];

                int64_t key_3;
                MMSEngineDBFacade::ContentType referenceContentType_3;
                Validator::DependencyType dependencyType_3;
				bool stopIfReferenceProcessingError_3;

                tie(key_3, referenceContentType_3, dependencyType_3, stopIfReferenceProcessingError_3)
					= dependencies[2];

                if (referenceContentType_1 != MMSEngineDBFacade::ContentType::Video
					|| referenceContentType_2 != MMSEngineDBFacade::ContentType::Video
					|| referenceContentType_3 != MMSEngineDBFacade::ContentType::Video
				)
                {
                    string errorMessage = __FILEREF__ + "Reference... does not refer all a video content"
						+ ", dependencyType_1: " + to_string(static_cast<int>(dependencyType_1))
						+ ", referenceMediaItemKey_1: " + to_string(key_1)
						+ ", referenceContentType_1: " + MMSEngineDBFacade::toString(referenceContentType_1)
						+ ", dependencyType_2: " + to_string(static_cast<int>(dependencyType_2))
						+ ", referenceMediaItemKey_2: " + to_string(key_2)
						+ ", referenceContentType_2: " + MMSEngineDBFacade::toString(referenceContentType_2)
						+ ", dependencyType_3: " + to_string(static_cast<int>(dependencyType_3))
						+ ", referenceMediaItemKey_3: " + to_string(key_3)
						+ ", referenceContentType_3: " + MMSEngineDBFacade::toString(referenceContentType_3)
						+ ", label: " + label
					;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
    }

	string keyField = "encodingProfileKey";
	string labelField = "encodingProfileLabel";
	if (!JSONUtils::isMetadataPresent(parametersRoot, keyField)
		&& !JSONUtils::isMetadataPresent(parametersRoot, labelField))
	{
		string errorMessage = __FILEREF__ + "Both fields are not present or it is null"
			+ ", Field: " + keyField
			+ ", Field: " + labelField
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	vector<string> mandatoryFields = {
		"introOverlayDurationInSeconds",
		"outroOverlayDurationInSeconds"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    field = "introOverlayDurationInSeconds";
	int introOverlayDurationInSeconds = JSONUtils::asInt(parametersRoot, field, 0);
	if (introOverlayDurationInSeconds <= 0)
	{
		string errorMessage = __FILEREF__ + field + " is wrong (it has to be major than 0)"
			+ ", Field: " + field
			+ ", introOverlayDurationInSeconds: " + to_string(introOverlayDurationInSeconds)
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);
       
		throw runtime_error(errorMessage);
	}

    field = "outroOverlayDurationInSeconds";
	int outroOverlayDurationInSeconds = JSONUtils::asInt(parametersRoot, field, 0);
	if (outroOverlayDurationInSeconds <= 0)
	{
		string errorMessage = __FILEREF__ + field + " is wrong (it has to be major than 0)"
			+ ", Field: " + field
			+ ", outroOverlayDurationInSeconds: " + to_string(outroOverlayDurationInSeconds)
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);
       
		throw runtime_error(errorMessage);
	}
}

void Validator::validateLiveGridMetadata(int64_t workspaceKey, string label,
	json parametersRoot, bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
        
	vector<string> mandatoryFields = {
		"inputConfigurationLabels",
		"columns",
		"gridWidth",
		"gridHeight"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "inputConfigurationLabels";
    json inputConfigurationLabelsRoot = parametersRoot[field];
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

    field = "columns";
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

    field = "gridWidth";
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

    field = "gridHeight";
	int gridHeight = JSONUtils::asInt(parametersRoot, field, 0);
	if (gridHeight < 1)
	{
		string errorMessage = __FILEREF__ + field + " is wrong (it has to be major than 0)"
			+ ", Field: " + field
			+ ", gridHeight: " + to_string(gridHeight)
			+ ", label: " + label
		;
		_logger->error(__FILEREF__ + errorMessage);
       
		throw runtime_error(errorMessage);
	}

	field = "outputs";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	json outputsRoot;
	if (JSONUtils::isMetadataPresent(parametersRoot, "outputs"))
		outputsRoot = parametersRoot["outputs"];

	if (outputsRoot.size() == 0)
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
	{
		json outputRoot = outputsRoot[outputIndex];

		validateOutputRootMetadata(workspaceKey, label, outputRoot, true);
	}

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::validateLiveCutMetadata(int64_t workspaceKey, string label,
    json parametersRoot, bool validateDependenciesToo,
	vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
		dependencies)
{
    // see sample in directory samples

	vector<string> mandatoryFields = {
		"cutPeriod"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(parametersRoot, mandatoryField))
        {
            string sParametersRoot = JSONUtils::toString(parametersRoot);
            
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField
                    + ", sParametersRoot: " + sParametersRoot
                    + ", label: " + label
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

	string field = "recordingCode";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

    field = "cutPeriod";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);
            
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	json cutPeriodRoot = parametersRoot[field];
    field = "start";
	if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);
            
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	// next code is the same in the MMSEngineProcessor class
	string cutPeriodStart = JSONUtils::asString(cutPeriodRoot, field, "");
	int64_t utcCutPeriodStart = DateTime::sDateMilliSecondsToUtc(cutPeriodStart);

    field = "end";
	if (!JSONUtils::isMetadataPresent(cutPeriodRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);
            
		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
			+ ", label: " + label
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	// next code is the same in the MMSEngineProcessor class
	string cutPeriodEnd = JSONUtils::asString(cutPeriodRoot, field, "");
	int64_t utcCutPeriodEnd = DateTime::sDateMilliSecondsToUtc(cutPeriodEnd);

	if (utcCutPeriodStart >= utcCutPeriodEnd)
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

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

    field = "processingStartingFrom";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string processingStartingFrom = JSONUtils::asString(parametersRoot, field, "");
		// scenario:
		//	- this is an optional date field
		//	- it is associated to a variable having "" as default value
		//	- the variable is not passed
		//	The result is that the field remain empty.
		//	Since it is optional we do not need to raise any error
		//		(DateTime::sDateSecondsToUtc would generate  'sscanf failed')
		if (processingStartingFrom != "")
			DateTime::sDateSecondsToUtc(processingStartingFrom);
	}
}

void Validator::fillDependencies(int64_t workspaceKey, string label, json parametersRoot, 
	vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>>&
		dependencies,
	bool priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey,
	bool encodingProfileFieldsToBeManaged)
{
    string field = "references";
    json referencesRoot = parametersRoot[field];

    for (int referenceIndex = 0; referenceIndex < referencesRoot.size(); referenceIndex++)
    {
        json referenceRoot = referencesRoot[referenceIndex];

		field = "stopIfReferenceProcessingError";
		bool stopIfReferenceProcessingError = JSONUtils::asBool(referenceRoot, field, false);

        int64_t referenceMediaItemKey = -1;
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";
        bool referenceLabel = false;

        field = "mediaItemKey";
        if (!JSONUtils::isMetadataPresent(referenceRoot, field))
        {
            field = "physicalPathKey";
            if (!JSONUtils::isMetadataPresent(referenceRoot, field))
            {
                field = "ingestionJobKey";
                if (!JSONUtils::isMetadataPresent(referenceRoot, field))
                {
                    field = "uniqueName";
                    if (!JSONUtils::isMetadataPresent(referenceRoot, field))
                    {
                        field = "label";
                        if (!JSONUtils::isMetadataPresent(referenceRoot, field))
                        {
                            string sParametersRoot = JSONUtils::toString(parametersRoot);

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
                        referenceUniqueName = JSONUtils::asString(referenceRoot, field, "");
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
					warningIfMissing,
					// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
					true);
                tie(referenceContentType, ignore, ignore, ignore, ignore, ignore)
					= contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;

				string fieldEncodingProfileKey = "encodingProfileKey";
				string fieldEncodingProfileLabel = "encodingProfileLabel";
				if (JSONUtils::isMetadataPresent(referenceRoot, fieldEncodingProfileKey))
				{
					int64_t referenceEncodingProfileKey = JSONUtils::asInt64(referenceRoot, fieldEncodingProfileKey, 0);    

					referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
						referenceMediaItemKey, referenceEncodingProfileKey, warningIfMissing,
						// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
						true);
				}
				else if (JSONUtils::isMetadataPresent(referenceRoot, fieldEncodingProfileLabel))
				{
					string referenceEncodingProfileLabel = JSONUtils::asString(referenceRoot,
						fieldEncodingProfileLabel, "");

					referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
						workspaceKey, referenceMediaItemKey, referenceContentType,
						referenceEncodingProfileLabel, warningIfMissing,
						// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
						true);
				}
            }
            else if (referencePhysicalPathKey != -1)
            {
                tuple<int64_t,MMSEngineDBFacade::ContentType,string,string,string,int64_t,
					string, string, int64_t> mediaItemKeyDetails = 
					_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
						workspaceKey, referencePhysicalPathKey, warningIfMissing,
						// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
						true);

                tie(referenceMediaItemKey, referenceContentType, ignore, ignore, ignore, ignore,
					ignore, ignore, ignore) = mediaItemKeyDetails;
            }
            else if (referenceIngestionJobKey != -1)
            {
                // the difference with the other if is that here, associated to the ingestionJobKey,
                // we may have a list of mediaItems (i.e.: periodic-frame)
                vector<tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType>> mediaItemsDetails;

                _mmsEngineDBFacade->getMediaItemDetailsByIngestionJobKey(
					workspaceKey, referenceIngestionJobKey, -1,
					mediaItemsDetails, warningIfMissing,
					// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
					true);

                if (mediaItemsDetails.size() == 0)
                {
                    string sParametersRoot = JSONUtils::toString(parametersRoot);

                    string errorMessage = __FILEREF__ + "No media items found"
                            + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                            + ", sParametersRoot: " + sParametersRoot
                            ;
                    _logger->warn(errorMessage);
                }
                else
                {
					// mediaItemsDetails contains all the mediaItemKey-PhysicalPathKey
					//	generated by the ingestionJobKey
                    for (tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> 
                            mediaItemKeyPhysicalPathKeyAndContentType: mediaItemsDetails)
                    {
						// scenario: user adds OnSuccess on the Encode Task. In this case the user wants
						// to apply the current task to the profile by the encode and not to the source media item.
						// So the generic rule is that, if referenceIngestionJobKey refers a Task generating
						// just one profile of a media item already present, so do not generates the media item,
						// (i.e.: Encode Task), it means the user asked implicitely to use
						// the generated profile and not the source media item
						//
						bool isIngestionTaskGeneratingAProfile = false;
						{
							MMSEngineDBFacade::IngestionType ingestionType;

							tuple<string, MMSEngineDBFacade::IngestionType,
								MMSEngineDBFacade::IngestionStatus, string, string>
								labelIngestionTypeAndErrorMessage =
								_mmsEngineDBFacade->getIngestionJobDetails(
									workspaceKey, referenceIngestionJobKey,
									// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
									true);
							tie(ignore, ingestionType, ignore, ignore, ignore) =
								labelIngestionTypeAndErrorMessage;

							if (ingestionType == MMSEngineDBFacade::IngestionType::Encode)
								isIngestionTaskGeneratingAProfile = true;
						}

                        if (priorityOnPhysicalPathKeyInCaseOfReferenceIngestionJobKey
							|| isIngestionTaskGeneratingAProfile)
                        {
                            tie(referenceMediaItemKey, referencePhysicalPathKey,
								referenceContentType) =
                                mediaItemKeyPhysicalPathKeyAndContentType;

                            if (referencePhysicalPathKey != -1)
							{
								_logger->debug(__FILEREF__ + "fillDependencies"
									+ ", label: " + label
									+ ", referencePhysicalPathKey: " + to_string(referencePhysicalPathKey)
									+ ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
									+ ", DependencyType::PhysicalPathKey"
								);

                                dependencies.push_back(make_tuple(referencePhysicalPathKey,
									referenceContentType, DependencyType::PhysicalPathKey,
									stopIfReferenceProcessingError));
							}
                            else if (referenceMediaItemKey != -1)
							{
								_logger->debug(__FILEREF__ + "fillDependencies"
									+ ", label: " + label
									+ ", referenceMediaItemKey: "
										+ to_string(referenceMediaItemKey)
									+ ", referenceContentType: "
										+ MMSEngineDBFacade::toString(referenceContentType)
									+ ", DependencyType::MediaItemKey"
								);

                                dependencies.push_back(make_tuple(referenceMediaItemKey,
									referenceContentType, DependencyType::MediaItemKey,
									stopIfReferenceProcessingError));
							}
                            else    // referenceLabel
                                ;
                        }
                        else
                        {
                            int64_t localReferencePhysicalPathKey;

                            tie(referenceMediaItemKey, localReferencePhysicalPathKey,
								referenceContentType) =
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

                                dependencies.push_back(make_tuple(referenceMediaItemKey,
									referenceContentType, DependencyType::MediaItemKey,
									stopIfReferenceProcessingError));
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
						workspaceKey, referenceUniqueName, warningIfMissing,
						// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
						true);

                referenceMediaItemKey = mediaItemKeyAndContentType.first;
                referenceContentType = mediaItemKeyAndContentType.second;

				string fieldEncodingProfileKey = "encodingProfileKey";
				string fieldEncodingProfileLabel = "encodingProfileLabel";
				if (JSONUtils::isMetadataPresent(referenceRoot, fieldEncodingProfileKey))
				{
					int64_t referenceEncodingProfileKey = JSONUtils::asInt64(referenceRoot,
						fieldEncodingProfileKey, 0);    

					referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
						referenceMediaItemKey, referenceEncodingProfileKey, warningIfMissing,
						// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
						true);
				}
				else if (JSONUtils::isMetadataPresent(referenceRoot, fieldEncodingProfileLabel))
				{
					string referenceEncodingProfileLabel = JSONUtils::asString(referenceRoot,
						fieldEncodingProfileLabel, "");

					referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
						workspaceKey, referenceMediaItemKey, referenceContentType,
						referenceEncodingProfileLabel, warningIfMissing,
						// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
						true);
				}
            }
            else // referenceLabel
            {
            }
        }
        catch(MediaItemKeyNotFound& e)
        {
            string errorMessage = __FILEREF__ + "fillDependencies failed (MediaItemKeyNotFound)"
				+ ", workspaceKey,: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
				+ ", referencePhysicalPathKey: " + to_string(referencePhysicalPathKey)
				+ ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
				+ ", referenceUniqueName: " + referenceUniqueName
				+ ", referenceLabel: " + to_string(referenceLabel)
				+ ", e.what: " + e.what()
			;
			if (stopIfReferenceProcessingError)
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
        catch(runtime_error& e)
        {
            string errorMessage = __FILEREF__ + "fillDependencies failed (runtime_error)"
				+ ", workspaceKey,: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
				+ ", referencePhysicalPathKey: " + to_string(referencePhysicalPathKey)
				+ ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
				+ ", referenceUniqueName: " + referenceUniqueName
				+ ", referenceLabel: " + to_string(referenceLabel)
				+ ", e.what: " + e.what()
			;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception& e)
        {
            string errorMessage = __FILEREF__ + "fillDependencies failed (exception)"
				+ ", workspaceKey,: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
				+ ", referencePhysicalPathKey: " + to_string(referencePhysicalPathKey)
				+ ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
				+ ", referenceUniqueName: " + referenceUniqueName
				+ ", referenceLabel: " + to_string(referenceLabel)
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

                    field = "encodingProfileKey";
                    if (JSONUtils::isMetadataPresent(referenceRoot, field))
                    {
                        int64_t encodingProfileKey = JSONUtils::asInt64(referenceRoot, field, 0);

						bool warningIfMissing = false;
                        referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
							referenceMediaItemKey, encodingProfileKey, warningIfMissing,
							// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
							true);
                    }  
                    else
                    {
                        field = "encodingProfileLabel";
                        if (JSONUtils::isMetadataPresent(referenceRoot, field))
                        {
                            string encodingProfileLabel = JSONUtils::asString(referenceRoot, field, "0");

							bool warningIfMissing = false;
                            referencePhysicalPathKey = _mmsEngineDBFacade->getPhysicalPathDetails(
								workspaceKey, referenceMediaItemKey, referenceContentType,
								encodingProfileLabel, warningIfMissing,
								// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
								true);
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

                dependencies.push_back(make_tuple(referencePhysicalPathKey, referenceContentType,
					DependencyType::PhysicalPathKey, stopIfReferenceProcessingError));
			}
            else if (referenceMediaItemKey != -1)
			{
				_logger->debug(__FILEREF__ + "fillDependencies"
					+ ", label: " + label
					+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
					+ ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
					+ ", DependencyType::MediaItemKey"
				);
                dependencies.push_back(make_tuple(referenceMediaItemKey, referenceContentType,
					DependencyType::MediaItemKey, stopIfReferenceProcessingError));
			}
            else    // referenceLabel
                ;
        }
	}

	_logger->info(__FILEREF__ + "fillDependencies"
		+ ", label: " + label
		+ ", references: " + JSONUtils::toString(referencesRoot)
		+ ", dependencies.size: " + to_string(dependencies.size())
	);
}

void Validator::fillReferencesOutput(
	int64_t workspaceKey, json parametersRoot,
	vector<pair<int64_t, int64_t>>& referencesOutput)
{

    string field = "referencesOutput";
	if (!JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string sParametersRoot = JSONUtils::toString(parametersRoot);

		string errorMessage = __FILEREF__ + "Field is not present or it is null"
			+ ", Field: " + field
			+ ", sParametersRoot: " + sParametersRoot
		;
		_logger->warn(errorMessage);

		return;
	}
    json referencesOutputRoot = parametersRoot[field];

    for (int referenceIndex = 0; referenceIndex < referencesOutputRoot.size(); referenceIndex++)
    {
        json referenceOutputRoot = referencesOutputRoot[referenceIndex];

        int64_t referenceMediaItemKey = -1;
        int64_t referencePhysicalPathKey = -1;
        int64_t referenceIngestionJobKey = -1;
        string referenceUniqueName = "";

        field = "mediaItemKey";
        if (!JSONUtils::isMetadataPresent(referenceOutputRoot, field))
        {
            field = "physicalPathKey";
            if (!JSONUtils::isMetadataPresent(referenceOutputRoot, field))
            {
                field = "ingestionJobKey";
                if (!JSONUtils::isMetadataPresent(referenceOutputRoot, field))
                {
                    field = "uniqueName";
                    if (!JSONUtils::isMetadataPresent(referenceOutputRoot, field))
                    {
						string sParametersRoot = JSONUtils::toString(parametersRoot);

						string errorMessage = __FILEREF__ + "Field is not present or it is null"
							+ ", Field: " + "Reference..."
							+ ", sParametersRoot: " + sParametersRoot
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
                    else
                    {
                        referenceUniqueName = JSONUtils::asString(referenceOutputRoot, field, "");
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
						referenceMediaItemKey, -1, warningIfMissing,
						// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
						true);

					referencesOutput.push_back(make_pair(referenceMediaItemKey, localPhysicalPathKey));
				}
				catch(MediaItemKeyNotFound& e)
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
					tuple<int64_t,MMSEngineDBFacade::ContentType,string,string,string,int64_t,
						string, string, int64_t>
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
						_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
							workspaceKey, referencePhysicalPathKey, warningIfMissing,
							// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
							true);

					int64_t localMediaItemKey;
					tie(localMediaItemKey, ignore, ignore, ignore, ignore, ignore, ignore, ignore, ignore) =
						mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;

					referencesOutput.push_back(make_pair(localMediaItemKey, referencePhysicalPathKey));
				}
				catch(MediaItemKeyNotFound& e)
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
					mediaItemsDetails, warningIfMissing,
					// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
					true);

                if (mediaItemsDetails.size() == 0)
                {
                    string sParametersRoot = JSONUtils::toString(parametersRoot);

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
                        workspaceKey, referenceUniqueName, warningIfMissing,
						// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
						true);

					tie(localMediaItemKey, ignore) = mediaItemKeyAndContentType;

					bool warningIfMissing = true;
					localPhysicalPathKey =
						_mmsEngineDBFacade->getPhysicalPathDetails(
						localMediaItemKey, -1, warningIfMissing,
						// 2022-12-18: il MIK potrebbe essere stato appena inserito dal task precedente
						true);

					referencesOutput.push_back(make_pair(localMediaItemKey, localPhysicalPathKey));
				}
				catch(MediaItemKeyNotFound& e)
				{
					_logger->warn(__FILEREF__
						+ "fillReferencesOutput. getMediaItemKeyDetailsByPhysicalPathKey failed"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", referenceUniqueName: " + referenceUniqueName
					);
				}
            }
        }
        catch(runtime_error& e)
        {
			string sParametersRoot = JSONUtils::toString(parametersRoot);

            string errorMessage = __FILEREF__ + "fillReferencesOutput failed"
                    + ", sParametersRoot: " + sParametersRoot
					+ ", e.what(): " + e.what()
                    ;
            _logger->error(errorMessage);

            throw e;
        }
        catch(exception& e)
        {
			string sParametersRoot = JSONUtils::toString(parametersRoot);

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
        "m3u8-tar.gz",
        "m3u8-streaming",
        "yuv",
        "mp4",
        "m4a",
        "m4p",
        "mpg",
        "mp2",
        "mp3",
        "mpeg",
        "mjpeg",
        "m4v",
        "3gp",
        "3g2",
        "mxf",
        "ts",
        "mts",
		"wav"
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

bool Validator::isCutTypeValid(string cutType)
{
    vector<string> validCutTypes = {
        "KeyFrameSeeking",
        "FrameAccurateWithEncoding",
        "FrameAccurateWithoutEncoding",
        "KeyFrameSeekingInterval"
    };

    for (string validCutType: validCutTypes)
    {
        if (cutType == validCutType) 
            return true;
    }
    
    return false;
}

bool Validator::isAddSilentTypeValid(string addType)
{
    vector<string> validAddTypes = {
        "entireTrack",
        "begin",
        "end"
    };

    for (string validAddType: validAddTypes)
    {
        if (addType == validAddType) 
            return true;
    }
    
    return false;
}

bool Validator::isFacebookNodeTypeValid(string nodeType)
{
    vector<string> validNodeTypes = {
        "Page",
        "User",
        "Event",
        "Group"
    };

    for (string validNodeType: validNodeTypes)
    {
        if (validNodeType == nodeType) 
            return true;
    }
    
    return false;
}

bool Validator::isFacebookLiveTypeValid(string liveType)
{
    vector<string> validLiveTypes = {
        "LiveNow",
        "LiveScheduled"
    };

    for (string validLiveType: validLiveTypes)
    {
        if (validLiveType == liveType) 
            return true;
    }
    
    return false;
}

bool Validator::isYouTubeLiveBroadcastSourceTypeValid(string sourceType)
{
    vector<string> validSourceTypes = {
        "Live",
        "MediaItem"
    };

    for (string validSourceType: validSourceTypes)
    {
        if (sourceType == validSourceType) 
            return true;
    }
    
    return false;
}

bool Validator::isFacebookLiveBroadcastSourceTypeValid(string sourceType)
{
    vector<string> validSourceTypes = {
        "Live",
        "MediaItem"
    };

    for (string validSourceType: validSourceTypes)
    {
        if (sourceType == validSourceType) 
            return true;
    }
    
    return false;
}

bool Validator::isYouTubePrivacyStatusValid(string privacyStatus)
{
    vector<string> validPrivacyStatuss = {
        "private",
        "public",
        "unlisted"
    };

    for (string validPrivacyStatus: validPrivacyStatuss)
    {
        if (privacyStatus == validPrivacyStatus) 
            return true;
    }
    
    return false;
}

bool Validator::isYouTubeTokenTypeValid(string tokenType)
{
    vector<string> validTokenTypes = {
        "RefreshToken",
        "AccessToken"
    };

    for (string validTokenType: validTokenTypes)
    {
        if (tokenType == validTokenType) 
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

	if (color.size() == 7
		&& color[0] == '#'
		&& isxdigit(color[1])
		&& isxdigit(color[2])
		&& isxdigit(color[3])
		&& isxdigit(color[4])
		&& isxdigit(color[5])
		&& isxdigit(color[6])
	)
		return true;

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
        "RTMP_Channel",
		"CDN_AWS",
		"CDN_CDN77",
        "UDP_Stream",
        "HLS_Channel"
        // "HLS",
        // "DASH"
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
        "SRT",
        "HLS_Channel"
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

    for (string suffix: suffixes)
    {
		if (speedType == suffix) 
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
    string label, json crossReferenceRoot,
	bool mediaItemKeyMandatory)
{
	if (mediaItemKeyMandatory)
	{
		vector<string> crossReferenceMandatoryFields = {
			"type",
			"mediaItemKey"
		};
		for (string mandatoryField: crossReferenceMandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(crossReferenceRoot, mandatoryField))
			{
				string sCrossReferenceRoot = JSONUtils::toString(crossReferenceRoot);
           
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
			"type",
		};
		for (string mandatoryField: crossReferenceMandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(crossReferenceRoot, mandatoryField))
			{
				string sCrossReferenceRoot = JSONUtils::toString(crossReferenceRoot);
           
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

	string field = "type";
	string sCrossReferenceType = JSONUtils::asString(crossReferenceRoot, field, "");
	MMSEngineDBFacade::CrossReferenceType crossReferenceType;
	try
	{
		crossReferenceType = MMSEngineDBFacade::toCrossReferenceType(sCrossReferenceType);
	}
	catch(exception& e)
	{
		string sCrossReferenceRoot = JSONUtils::toString(crossReferenceRoot);
           
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
		field = "parameters";
		if (!JSONUtils::isMetadataPresent(crossReferenceRoot, field))
		{
			string sCrossReferenceRoot = JSONUtils::toString(crossReferenceRoot);
           
			string errorMessage = __FILEREF__ + "Field 'CrossReference->Parameters' is missing"
				+ ", label: " + label
				+ ", sCrossReferenceRoot: " + sCrossReferenceRoot
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		json crossReferenceParameters = crossReferenceRoot[field];

		vector<string> crossReferenceCutMandatoryFields = {
			"startTimeInSeconds",
			"endTimeInSeconds"
		};
		for (string mandatoryField: crossReferenceCutMandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(crossReferenceParameters, mandatoryField))
			{
				string sCrossReferenceRoot = JSONUtils::toString(crossReferenceRoot);
           
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
    json encodingProfilesSetRoot)
{
    vector<string> mandatoryFields = {
        "label",
        "Profiles"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!JSONUtils::isMetadataPresent(encodingProfilesSetRoot, mandatoryField))
        {
            string sEncodingProfilesSetRoot = JSONUtils::toString(encodingProfilesSetRoot);
            
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
        json profilesRoot = encodingProfilesSetRoot[field];

        for (int profileIndex = 0; profileIndex < profilesRoot.size(); profileIndex++)
        {
            json encodingProfileRoot = profilesRoot[profileIndex];

            validateEncodingProfileRootMetadata(contentType, encodingProfileRoot);
        }
    }
    */
}

void Validator::validateEncodingProfileRootMetadata(
    MMSEngineDBFacade::ContentType contentType,
    json encodingProfileRoot)
{
    if (contentType == MMSEngineDBFacade::ContentType::Video)
        validateEncodingProfileRootVideoMetadata(encodingProfileRoot);
    else if (contentType == MMSEngineDBFacade::ContentType::Audio)
        validateEncodingProfileRootAudioMetadata(encodingProfileRoot);
    else // if (contentType == MMSEngineDBFacade::ContentType::Image)
        validateEncodingProfileRootImageMetadata(encodingProfileRoot);
}

void Validator::validateEncodingProfileRootVideoMetadata(
    json encodingProfileRoot)
{
    {
        vector<string> mandatoryFields = {
            "label",
            "fileFormat",
            "video",
            "audio"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!JSONUtils::isMetadataPresent(encodingProfileRoot, mandatoryField))
            {
                string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
    
    {
        string field = "label";
        string label = JSONUtils::asString(encodingProfileRoot, field, "");
        string mmsPredefinedProfilePrefix ("MMS_");
        if (label.compare(0, mmsPredefinedProfilePrefix.size(), mmsPredefinedProfilePrefix) == 0)   
        {
            string errorMessage = __FILEREF__ + "Profiles starting with " + mmsPredefinedProfilePrefix + " are reserved"
                    + ", Label: " + label;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
	json encodingProfileVideoRoot;
    {
        string field = "video";
        encodingProfileVideoRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "codec",
            "twoPasses",
            "bitRates"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!JSONUtils::isMetadataPresent(encodingProfileVideoRoot, mandatoryField))
            {
                string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }

    {
        string field = "bitRates";
		json videoBitRatesRoot = encodingProfileVideoRoot[field];

		if (videoBitRatesRoot.size() == 0)
		{
			string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);

			string errorMessage = __FILEREF__ + "No video bit rates are present"
				+ ", sEncodingProfileRoot: " + sEncodingProfileRoot;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

        vector<string> mandatoryFields = {
            "width",
            "height",
            "kBitRate"
        };
		for(int bitRateIndex = 0; bitRateIndex < videoBitRatesRoot.size(); bitRateIndex++)
		{
			json bitRateRoot = videoBitRatesRoot[bitRateIndex];

			for (string mandatoryField: mandatoryFields)
			{
				if (!JSONUtils::isMetadataPresent(bitRateRoot, mandatoryField))
				{
					string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);

					string errorMessage = __FILEREF__ + "Field is not present or it is null"
						+ ", Field: " + mandatoryField
						+ ", sEncodingProfileRoot: " + sEncodingProfileRoot;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
        }
    }

	json encodingProfileAudioRoot;
    {
        string field = "audio";
        encodingProfileAudioRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "codec",
            "bitRates"
        };
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(encodingProfileAudioRoot, mandatoryField))
			{
				string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);
               
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
                       + ", Field: " + mandatoryField
                       + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
    }

    {
        string field = "bitRates";
		json audioBitRatesRoot = encodingProfileAudioRoot[field];

		if (audioBitRatesRoot.size() == 0)
		{
			string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);

			string errorMessage = __FILEREF__ + "No audio bit rates are present"
				+ ", sEncodingProfileRoot: " + sEncodingProfileRoot;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

        vector<string> mandatoryFields = {
            "kBitRate"
        };
		for(int bitRateIndex = 0; bitRateIndex < audioBitRatesRoot.size(); bitRateIndex++)
		{
			json bitRateRoot = audioBitRatesRoot[bitRateIndex];

			for (string mandatoryField: mandatoryFields)
			{
				if (!JSONUtils::isMetadataPresent(bitRateRoot, mandatoryField))
				{
					string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);
                
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
            }
        }
    }
}

void Validator::validateEncodingProfileRootAudioMetadata(
    json encodingProfileRoot)
{
    {
        vector<string> mandatoryFields = {
            "label",
            "fileFormat",
            "audio"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!JSONUtils::isMetadataPresent(encodingProfileRoot, mandatoryField))
            {
                string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
    
    {
        string field = "label";
        string label = JSONUtils::asString(encodingProfileRoot, field, "");
        string mmsPredefinedProfilePrefix ("MMS_");
        if (label.compare(0, mmsPredefinedProfilePrefix.size(), mmsPredefinedProfilePrefix) == 0)   
        {
            string errorMessage = __FILEREF__ + "Profiles starting with " + mmsPredefinedProfilePrefix + " are reserved"
                    + ", Label: " + label;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    
	json encodingProfileAudioRoot;
    {
        string field = "audio";
        encodingProfileAudioRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "codec",
            "bitRates"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!JSONUtils::isMetadataPresent(encodingProfileAudioRoot, mandatoryField))
            {
                string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }

    {
        string field = "bitRates";
		json audioBitRatesRoot = encodingProfileAudioRoot[field];

		if (audioBitRatesRoot.size() == 0)
		{
			string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);

			string errorMessage = __FILEREF__ + "No audio bit rates are present"
				+ ", sEncodingProfileRoot: " + sEncodingProfileRoot;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

        vector<string> mandatoryFields = {
            "kBitRate"
        };
		for(int bitRateIndex = 0; bitRateIndex < audioBitRatesRoot.size(); bitRateIndex++)
		{
			json bitRateRoot = audioBitRatesRoot[bitRateIndex];

			for (string mandatoryField: mandatoryFields)
			{
				if (!JSONUtils::isMetadataPresent(bitRateRoot, mandatoryField))
				{
					string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);
                
					string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
            }
        }
    }
}

void Validator::validateEncodingProfileRootImageMetadata(
    json encodingProfileRoot)
{
    {
        vector<string> mandatoryFields = {
            "label",
            "fileFormat",
            "Image"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!JSONUtils::isMetadataPresent(encodingProfileRoot, mandatoryField))
            {
                string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }    

    {
        string field = "label";
        string label = JSONUtils::asString(encodingProfileRoot, field, "");
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
        json encodingProfileImageRoot = encodingProfileRoot[field];

        vector<string> mandatoryFields = {
            "width",
            "height",
            "AspectRatio",
            "InterlaceType"
        };
        for (string mandatoryField: mandatoryFields)
        {
            if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, mandatoryField))
            {
                string sEncodingProfileRoot = JSONUtils::toString(encodingProfileRoot);
                
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + mandatoryField
                        + ", sEncodingProfileRoot: " + sEncodingProfileRoot;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
}

void Validator::validateOutputRootMetadata(int64_t workspaceKey, string label,
	json outputRoot, bool encodingMandatory)
{

	string field = "outputType";
	string liveProxyOutputType;
	if (JSONUtils::isMetadataPresent(outputRoot, field))
	{
		liveProxyOutputType = JSONUtils::asString(outputRoot, field, "");
		if (!isLiveProxyOutputTypeValid(liveProxyOutputType))
		{
			string errorMessage = __FILEREF__ + field + " is wrong (it could be RTMP_Channel, CDN_AWS, CDN_CDN77, UDP_Stream or HLS_Channel)"
				+ ", Field: " + field
				+ ", liveProxyOutputType: " + liveProxyOutputType
				+ ", label: " + label
				;
			_logger->error(__FILEREF__ + errorMessage);
       
			throw runtime_error(errorMessage);
		}
	}

	if (liveProxyOutputType == "CDN_AWS")
	{
	}
	else if (liveProxyOutputType == "CDN_CDN77")
	{
	}
	else if (liveProxyOutputType == "RTMP_Channel")
	{
	}
	else if (liveProxyOutputType == "HLS_Channel")
	{
	}
	else if (liveProxyOutputType == "UDP_Stream")
	{
		vector<string> mandatoryFields = {
			"udpUrl"
		};
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(outputRoot, mandatoryField))
			{
				string sParametersRoot = JSONUtils::toString(outputRoot);
           
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

	if (JSONUtils::isMetadataPresent(outputRoot, "drawTextDetails"))
	{
		json drawTextDetailsRoot = outputRoot["drawTextDetails"];

		vector<string> mandatoryFields = {
			"text"
		};
		for (string mandatoryField: mandatoryFields)
		{
			if (!JSONUtils::isMetadataPresent(drawTextDetailsRoot, mandatoryField))
			{
				string sParametersRoot = JSONUtils::toString(drawTextDetailsRoot);
           
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + mandatoryField
					+ ", sParametersRoot: " + sParametersRoot
					+ ", label: " + label
                   ;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string field = "fontType";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			string fontType = JSONUtils::asString(drawTextDetailsRoot, field, "");

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

		field = "fontColor";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			string fontColor = JSONUtils::asString(drawTextDetailsRoot, field, "");

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

		field = "textPercentageOpacity";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			int textPercentageOpacity = JSONUtils::asInt(drawTextDetailsRoot, field, 200);

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

		field = "boxEnable";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			bool boxEnable = JSONUtils::asBool(drawTextDetailsRoot, field, true);                        
		}

		field = "boxPercentageOpacity";
		if (JSONUtils::isMetadataPresent(drawTextDetailsRoot, field))
		{
			int boxPercentageOpacity = JSONUtils::asInt(drawTextDetailsRoot, field, 200);

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
	}

	if (encodingMandatory)
	{
		if (!JSONUtils::isMetadataPresent(outputRoot, "encodingProfileKey")
			&& !JSONUtils::isMetadataPresent(outputRoot, "encodingProfileLabel"))
		{
			string errorMessage = string("encodingProfile is missing")
				+ ", label: " + label
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
	}
}

