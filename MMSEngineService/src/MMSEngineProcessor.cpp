
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "catralibraries/System.h"
#include "catralibraries/Encrypt.h"
#include "FFMpeg.h"
#include "MMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "CheckEncodingTimes.h"
#include "RetentionTimes.h"
#include "catralibraries/md5.h"
#include "EMailSender.h"
#include "Magick++.h"


MMSEngineProcessor::MMSEngineProcessor(
        int processorIdentifier,
        shared_ptr<spdlog::logger> logger, 
        shared_ptr<MultiEventsSet> multiEventsSet,
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        shared_ptr<MMSStorage> mmsStorage,
        ActiveEncodingsManager* pActiveEncodingsManager,
        Json::Value configuration
)
{
    _processorIdentifier         = processorIdentifier;
    _logger             = logger;
    _configuration      = configuration;
    _multiEventsSet     = multiEventsSet;
    _mmsEngineDBFacade  = mmsEngineDBFacade;
    _mmsStorage         = mmsStorage;
    _pActiveEncodingsManager = pActiveEncodingsManager;

    _processorMMS                   = System::getHostName();
    
// cout << "processorIdentifier: " << processorIdentifier << endl;
    _maxDownloadAttemptNumber       = configuration["download"].get("maxDownloadAttemptNumber", 5).asInt();
    _progressUpdatePeriodInSeconds  = configuration["download"].get("progressUpdatePeriodInSeconds", 5).asInt();
    _secondsWaitingAmongDownloadingAttempt  = configuration["download"].get("secondsWaitingAmongDownloadingAttempt", 5).asInt();
    
    _maxIngestionJobsPerEvent       = configuration["mms"].get("maxIngestionJobsPerEvent", 5).asInt();
    // _maxIngestionJobsWithDependencyToCheckPerEvent = configuration["mms"].get("maxIngestionJobsWithDependencyToCheckPerEvent", 5).asInt();

    _dependencyExpirationInHours        = configuration["mms"].get("dependencyExpirationInHours", 5).asInt();
    _stagingRetentionInDays             = configuration["mms"].get("stagingRetentionInDays", 5).asInt();
    _downloadChunkSizeInMegaBytes       = configuration["download"].get("downloadChunkSizeInMegaBytes", 5).asInt();
    
    _emailProtocol                      = _configuration["EmailNotification"].get("protocol", "XXX").asString();
    _emailServer                        = _configuration["EmailNotification"].get("server", "XXX").asString();
    _emailPort                          = _configuration["EmailNotification"].get("port", "XXX").asInt();
    _emailUserName                      = _configuration["EmailNotification"].get("userName", "XXX").asString();
    string _emailPassword;
    {
        string encryptedPassword = _configuration["EmailNotification"].get("password", "XXX").asString();
        _emailPassword = Encrypt::decrypt(encryptedPassword);        
    }
    _emailFrom                          = _configuration["EmailNotification"].get("from", "XXX").asString();
    
    if (_processorIdentifier == 0)
    {
        try
        {
            _mmsEngineDBFacade->resetProcessingJobsIfNeeded(_processorMMS);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->resetProcessingJobsIfNeeded failed"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", exception: " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->resetProcessingJobsIfNeeded failed"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
            );

            throw e;
        }
    }
}

MMSEngineProcessor::~MMSEngineProcessor()
{
    
}

void MMSEngineProcessor::operator ()() 
{
    bool blocking = true;
    chrono::milliseconds milliSecondsToBlock(100);

    //SPDLOG_DEBUG(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    // SPDLOG_TRACE(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    _logger->info(__FILEREF__ + "MMSEngineProcessor thread started"
        + ", _processorIdentifier: " + to_string(_processorIdentifier)
    );

    bool endEvent = false;
    while(!endEvent)
    {
        shared_ptr<Event2> event = _multiEventsSet->getAndRemoveFirstEvent(MMSENGINEPROCESSORNAME, blocking, milliSecondsToBlock);
        if (event == nullptr)
        {
            // cout << "No event found or event not yet expired" << endl;

            continue;
        }

        switch(event->getEventKey().first)
        {
            case MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTIONEVENT:	// 1
            {
                _logger->debug(__FILEREF__ + "1. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );

                try
                {
                    handleCheckIngestionEvent ();
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleCheckIngestionEvent failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

                _logger->debug(__FILEREF__ + "2. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );
            }
            break;
            case MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT:	// 2
            {
                _logger->debug(__FILEREF__ + "1. Received LOCALASSETINGESTIONEVENT"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );

                shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = dynamic_pointer_cast<LocalAssetIngestionEvent>(event);

                try
                {
                    handleLocalAssetIngestionEvent (localAssetIngestionEvent);
                }
                catch(runtime_error e)
                {
                    _logger->error(__FILEREF__ + "handleLocalAssetIngestionEvent failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", exception: " + e.what()
                    );
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleLocalAssetIngestionEvent failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<LocalAssetIngestionEvent>(localAssetIngestionEvent);

                _logger->debug(__FILEREF__ + "2. Received LOCALASSETINGESTIONEVENT"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );
            }
            break;
            case MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODINGEVENT:	// 3
            {
                _logger->debug(__FILEREF__ + "1. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );

                try
                {
                    handleCheckEncodingEvent ();
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleCheckEncodingEvent failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

                _logger->debug(__FILEREF__ + "2. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );
            }
            break;
            case MMSENGINE_EVENTTYPEIDENTIFIER_CONTENTRETENTIONEVENT:	// 4
            {
                _logger->debug(__FILEREF__ + "1. Received MMSENGINE_EVENTTYPEIDENTIFIER_RETENTIONEVENT"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );

                try
                {
                    
                    thread contentRetention(&MMSEngineProcessor::handleContentRetentionEvent, this);
                    contentRetention.detach();
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleContentRetentionEvent failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

                _logger->debug(__FILEREF__ + "2. Received MMSENGINE_EVENTTYPEIDENTIFIER_RETENTIONEVENT"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );
            }
            break;
            case MMSENGINE_EVENTTYPEIDENTIFIER_MULTILOCALASSETINGESTIONEVENT:	// 5
            {
                _logger->debug(__FILEREF__ + "1. Received MULTILOCALASSETINGESTIONEVENT"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );

                shared_ptr<MultiLocalAssetIngestionEvent>    multiLocalAssetIngestionEvent = dynamic_pointer_cast<MultiLocalAssetIngestionEvent>(event);

                try
                {
                    handleMultiLocalAssetIngestionEvent (multiLocalAssetIngestionEvent);
                }
                catch(runtime_error e)
                {
                    _logger->error(__FILEREF__ + "handleMultiLocalAssetIngestionEvent failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", exception: " + e.what()
                    );
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleMultiLocalAssetIngestionEvent failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<MultiLocalAssetIngestionEvent>(multiLocalAssetIngestionEvent);

                _logger->debug(__FILEREF__ + "2. Received MULTILOCALASSETINGESTIONEVENT"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                );
            }
            break;
            default:
                throw runtime_error(string("Event type identifier not managed")
                        + to_string(event->getEventKey().first));
        }
    }

    _logger->info(__FILEREF__ + "MMSEngineProcessor thread terminated"
        + ", _processorIdentifier: " + to_string(_processorIdentifier)
    );
}

void MMSEngineProcessor::handleCheckIngestionEvent()
{
    
    try
    {
        vector<tuple<int64_t,shared_ptr<Workspace>,string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus>> 
                ingestionsToBeManaged;
        
        try
        {
            _mmsEngineDBFacade->getIngestionsToBeManaged(ingestionsToBeManaged, 
                    _processorMMS, _maxIngestionJobsPerEvent 
            );
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "getIngestionsToBeManaged failed"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", exception: " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "getIngestionsToBeManaged failed"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", exception: " + e.what()
            );

            throw e;
        }
        
        for (tuple<int64_t, shared_ptr<Workspace>, string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus> 
                ingestionToBeManaged: ingestionsToBeManaged)
        {
            int64_t ingestionJobKey;
            try
            {
                shared_ptr<Workspace> workspace;
                string metaDataContent;
                string sourceReference;
                MMSEngineDBFacade::IngestionType ingestionType;
                MMSEngineDBFacade::IngestionStatus ingestionStatus;

                tie(ingestionJobKey, workspace, metaDataContent,
                        ingestionType, ingestionStatus) = ingestionToBeManaged;
                
                _logger->info(__FILEREF__ + "json to be processed"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                    + ", metaDataContent: " + metaDataContent
                    + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType)
                    + ", ingestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                );

                try
                {
                    _mmsEngineDBFacade->checkWorkspaceMaxIngestionNumber (
                            workspace->_workspaceKey);
                }
                catch(runtime_error e)
                {
                    _logger->error(__FILEREF__ + "checkWorkspaceMaxIngestionNumber failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", exception: " + e.what()
                    );
                    string errorMessage = e.what();

                    _logger->info(__FILEREF__ + "Update IngestionJob"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", IngestionStatus: " + "End_WorkspaceReachedHisMaxIngestionNumber"
                        + ", errorMessage: " + e.what()
                    );                            
                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                            MMSEngineDBFacade::IngestionStatus::End_WorkspaceReachedHisMaxIngestionNumber,
                            e.what(), 
                            "" // processorMMS
                    );

                    throw e;
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "checkWorkspaceMaxIngestionNumber failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", exception: " + e.what()
                    );
                    string errorMessage = e.what();

                    _logger->info(__FILEREF__ + "Update IngestionJob"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", IngestionStatus: " + "End_WorkspaceReachedHisMaxIngestionNumber"
                        + ", errorMessage: " + e.what()
                    );                            
                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                            MMSEngineDBFacade::IngestionStatus::End_WorkspaceReachedHisMaxIngestionNumber,
                            e.what(), 
                            "" // processorMMS
                    );

                    throw e;
                }
                
                if (ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress
                        || ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress
                        || ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress
                        || ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
                {
                    // source binary download or uploaded terminated

                    string sourceFileName = to_string(ingestionJobKey) + "_source";

                    {
                        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                                ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

                        localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
                        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
                        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

                        localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
                        localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
                        localAssetIngestionEvent->setMMSSourceFileName("");
                        localAssetIngestionEvent->setWorkspace(workspace);
                        localAssetIngestionEvent->setIngestionType(ingestionType);
                        localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

                        localAssetIngestionEvent->setMetadataContent(metaDataContent);

                        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
                        _multiEventsSet->addEvent(event);

                        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", getEventKey().first: " + to_string(event->getEventKey().first)
                            + ", getEventKey().second: " + to_string(event->getEventKey().second));
                    }
                }
                else    // Start_TaskQueued
                {
                    Json::Value parametersRoot;
                    try
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(metaDataContent.c_str(),
                                metaDataContent.c_str() + metaDataContent.size(), 
                                &parametersRoot, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = __FILEREF__ + "failed to parse the metadata"
                                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", errors: " + errors
                                    + ", metaDataContent: " + metaDataContent
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                    catch(...)
                    {
                        string errorMessage = string("metadata json is not well format")
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", metaDataContent: " + metaDataContent
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        _logger->info(__FILEREF__ + "Update IngestionJob"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", IngestionStatus: " + "End_ValidationMetadataFailed"
                            + ", errorMessage: " + errorMessage
                            + ", processorMMS: " + ""
                        );
                        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, 
                                errorMessage,
                                "" // processorMMS
                        );

                        throw runtime_error(errorMessage);
                    }

                    vector<pair<int64_t,Validator::DependencyType>> dependencies;
                    try
                    {
                        Validator validator(_logger, _mmsEngineDBFacade);
                        
                        dependencies = validator.validateSingleTaskMetadata(
                                workspace->_workspaceKey, ingestionType, parametersRoot);                        
                    }
                    catch(runtime_error e)
                    {
                        _logger->error(__FILEREF__ + "validateMetadata failed"
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", exception: " + e.what()
                        );

                        string errorMessage = e.what();

                        _logger->info(__FILEREF__ + "Update IngestionJob"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", IngestionStatus: " + "End_ValidationMetadataFailed"
                            + ", errorMessage: " + errorMessage
                            + ", processorMMS: " + ""
                        );
                        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, 
                                errorMessage,
                                "" // processorMMS
                        );

                        throw runtime_error(errorMessage);
                    }
                    catch(exception e)
                    {
                        _logger->error(__FILEREF__ + "validateMetadata failed"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", exception: " + e.what()
                        );

                        string errorMessage = e.what();

                        _logger->info(__FILEREF__ + "Update IngestionJob"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", IngestionStatus: " + "End_ValidationMetadataFailed"
                            + ", errorMessage: " + errorMessage
                            + ", processorMMS: " + ""
                        );
                        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, 
                                errorMessage,
                                "" // processorMMS
                        );

                        throw runtime_error(errorMessage);
                    }

                    {
                        if (ingestionType == MMSEngineDBFacade::IngestionType::AddContent)
                        {
                            MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
                            string mediaSourceURL;
                            string mediaFileFormat;
                            string md5FileCheckSum;
                            int fileSizeInBytes;
                            try
                            {
                                tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> mediaSourceDetails;

                                mediaSourceDetails = getMediaSourceDetails(
                                        ingestionJobKey, workspace,
                                        ingestionType, parametersRoot);

                                tie(nextIngestionStatus,
                                        mediaSourceURL, mediaFileFormat, 
                                        md5FileCheckSum, fileSizeInBytes) = mediaSourceDetails;                        
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }

                            try
                            {
                                if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress)
                                {
                                    string errorMessage = "";
                                    string processorMMS = "";

                                    _logger->info(__FILEREF__ + "Update IngestionJob"
                                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus)
                                        + ", errorMessage: " + errorMessage
                                        + ", processorMMS: " + processorMMS
                                    );                            
                                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                            nextIngestionStatus,
                                            errorMessage,
                                            processorMMS
                                            );

                                    thread downloadMediaSource(&MMSEngineProcessor::downloadMediaSourceFile, this, 
                                        mediaSourceURL, ingestionJobKey, workspace);
                                    downloadMediaSource.detach();
                                }
                                else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress)
                                {
                                    string errorMessage = "";
                                    string processorMMS = "";

                                    _logger->info(__FILEREF__ + "Update IngestionJob"
                                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus)
                                        + ", errorMessage: " + errorMessage
                                        + ", processorMMS: " + processorMMS
                                    );                            
                                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                            nextIngestionStatus,
                                            errorMessage,
                                            processorMMS
                                            );

                                    thread moveMediaSource(&MMSEngineProcessor::moveMediaSourceFile, this, 
                                        mediaSourceURL, ingestionJobKey, workspace);
                                    moveMediaSource.detach();
                                }
                                else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress)
                                {
                                    string errorMessage = "";
                                    string processorMMS = "";

                                    _logger->info(__FILEREF__ + "Update IngestionJob"
                                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus)
                                        + ", errorMessage: " + errorMessage
                                        + ", processorMMS: " + processorMMS
                                    );                            
                                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                            nextIngestionStatus, 
                                            errorMessage,
                                            processorMMS
                                            );

                                    thread copyMediaSource(&MMSEngineProcessor::copyMediaSourceFile, this, 
                                        mediaSourceURL, ingestionJobKey, workspace);
                                    copyMediaSource.detach();
                                }
                                else // if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
                                {
                                    string errorMessage = "";
                                    string processorMMS = "";

                                    _logger->info(__FILEREF__ + "Update IngestionJob"
                                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus)
                                        + ", errorMessage: " + errorMessage
                                        + ", processorMMS: " + processorMMS
                                    );                            
                                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                            nextIngestionStatus, 
                                            errorMessage,
                                            processorMMS
                                            );
                                }
                            }
                            catch(exception e)
                            {
                                string errorMessage = string("Downloading media source or update Ingestion job failed")
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                ;
                                _logger->error(__FILEREF__ + errorMessage);

                                throw runtime_error(errorMessage);
                            }
                        }
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::RemoveContent)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                removeContent(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "removeContent failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "removeContent failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                        }
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::Encode)
                        {
                            try
                            {
                                manageEncodeTask(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "manageEncodeTask failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "manageEncodeTask failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                        }
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::Frame
                                || ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
                                || ingestionType == MMSEngineDBFacade::IngestionType::IFrames
                                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames
                                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames
                                )
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
                                    || ingestionType == MMSEngineDBFacade::IngestionType::IFrames
                                    || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames
                                    || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
                                {
                                    manageGenerateFramesTask(
                                        ingestionJobKey,
                                        workspace,
                                        ingestionType,
                                        parametersRoot,
                                        dependencies);
                                }
                                else
                                {
                                    generateAndIngestFrames(
                                        ingestionJobKey, 
                                        workspace, 
                                        ingestionType,
                                        parametersRoot, 
                                        dependencies);
                                }
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestFrames failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestFrames failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                        }
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::Slideshow)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                generateAndIngestSlideshow(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestSlideshow failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestSlideshow failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                        }
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::ConcatDemuxer)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                generateAndIngestConcatenation(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestConcatenation failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestConcatenation failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                        }
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::Cut)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                generateAndIngestCutMedia(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestCutMedia failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestCutMedia failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                        }
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayImageOnVideo)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                manageOverlayImageOnVideoTask(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "manageOverlayImageOnVideoTask failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "manageOverlayImageOnVideoTask failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                        }
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::OverlayTextOnVideo)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                manageOverlayTextOnVideoTask(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "manageOverlayTextOnVideoTask failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "manageOverlayTextOnVideoTask failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                        }
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::EmailNotification)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                manageEmailNotification(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "manageEmailNotification failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "manageEmailNotification failed"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionStatus: " + "End_IngestionFailure"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                        }
                        else
                        {
                            string errorMessage = string("Unknown IngestionType")
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
                            _logger->error(__FILEREF__ + errorMessage);

                            _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
                                + ", errorMessage: " + errorMessage
                                + ", processorMMS: " + ""
                            );                            
                            _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                    MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, 
                                    errorMessage,
                                    "" // processorMMS
                                    );

                            throw runtime_error(errorMessage);
                        }
                    }
                }
            }
            catch(runtime_error e)
            {
                _logger->error(__FILEREF__ + "Exception managing the Ingestion entry"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", exception: " + e.what()
                );
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "Exception managing the Ingestion entry"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", exception: " + e.what()
                );
            }
        }
    }
    catch(...)
    {
        _logger->error(__FILEREF__ + "handleCheckIngestionEvent failed"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
        );
    }
}

void MMSEngineProcessor::handleLocalAssetIngestionEvent (
    shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent)
{
    string workspaceIngestionBinaryPathName;

    workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(
            localAssetIngestionEvent->getWorkspace());
    workspaceIngestionBinaryPathName
            .append("/")
            .append(localAssetIngestionEvent->getIngestionSourceFileName())
            ;
    
    string      metadataFileContent;
    vector<pair<int64_t,Validator::DependencyType>> dependencies;
    Json::Value parametersRoot;
    Validator validator(_logger, _mmsEngineDBFacade);
    try
    {
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();
        string errors;

        string sMetadataContent = localAssetIngestionEvent->getMetadataContent();
        
        // LF and CR create problems to the json parser...
        while (sMetadataContent.back() == 10 || sMetadataContent.back() == 13)
            sMetadataContent.pop_back();
        
        bool parsingSuccessful = reader->parse(sMetadataContent.c_str(),
                sMetadataContent.c_str() + sMetadataContent.size(), 
                &parametersRoot, &errors);
        delete reader;

        if (!parsingSuccessful)
        {
            string errorMessage = __FILEREF__ + "failed to parse the metadata"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                    + ", errors: " + errors
                    + ", metaDataContent: " + sMetadataContent
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        dependencies = validator.validateSingleTaskMetadata(
                localAssetIngestionEvent->getWorkspace()->_workspaceKey,
                localAssetIngestionEvent->getIngestionType(), parametersRoot);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "validateMetadata failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", localAssetIngestionEvent->getMetadataContent(): " + localAssetIngestionEvent->getMetadataContent()
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMetadataFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what(), "" //ProcessorMMS
        );

        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        FileIO::remove(workspaceIngestionBinaryPathName);
            
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "validateMetadata failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMetadataFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what(), "" // ProcessorMMS
        );

        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        FileIO::remove(workspaceIngestionBinaryPathName);
            
        throw e;
    }

    MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
    string mediaSourceURL;
    string mediaFileFormat;
    string md5FileCheckSum;
    int fileSizeInBytes;
    try
    {
        tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int>
            mediaSourceDetails = getMediaSourceDetails(
                localAssetIngestionEvent->getIngestionJobKey(),
                localAssetIngestionEvent->getWorkspace(),
                localAssetIngestionEvent->getIngestionType(), parametersRoot);
        
        tie(nextIngestionStatus,
                mediaSourceURL, mediaFileFormat, 
                md5FileCheckSum, fileSizeInBytes) = mediaSourceDetails;                        
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed,
            e.what(), "" // ProcessorMMS
        );

        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        FileIO::remove(workspaceIngestionBinaryPathName);
            
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed,
            e.what(), "" // ProcessorMMS
        );

        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        FileIO::remove(workspaceIngestionBinaryPathName);
            
        throw e;
    }

    try
    {
        validateMediaSourceFile(
                localAssetIngestionEvent->getIngestionJobKey(),
                workspaceIngestionBinaryPathName,
                md5FileCheckSum, fileSizeInBytes);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "validateMediaSourceFile failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed,
            e.what(), "" // ProcessorMMS
        );

        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        FileIO::remove(workspaceIngestionBinaryPathName);
            
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "validateMediaSourceFile failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed,
            e.what(), "" // ProcessorMMS
        );

        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        FileIO::remove(workspaceIngestionBinaryPathName);
            
        throw e;
    }

    string mediaSourceFileName = localAssetIngestionEvent->getMMSSourceFileName();
    if (mediaSourceFileName == "")
    {
        mediaSourceFileName = localAssetIngestionEvent->getIngestionSourceFileName() + "." + mediaFileFormat;
    }

    string relativePathToBeUsed;
    unsigned long mmsPartitionIndexUsed;
    string mmsAssetPathName;
    try
    {
        relativePathToBeUsed = _mmsEngineDBFacade->nextRelativePathToBeUsed (
                localAssetIngestionEvent->getWorkspace()->_workspaceKey);
        
        bool partitionIndexToBeCalculated   = true;
        bool deliveryRepositoriesToo        = true;
        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            workspaceIngestionBinaryPathName,
            localAssetIngestionEvent->getWorkspace()->_directoryName,
            mediaSourceFileName,
            relativePathToBeUsed,
            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed,
            deliveryRepositoriesToo,
            localAssetIngestionEvent->getWorkspace()->_territories
            );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
        );
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), "" // ProcessorMMS
        );
        
        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        FileIO::remove(workspaceIngestionBinaryPathName);
            
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
        );
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), "" // ProcessorMMS
        );
        
        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        FileIO::remove(workspaceIngestionBinaryPathName);
            
        throw e;
    }

    MMSEngineDBFacade::ContentType contentType;
    
    int64_t durationInMilliSeconds = -1;
    long bitRate = -1;
    string videoCodecName;
    string videoProfile;
    int videoWidth = -1;
    int videoHeight = -1;
    string videoAvgFrameRate;
    long videoBitRate = -1;
    string audioCodecName;
    long audioSampleRate = -1;
    int audioChannels = -1;
    long audioBitRate = -1;

    int imageWidth = -1;
    int imageHeight = -1;
    string imageFormat;
    int imageQuality = -1;
    if (validator.isVideoAudioFileFormat(mediaFileFormat))
    {
        try
        {
            FFMpeg ffmpeg (_configuration, _logger);
            tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> mediaInfo =
                ffmpeg.getMediaInfo(mmsAssetPathName);

            tie(durationInMilliSeconds, bitRate, 
                videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
                audioCodecName, audioSampleRate, audioChannels, audioBitRate) = mediaInfo;
            
            if (localAssetIngestionEvent->getForcedAvgFrameRate() != "")
            {
                _logger->info(__FILEREF__ + "handleLocalAssetIngestionEvent. Forced Avg Frame Rate"
                    + ", current avgFrameRate: " + videoAvgFrameRate
                    + ", forced avgFrameRate: " + localAssetIngestionEvent->getForcedAvgFrameRate()
                );
                
                videoAvgFrameRate = localAssetIngestionEvent->getForcedAvgFrameRate();
            }
                
            if (videoCodecName == "")
                contentType = MMSEngineDBFacade::ContentType::Audio;
            else
                contentType = MMSEngineDBFacade::ContentType::Video;
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "EncoderVideoAudioProxy::getMediaInfo failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", IngestionStatus: " + "End_IngestionFailure"
                + ", errorMessage: " + e.what()
            );                            
            _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                    MMSEngineDBFacade::IngestionStatus::End_IngestionFailure,
                    e.what(), "" // ProcessorMMS
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "EncoderVideoAudioProxy::getVideoOrAudioDurationInMilliSeconds failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", IngestionStatus: " + "End_IngestionFailure"
                + ", errorMessage: " + e.what()
            );                            
            _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                    MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                    e.what(), "" // ProcessorMMS
            );

            throw e;
        }        
    }
    else if (validator.isImageFileFormat(mediaFileFormat))
    {
        try
        {
            _logger->info(__FILEREF__ + "Processing through Magick"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            Magick::Image      imageToEncode;

            imageToEncode.read (mmsAssetPathName.c_str());

            imageWidth	= imageToEncode.columns();
            imageHeight	= imageToEncode.rows();
            imageFormat = imageToEncode.magick();
            imageQuality = imageToEncode.quality();
            
            contentType = MMSEngineDBFacade::ContentType::Image;
        }
        catch( Magick::WarningCoder &e )
        {
            // Process coder warning while loading file (e.g. TIFF warning)
            // Maybe the user will be interested in these warnings (or not).
            // If a warning is produced while loading an image, the image
            // can normally still be used (but not if the warning was about
            // something important!)
            _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", e.what(): " + e.what()
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", IngestionStatus: " + "End_IngestionFailure"
                + ", errorMessage: " + e.what()
            );                            
            _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                    MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                    e.what(), "" // ProcessorMMS
            );

            throw runtime_error(e.what());
        }
        catch( Magick::Warning &e )
        {
            _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", e.what(): " + e.what()
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", IngestionStatus: " + "End_IngestionFailure"
                + ", errorMessage: " + e.what()
            );                            
            _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                    MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                    e.what(), "" // ProcessorMMS
            );

            throw runtime_error(e.what());
        }
        catch( Magick::ErrorFileOpen &e ) 
        { 
            _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", e.what(): " + e.what()
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", IngestionStatus: " + "End_IngestionFailure"
                + ", errorMessage: " + e.what()
            );                            
            _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                    MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                    e.what(), "" // ProcessorMMS
            );

            throw runtime_error(e.what());
        }
        catch (Magick::Error &e)
        { 
            _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", e.what(): " + e.what()
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", IngestionStatus: " + "End_IngestionFailure"
                + ", errorMessage: " + e.what()
            );                            
            _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                    MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                    e.what(), "" // ProcessorMMS
            );

            throw runtime_error(e.what());
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", IngestionStatus: " + "End_IngestionFailure"
                + ", errorMessage: " + e.what()
            );                            
            _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                    MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                    e.what(), "" // ProcessorMMS
            );

            throw e;
        }
    }
    else
    {
        string errorMessage = string("Unknown mediaSourceFileName extension")
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mediaSourceFileName: " + mediaSourceFileName
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mmsAssetPathName: " + mmsAssetPathName
        );
        FileIO::remove(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + errorMessage
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                errorMessage, "" // ProcessorMMS
        );

        throw runtime_error(errorMessage);
    }

    int64_t mediaItemKey;
    try
    {
        bool inCaseOfLinkHasItToBeRead = false;
        unsigned long sizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                inCaseOfLinkHasItToBeRead);   

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata..."
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", contentType: " + MMSEngineDBFacade::toString(contentType)
            + ", relativePathToBeUsed: " + relativePathToBeUsed
            + ", mediaSourceFileName: " + mediaSourceFileName
            + ", mmsPartitionIndexUsed: " + to_string(mmsPartitionIndexUsed)
            + ", sizeInBytes: " + to_string(sizeInBytes)

            + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
            + ", bitRate: " + to_string(bitRate)
            + ", videoCodecName: " + videoCodecName
            + ", videoProfile: " + videoProfile
            + ", videoWidth: " + to_string(videoWidth)
            + ", videoHeight: " + to_string(videoHeight)
            + ", videoAvgFrameRate: " + videoAvgFrameRate
            + ", videoBitRate: " + to_string(videoBitRate)
            + ", audioCodecName: " + audioCodecName
            + ", audioSampleRate: " + to_string(audioSampleRate)
            + ", audioChannels: " + to_string(audioChannels)
            + ", audioBitRate: " + to_string(audioBitRate)

            + ", imageWidth: " + to_string(imageWidth)
            + ", imageHeight: " + to_string(imageHeight)
            + ", imageFormat: " + imageFormat
            + ", imageQuality: " + to_string(imageQuality)
        );

        pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey =
                _mmsEngineDBFacade->saveIngestedContentMetadata (
                    localAssetIngestionEvent->getWorkspace(),
                    localAssetIngestionEvent->getIngestionJobKey(),
                    localAssetIngestionEvent->getIngestionRowToBeUpdatedAsSuccess(),
                    contentType,
                    parametersRoot,
                    relativePathToBeUsed,
                    mediaSourceFileName,
                    mmsPartitionIndexUsed,
                    sizeInBytes,
                
                    // video-audio
                    durationInMilliSeconds,
                    bitRate,
                    videoCodecName,
                    videoProfile,
                    videoWidth,
                    videoHeight,
                    videoAvgFrameRate,
                    videoBitRate,
                    audioCodecName,
                    audioSampleRate,
                    audioChannels,
                    audioBitRate,

                    // image
                    imageWidth,
                    imageHeight,
                    imageFormat,
                    imageQuality
        );

        mediaItemKey = mediaItemKeyAndPhysicalPathKey.first;

        _logger->info(__FILEREF__ + "Added a new ingested content"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mediaItemKey: " + to_string(mediaItemKeyAndPhysicalPathKey.first)
            + ", physicalPathKey: " + to_string(mediaItemKeyAndPhysicalPathKey.second)
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", e.what: " + e.what()
        );

        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mmsAssetPathName: " + mmsAssetPathName
        );
        FileIO::remove(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), "" // ProcessorMMS
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
        );

        _logger->info(__FILEREF__ + "Remove file"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mmsAssetPathName: " + mmsAssetPathName
        );
        FileIO::remove(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), 
                "" // ProcessorMMS
        );

        throw e;
    }    
}

void MMSEngineProcessor::removeContent(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any media to be removed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        for (pair<int64_t,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            if (keyAndDependencyType.second == Validator::DependencyType::MediaItemKey)
            {
                _logger->info(__FILEREF__ + "removeMediaItem"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", mediaItemKey: " + to_string(keyAndDependencyType.first)
                );
                _mmsStorage->removeMediaItem(keyAndDependencyType.first);
            }
            else
            {
                _logger->info(__FILEREF__ + "removePhysicalPath"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", physicalPathKey: " + to_string(keyAndDependencyType.first)
                );
                _mmsStorage->removePhysicalPath(keyAndDependencyType.first);
            }
        }

        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_TaskSuccess"
            + ", errorMessage: " + ""
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                MMSEngineDBFacade::IngestionStatus::End_TaskSuccess, 
                "", // errorMessage
                "" // ProcessorMMS
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestCutMedia failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), 
                "" // ProcessorMMS
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestCutMedia failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), 
                "" // ProcessorMMS
        );

        throw e;
    }
}

void MMSEngineProcessor::handleMultiLocalAssetIngestionEvent (
    shared_ptr<MultiLocalAssetIngestionEvent> multiLocalAssetIngestionEvent)
{
    
    string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
            multiLocalAssetIngestionEvent->getWorkspace());
    vector<string> generatedFramesFileNames;
    
    try
    {
        // get files from file system       
        {
            string imageBaseFileName = to_string(multiLocalAssetIngestionEvent->getIngestionJobKey());

            FileIO::DirectoryEntryType_t detDirectoryEntryType;
            shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (workspaceIngestionRepository + "/");

            bool scanDirectoryFinished = false;
            while (!scanDirectoryFinished)
            {
                string directoryEntry;
                try
                {
                    string directoryEntry = FileIO::readDirectory (directory,
                        &detDirectoryEntryType);

                    if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
                        continue;

                    if (directoryEntry.size() >= imageBaseFileName.size() && 0 == directoryEntry.compare(0, imageBaseFileName.size(), imageBaseFileName))
                        generatedFramesFileNames.push_back(directoryEntry);
                }
                catch(DirectoryListFinished e)
                {
                    scanDirectoryFinished = true;
                }
                catch(runtime_error e)
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
                           + ", e.what(): " + e.what()
                    ;
                    _logger->error(errorMessage);

                    throw e;
                }
                catch(exception e)
                {
                    string errorMessage = __FILEREF__ + "ffmpeg: listing directory failed"
                           + ", e.what(): " + e.what()
                    ;
                    _logger->error(errorMessage);

                    throw e;
                }
            }

            FileIO::closeDirectory (directory);
        }
           
        // we have one ingestion job row and many generatd frames to be ingested
        // We want to update the ingestion row just once at the end in case of success
        // or when an error happens.
        // To do this we will add a field in the localAssetIngestionEvent structure (ingestionRowToBeUpdatedAsSuccess)
        // and we will set it to false but the last frame that we will set to true
        // In case of error, handleLocalAssetIngestionEvent will update ingestion row
        // and we will not call anymore handleLocalAssetIngestionEvent for the next frames
        // When I say 'update the ingestion row', it's not just the update but it is also
        // manageIngestionJobStatusUpdate
        bool generatedFrameIngestionFailed = false;

        for(vector<string>::iterator it = generatedFramesFileNames.begin(); 
                it != generatedFramesFileNames.end(); ++it) 
        {
            string generatedFrameFileName = *it;

            if (generatedFrameIngestionFailed)
            {
                string workspaceIngestionBinaryPathName = workspaceIngestionRepository 
                        + "/"
                        + generatedFrameFileName
                        ;

                _logger->info(__FILEREF__ + "Remove file"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
                    + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                );
                FileIO::remove(workspaceIngestionBinaryPathName);
            }
            else
            {
                _logger->info(__FILEREF__ + "Generated Frame to ingest"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
                    + ", generatedFrameFileName: " + generatedFrameFileName
                    // + ", textToBeReplaced: " + textToBeReplaced
                    // + ", textToReplace: " + textToReplace
                );

                string fileFormat;
                size_t extensionIndex = generatedFrameFileName.find_last_of(".");
                if (extensionIndex == string::npos)
                {
                    string errorMessage = __FILEREF__ + "No fileFormat (extension of the file) found in generatedFileName"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
                            + ", generatedFrameFileName: " + generatedFrameFileName
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                fileFormat = generatedFrameFileName.substr(extensionIndex + 1);

    //            if (mmsSourceFileName.find(textToBeReplaced) != string::npos)
    //                mmsSourceFileName.replace(mmsSourceFileName.find(textToBeReplaced), textToBeReplaced.length(), textToReplace);

                _logger->info(__FILEREF__ + "Generated Frame to ingest"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
                    + ", new generatedFrameFileName: " + generatedFrameFileName
                    + ", fileFormat: " + fileFormat
                );

                string imageMetaDataContent = generateMediaMetadataToIngest(
                        multiLocalAssetIngestionEvent->getIngestionJobKey(),
                        // mjpeg,
                        fileFormat,
                        multiLocalAssetIngestionEvent->getParametersRoot()
                );

                {
                    // shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                    //        ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);
                    shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent 
                            = make_shared<LocalAssetIngestionEvent>();

                    localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
                    localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
                    localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

                    localAssetIngestionEvent->setIngestionJobKey(multiLocalAssetIngestionEvent->getIngestionJobKey());
                    localAssetIngestionEvent->setIngestionSourceFileName(generatedFrameFileName);
                    localAssetIngestionEvent->setMMSSourceFileName(generatedFrameFileName);
                    localAssetIngestionEvent->setWorkspace(multiLocalAssetIngestionEvent->getWorkspace());
                    localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
                    localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(false);

                    localAssetIngestionEvent->setMetadataContent(imageMetaDataContent);

                    try
                    {
                        handleLocalAssetIngestionEvent (localAssetIngestionEvent);
                    }
                    catch(runtime_error e)
                    {
                        generatedFrameIngestionFailed = true;

                        _logger->error(__FILEREF__ + "handleLocalAssetIngestionEvent failed"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", exception: " + e.what()
                        );
                    }
                    catch(exception e)
                    {
                        generatedFrameIngestionFailed = true;

                        _logger->error(__FILEREF__ + "handleLocalAssetIngestionEvent failed"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", exception: " + e.what()
                        );
                    }

//                    shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
//                    _multiEventsSet->addEvent(event);
//
//                    _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
//                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
//                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
//                        + ", getEventKey().first: " + to_string(event->getEventKey().first)
//                        + ", getEventKey().second: " + to_string(event->getEventKey().second));
                }
            }
        }
        
        if (generatedFrameIngestionFailed)
        {
            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
                + ", _encodingItem->_encodingJobKey: " + to_string(multiLocalAssetIngestionEvent->getEncodingJobKey())
                + ", _encodingItem->_ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
            );

            int64_t mediaItemKey = -1;
            int64_t encodedPhysicalPathKey = -1;
            // PunctualError is used because, in case it always happens, the encoding will never reach a final state
            int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                    multiLocalAssetIngestionEvent->getEncodingJobKey(), 
                    MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                    mediaItemKey, encodedPhysicalPathKey,
                    multiLocalAssetIngestionEvent->getIngestionJobKey());

            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
                + ", _encodingItem->_encodingJobKey: " + to_string(multiLocalAssetIngestionEvent->getEncodingJobKey())
                + ", _encodingItem->_ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
                + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
            );
        }
        else
        {
            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob NoError"
                + ", _encodingItem->_encodingJobKey: " + to_string(multiLocalAssetIngestionEvent->getEncodingJobKey())
                + ", _encodingItem->_ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
            );

            int64_t mediaItemKey = -1;
            int64_t encodedPhysicalPathKey = -1;
            _mmsEngineDBFacade->updateEncodingJob (
                multiLocalAssetIngestionEvent->getEncodingJobKey(), 
                MMSEngineDBFacade::EncodingError::NoError,
                mediaItemKey, encodedPhysicalPathKey,
                multiLocalAssetIngestionEvent->getIngestionJobKey());
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "handleMultiLocalAssetIngestionEvent failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
            + ", e.what(): " + e.what()
        );
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (multiLocalAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_IngestionFailure,
            e.what(), "" //ProcessorMMS
        );

        bool exceptionInCaseOfError = false;
        
        for(vector<string>::iterator it = generatedFramesFileNames.begin(); 
                it != generatedFramesFileNames.end(); ++it) 
        {
            string workspaceIngestionBinaryPathName = workspaceIngestionRepository + "/" + *it;
            
            _logger->info(__FILEREF__ + "Remove file"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
                + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
            );
            FileIO::remove(workspaceIngestionBinaryPathName, exceptionInCaseOfError);
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "handleMultiLocalAssetIngestionEvent failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
        );
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (multiLocalAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_IngestionFailure,
            e.what(), "" //ProcessorMMS
        );

        bool exceptionInCaseOfError = false;
        
        for(vector<string>::iterator it = generatedFramesFileNames.begin(); 
                it != generatedFramesFileNames.end(); ++it) 
        {
            string workspaceIngestionBinaryPathName = workspaceIngestionRepository + "/" + *it;
            
            _logger->info(__FILEREF__ + "Remove file"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
                + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
            );
            FileIO::remove(workspaceIngestionBinaryPathName, exceptionInCaseOfError);
        }
        
        throw e;
    }
    
}

void MMSEngineProcessor::generateAndIngestFrames(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {
        string field;
        
        if (dependencies.size() != 1)
        {
            string errorMessage = __FILEREF__ + "No video found"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        int periodInSeconds;
        double startTimeInSeconds;
        int maxFramesNumber;
        string videoFilter;
        bool mjpeg;
        int imageWidth;
        int imageHeight;
        string imageFileName;
        int64_t sourcePhysicalPathKey;
        string sourcePhysicalPath;
        int64_t durationInMilliSeconds;
        fillGenerateFramesParameters(
                workspace,
                ingestionJobKey,
                ingestionType,
                parametersRoot,
                dependencies,
                
                periodInSeconds, startTimeInSeconds,
                maxFramesNumber, videoFilter,
                mjpeg, imageWidth, imageHeight, imageFileName,
                sourcePhysicalPathKey, sourcePhysicalPath, durationInMilliSeconds);
        
        /*
        int periodInSeconds = -1;
        if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
        {
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames)
        {
            field = "PeriodInSeconds";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            periodInSeconds = parametersRoot.get(field, "XXX").asInt();
        }
        else // if (ingestionType == MMSEngineDBFacade::IngestionType::IFrames || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
        {
            
        }
            
        double startTimeInSeconds = 0;
        if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
        {
            field = "InstantInSeconds";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                startTimeInSeconds = parametersRoot.get(field, "XXX").asDouble();
            }
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::IFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
        {
            field = "StartTimeInSeconds";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                startTimeInSeconds = parametersRoot.get(field, "XXX").asDouble();
            }
        }

        int maxFramesNumber = -1;
        if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
        {
            maxFramesNumber = 1;
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::IFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
        {
            field = "MaxFramesNumber";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                maxFramesNumber = parametersRoot.get(field, "XXX").asInt();
            }
        }

        string videoFilter;
        if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
        {
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames)
        {
            videoFilter = "PeriodicFrame";
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::IFrames)
        {
            videoFilter = "All-I-Frames";
        }

        bool mjpeg;
        if (ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
        {
            mjpeg = true;
        }
        else
        {
            mjpeg = false;
        }

        int width = -1;
        field = "Width";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            width = parametersRoot.get(field, "XXX").asInt();
        }

        int height = -1;
        field = "Height";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            height = parametersRoot.get(field, "XXX").asInt();
        }

        int64_t sourceMediaItemKey;
        int64_t sourcePhysicalPathKey;
        string sourcePhysicalPath;
        pair<int64_t,Validator::DependencyType>& keyAndDependencyType = dependencies.back();
        if (keyAndDependencyType.second == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey = keyAndDependencyType.first;

            sourcePhysicalPathKey = -1;
            int64_t encodingProfileKey = -1;
            pair<int64_t,string> physicalPathKeyAndPhysicalPath 
                    = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);
            
            int64_t localPhysicalPathKey;
            tie(localPhysicalPathKey,sourcePhysicalPath) = physicalPathKeyAndPhysicalPath;
        }
        else
        {
            sourcePhysicalPathKey = keyAndDependencyType.first;
            
            bool warningIfMissing = false;
            pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey, warningIfMissing);
    
            MMSEngineDBFacade::ContentType localContentType;
            tie(sourceMediaItemKey,localContentType)
                    = mediaItemKeyContentTypeAndAvgFrameRate;
            
            sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
        }

        int64_t durationInMilliSeconds;
        int videoWidth;
        int videoHeight;
        try
        {
            long bitRate;
            string videoCodecName;
            string videoProfile;
            string videoAvgFrameRate;
            long videoBitRate;
            string audioCodecName;
            long audioSampleRate;
            int audioChannels;
            long audioBitRate;
        
            tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
                videoDetails = _mmsEngineDBFacade->getVideoDetails(sourceMediaItemKey, sourcePhysicalPathKey);
            
            tie(durationInMilliSeconds, bitRate,
                videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
                audioCodecName, audioSampleRate, audioChannels, audioBitRate) = videoDetails;
        }
        catch(runtime_error e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (durationInMilliSeconds < startTimeInSeconds * 1000)
        {
            string errorMessage = __FILEREF__ + "Frame was not generated because instantInSeconds is bigger than the video duration"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                    + ", startTimeInSeconds: " + to_string(startTimeInSeconds)
                    + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        string sourceFileName;
        field = "SourceFileName";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            sourceFileName = parametersRoot.get(field, "XXX").asString();
        }

        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
                workspace);

        string localSourceFileName;
        // string textToBeReplaced;
        // string textToReplace;
        {
            localSourceFileName = to_string(ingestionJobKey) + ".jpg";
            // size_t extensionIndex = sourceFileName.find_last_of(".");
            // if (extensionIndex != string::npos)
            //    temporaryFileName.append(sourceFileName.substr(extensionIndex))

            // textToBeReplaced = to_string(ingestionJobKey) + "_source";
            // textToReplace = sourceFileName.substr(0, extensionIndex);
        }
        */
        
        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
                workspace);

        FFMpeg ffmpeg (_configuration, _logger);

        vector<string> generatedFramesFileNames = ffmpeg.generateFramesToIngest(
                ingestionJobKey,
                0,  // encodingJobKey
                workspaceIngestionRepository,
                imageFileName,
                startTimeInSeconds,
                maxFramesNumber,
                videoFilter,
                periodInSeconds,
                mjpeg,
                imageWidth, 
                imageHeight,
                sourcePhysicalPath,
                durationInMilliSeconds
        );

        _logger->info(__FILEREF__ + "generateFramesToIngest done"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", generatedFramesFileNames.size: " + to_string(generatedFramesFileNames.size())
        );
        
        if (generatedFramesFileNames.size() == 0)
        {
            MMSEngineDBFacade::IngestionStatus newIngestionStatus 
                    = MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            _mmsEngineDBFacade->updateIngestionJob (
                ingestionJobKey, 
                newIngestionStatus, errorMessage, processorMMS);
        }
        else
        {
            // we have one ingestion job row and many generatd frames to be ingested
            // We want to update the ingestion row just once at the end in case of success
            // or when an error happens.
            // To do this we will add a field in the localAssetIngestionEvent structure (ingestionRowToBeUpdatedAsSuccess)
            // and we will set it to false but the last frame that we will set to true
            // In case of error, handleLocalAssetIngestionEvent will update ingestion row
            // and we will not call anymore handleLocalAssetIngestionEvent for the next frames
            // When I say 'update the ingestion row', it's not just the update but it is also
            // manageIngestionJobStatusUpdate
            bool generatedFrameIngestionFailed = false;

            for(vector<string>::iterator it = generatedFramesFileNames.begin(); 
                    it != generatedFramesFileNames.end(); ++it) 
            {
                string generatedFrameFileName = *it;

                if (generatedFrameIngestionFailed)
                {
                    string workspaceIngestionBinaryPathName;

                    workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace);
                    workspaceIngestionBinaryPathName
                            .append("/")
                            .append(generatedFrameFileName)
                            ;

                    _logger->info(__FILEREF__ + "Remove file"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                    );
                    FileIO::remove(workspaceIngestionBinaryPathName);
                }
                else
                {
                    _logger->info(__FILEREF__ + "Generated Frame to ingest"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", generatedFrameFileName: " + generatedFrameFileName
                        // + ", textToBeReplaced: " + textToBeReplaced
                        // + ", textToReplace: " + textToReplace
                    );

                    string fileFormat;
                    size_t extensionIndex = generatedFrameFileName.find_last_of(".");
                    if (extensionIndex == string::npos)
                    {
                        string errorMessage = __FILEREF__ + "No fileFormat (extension of the file) found in generatedFileName"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", generatedFrameFileName: " + generatedFrameFileName
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    fileFormat = generatedFrameFileName.substr(extensionIndex + 1);

        //            if (mmsSourceFileName.find(textToBeReplaced) != string::npos)
        //                mmsSourceFileName.replace(mmsSourceFileName.find(textToBeReplaced), textToBeReplaced.length(), textToReplace);

                    _logger->info(__FILEREF__ + "Generated Frame to ingest"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", new generatedFrameFileName: " + generatedFrameFileName
                        + ", fileFormat: " + fileFormat
                    );

                    string imageMetaDataContent = generateMediaMetadataToIngest(
                            ingestionJobKey,
                            // mjpeg,
                            fileFormat,
                            parametersRoot
                    );

                    {
                        // shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                        //        ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);
                        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent 
                                = make_shared<LocalAssetIngestionEvent>();

                        localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
                        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
                        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

                        localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
                        localAssetIngestionEvent->setIngestionSourceFileName(generatedFrameFileName);
                        // localAssetIngestionEvent->setMMSSourceFileName(mmsSourceFileName);
                        localAssetIngestionEvent->setMMSSourceFileName(generatedFrameFileName);
                        localAssetIngestionEvent->setWorkspace(workspace);
                        localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
                        localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(
                            it + 1 == generatedFramesFileNames.end() ? true : false);

                        localAssetIngestionEvent->setMetadataContent(imageMetaDataContent);

                        try
                        {
                            handleLocalAssetIngestionEvent (localAssetIngestionEvent);
                        }
                        catch(runtime_error e)
                        {
                            generatedFrameIngestionFailed = true;

                            _logger->error(__FILEREF__ + "handleLocalAssetIngestionEvent failed"
                                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                + ", exception: " + e.what()
                            );
                        }
                        catch(exception e)
                        {
                            generatedFrameIngestionFailed = true;

                            _logger->error(__FILEREF__ + "handleLocalAssetIngestionEvent failed"
                                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                + ", exception: " + e.what()
                            );
                        }

                        /*
                        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
                        _multiEventsSet->addEvent(event);

                        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", getEventKey().first: " + to_string(event->getEventKey().first)
                            + ", getEventKey().second: " + to_string(event->getEventKey().second));
                        */
                    }
                }
            }
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::manageGenerateFramesTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() != 1)
        {
            string errorMessage = __FILEREF__ + "Wrong number of dependencies"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        MMSEngineDBFacade::EncodingPriority encodingPriority;
        string field = "EncodingPriority";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            encodingPriority = 
                    static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
        }
        else
        {
            encodingPriority =
                MMSEngineDBFacade::toEncodingPriority(parametersRoot.get(field, "XXX").asString());
        }

        int periodInSeconds;
        double startTimeInSeconds;
        int maxFramesNumber;
        string videoFilter;
        bool mjpeg;
        int imageWidth;
        int imageHeight;
        string imageFileName;
        int64_t sourcePhysicalPathKey;
        string sourcePhysicalPath;
        int64_t durationInMilliSeconds;
        fillGenerateFramesParameters(
                workspace,
                ingestionJobKey,
                ingestionType,
                parametersRoot,
                dependencies,
                
                periodInSeconds, startTimeInSeconds,
                maxFramesNumber, videoFilter,
                mjpeg, imageWidth, imageHeight, imageFileName,
                sourcePhysicalPathKey, sourcePhysicalPath,
                durationInMilliSeconds);

        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
                workspace);

        _mmsEngineDBFacade->addEncoding_GenerateFramesJob (
                ingestionJobKey, encodingPriority,
                workspaceIngestionRepository, imageFileName, 
                startTimeInSeconds, maxFramesNumber, 
                videoFilter, periodInSeconds, 
                mjpeg, imageWidth, imageHeight,
                sourcePhysicalPathKey,
                durationInMilliSeconds
                );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "manageOverlayTextOnVideoTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "manageOverlayTextOnVideoTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::fillGenerateFramesParameters(
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    MMSEngineDBFacade::IngestionType ingestionType,
    Json::Value parametersRoot,
    vector<pair<int64_t,Validator::DependencyType>>& dependencies,
        
    int& periodInSeconds, double& startTimeInSeconds,
    int& maxFramesNumber, string& videoFilter,
    bool& mjpeg, int& imageWidth, int& imageHeight,
    string& imageFileName,
    int64_t& sourcePhysicalPathKey, string& sourcePhysicalPath,
    int64_t& durationInMilliSeconds
)
{
    try
    {
        string field;
        
        periodInSeconds = -1;
        if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
        {
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames)
        {
            field = "PeriodInSeconds";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            periodInSeconds = parametersRoot.get(field, "XXX").asInt();
        }
        else // if (ingestionType == MMSEngineDBFacade::IngestionType::IFrames || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
        {
            
        }
            
        startTimeInSeconds = 0;
        if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
        {
            field = "InstantInSeconds";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                startTimeInSeconds = parametersRoot.get(field, "XXX").asDouble();
            }
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::IFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
        {
            field = "StartTimeInSeconds";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                startTimeInSeconds = parametersRoot.get(field, "XXX").asDouble();
            }
        }

        maxFramesNumber = -1;
        if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
        {
            maxFramesNumber = 1;
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::IFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
        {
            field = "MaxFramesNumber";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                maxFramesNumber = parametersRoot.get(field, "XXX").asInt();
            }
        }

        if (ingestionType == MMSEngineDBFacade::IngestionType::Frame)
        {
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::PeriodicalFrames)
        {
            videoFilter = "PeriodicFrame";
        }
        else if (ingestionType == MMSEngineDBFacade::IngestionType::IFrames)
        {
            videoFilter = "All-I-Frames";
        }

        if (ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByPeriodicalFrames
                || ingestionType == MMSEngineDBFacade::IngestionType::MotionJPEGByIFrames)
        {
            mjpeg = true;
        }
        else
        {
            mjpeg = false;
        }

        int width = -1;
        field = "Width";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            width = parametersRoot.get(field, "XXX").asInt();
        }

        int height = -1;
        field = "Height";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            height = parametersRoot.get(field, "XXX").asInt();
        }

        int64_t sourceMediaItemKey;
        // int64_t sourcePhysicalPathKey;
        // string sourcePhysicalPath;
        pair<int64_t,Validator::DependencyType>& keyAndDependencyType = dependencies.back();
        if (keyAndDependencyType.second == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey = keyAndDependencyType.first;

            sourcePhysicalPathKey = -1;
            int64_t encodingProfileKey = -1;
            pair<int64_t,string> physicalPathKeyAndPhysicalPath 
                    = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);
            
            int64_t localPhysicalPathKey;
            tie(localPhysicalPathKey,sourcePhysicalPath) = physicalPathKeyAndPhysicalPath;
        }
        else
        {
            sourcePhysicalPathKey = keyAndDependencyType.first;
            
            bool warningIfMissing = false;
            pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey, warningIfMissing);
    
            MMSEngineDBFacade::ContentType localContentType;
            tie(sourceMediaItemKey,localContentType)
                    = mediaItemKeyContentTypeAndAvgFrameRate;
            
            sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
       }

        int videoWidth;
        int videoHeight;
        try
        {
            long bitRate;
            string videoCodecName;
            string videoProfile;
            string videoAvgFrameRate;
            long videoBitRate;
            string audioCodecName;
            long audioSampleRate;
            int audioChannels;
            long audioBitRate;
        
            tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
                videoDetails = _mmsEngineDBFacade->getVideoDetails(sourceMediaItemKey, sourcePhysicalPathKey);
            
            tie(durationInMilliSeconds, bitRate,
                videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
                audioCodecName, audioSampleRate, audioChannels, audioBitRate) = videoDetails;
        }
        catch(runtime_error e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        imageWidth = width == -1 ? videoWidth : width;
        imageHeight = height == -1 ? videoHeight : height;

        if (durationInMilliSeconds < startTimeInSeconds * 1000)
        {
            string errorMessage = __FILEREF__ + "Frame was not generated because instantInSeconds is bigger than the video duration"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                    + ", startTimeInSeconds: " + to_string(startTimeInSeconds)
                    + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        /*
        string sourceFileName;
        field = "SourceFileName";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            sourceFileName = parametersRoot.get(field, "XXX").asString();
        }

        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
                workspace);
        */

        // string textToBeReplaced;
        // string textToReplace;
        {
            imageFileName = to_string(ingestionJobKey) + /* "_source" + */ ".jpg";
            /*
            size_t extensionIndex = sourceFileName.find_last_of(".");
            if (extensionIndex != string::npos)
                temporaryFileName.append(sourceFileName.substr(extensionIndex));
            */

            // textToBeReplaced = to_string(ingestionJobKey) + "_source";
            // textToReplace = sourceFileName.substr(0, extensionIndex);
        }    
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "fillGenerateFramesParameters failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "fillGenerateFramesParameters failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::generateAndIngestSlideshow(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No images found"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        MMSEngineDBFacade::ContentType slideshowContentType;
        bool slideshowContentTypeInitialized = false;
        vector<string> sourcePhysicalPaths;
        
        for (pair<int64_t,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            // int64_t encodingProfileKey = -1;
            // string sourcePhysicalPath = _mmsStorage->getPhysicalPath(keyAndDependencyType.first, encodingProfileKey);

            int64_t sourceMediaItemKey;
            int64_t sourcePhysicalPathKey;
            string sourcePhysicalPath;
            if (keyAndDependencyType.second == Validator::DependencyType::MediaItemKey)
            {
                sourceMediaItemKey = keyAndDependencyType.first;

                sourcePhysicalPathKey = -1;
                int64_t encodingProfileKey = -1;
                pair<int64_t,string> physicalPathKeyAndPhysicalPath 
                        = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);

                int64_t localPhysicalPathKey;
                tie(localPhysicalPathKey,sourcePhysicalPath) = physicalPathKeyAndPhysicalPath;
            }
            else
            {
                sourcePhysicalPathKey = keyAndDependencyType.first;

                bool warningIfMissing = false;
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate =
                    _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        sourcePhysicalPathKey, warningIfMissing);

                MMSEngineDBFacade::ContentType localContentType;
                tie(sourceMediaItemKey,localContentType)
                        = mediaItemKeyContentTypeAndAvgFrameRate;

                sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
            }
            
            sourcePhysicalPaths.push_back(sourcePhysicalPath);
            
            bool warningIfMissing = false;
            
            MMSEngineDBFacade::ContentType contentType = _mmsEngineDBFacade->getMediaItemKeyDetails(
                sourceMediaItemKey, warningIfMissing);
            
            if (!slideshowContentTypeInitialized)
            {
                slideshowContentType = contentType;
                if (slideshowContentType != MMSEngineDBFacade::ContentType::Image)
                {
                    string errorMessage = __FILEREF__ + "It is not possible to build a slideshow with a media that is not an Image"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", slideshowContentType: " + MMSEngineDBFacade::toString(slideshowContentType)
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            else
            {
                if (slideshowContentType != contentType)
                {
                    string errorMessage = __FILEREF__ + "Not all the References have the same ContentType"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                            + ", slideshowContentType: " + MMSEngineDBFacade::toString(slideshowContentType)
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        int durationOfEachSlideInSeconds = 5;
        string field = "DurationOfEachSlideInSeconds";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            durationOfEachSlideInSeconds = parametersRoot.get(field, "XXX").asInt();
        }

        int outputFrameRate = 30;
        field = "OutputFrameRate";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            outputFrameRate = parametersRoot.get(field, "XXX").asInt();
        }

        string sourceFileName;
        field = "SourceFileName";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            sourceFileName = parametersRoot.get(field, "XXX").asString();
        }

        string fileFormat = "mp4";
        
        string localSourceFileName = to_string(ingestionJobKey)
                // + "_source"
                + "." + fileFormat
                ;
        /*
        size_t extensionIndex = sourceFileName.find_last_of(".");
        if (extensionIndex != string::npos)
            localSourceFileName.append(sourceFileName.substr(extensionIndex));
        */
        
        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
            workspace);
        string slideshowMediaPathName = workspaceIngestionRepository + "/" 
                + localSourceFileName;
        
        FFMpeg ffmpeg (_configuration, _logger);
        ffmpeg.generateSlideshowMediaToIngest(ingestionJobKey, 
                sourcePhysicalPaths, durationOfEachSlideInSeconds, outputFrameRate,
                slideshowMediaPathName);

        _logger->info(__FILEREF__ + "generateSlideshowMediaToIngest done"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", slideshowMediaPathName: " + slideshowMediaPathName
        );
                            
        string mediaMetaDataContent = generateMediaMetadataToIngest(
                ingestionJobKey,
                // true,
                fileFormat,
                parametersRoot
        );

        {
            shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                    ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

            localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
            localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
            localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

            localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
            localAssetIngestionEvent->setIngestionSourceFileName(localSourceFileName);
            localAssetIngestionEvent->setMMSSourceFileName(localSourceFileName);
            localAssetIngestionEvent->setWorkspace(workspace);
            localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
            localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

            // to manage a ffmpeg bug generating a corrupted/wrong avgFrameRate, we will
            // force the slideshow file to have the avgFrameRate as specified in the parameter
            localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate) + "/1");            

            localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

            shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
            _multiEventsSet->addEvent(event);

            _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", getEventKey().first: " + to_string(event->getEventKey().first)
                + ", getEventKey().second: " + to_string(event->getEventKey().second));
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestSlideshow failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestSlideshow failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::generateAndIngestConcatenation(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() < 2)
        {
            string errorMessage = __FILEREF__ + "No enough media to be concatenated"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        MMSEngineDBFacade::ContentType concatContentType;
        bool concatContentTypeInitialized = false;
        vector<string> sourcePhysicalPaths;
        string forcedAvgFrameRate;
        
        for (pair<int64_t,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            // int64_t encodingProfileKey = -1;
            // string sourcePhysicalPath = _mmsStorage->getPhysicalPath(keyAndDependencyType.first, encodingProfileKey);

            int64_t sourceMediaItemKey;
            int64_t sourcePhysicalPathKey;
            string sourcePhysicalPath;
            if (keyAndDependencyType.second == Validator::DependencyType::MediaItemKey)
            {
                sourceMediaItemKey = keyAndDependencyType.first;

                sourcePhysicalPathKey = -1;
                int64_t encodingProfileKey = -1;
                pair<int64_t,string> physicalPathInfo = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);
                
                tie(sourcePhysicalPathKey,sourcePhysicalPath) = physicalPathInfo;
            }
            else
            {
                sourcePhysicalPathKey = keyAndDependencyType.first;

                bool warningIfMissing = false;
                pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate =
                    _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        sourcePhysicalPathKey, warningIfMissing);

                MMSEngineDBFacade::ContentType localContentType;
                tie(sourceMediaItemKey,localContentType)
                        = mediaItemKeyContentTypeAndAvgFrameRate;

                sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
            }

            sourcePhysicalPaths.push_back(sourcePhysicalPath);
            
            bool warningIfMissing = false;
            pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate 
                    = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        sourcePhysicalPathKey, warningIfMissing);
            
            MMSEngineDBFacade::ContentType contentType;
            {
                int64_t localMediaItemKey;
                tie(localMediaItemKey, contentType) = mediaItemKeyContentTypeAndAvgFrameRate;                
            }
            
            if (!concatContentTypeInitialized)
            {
                concatContentType = contentType;
                if (concatContentType != MMSEngineDBFacade::ContentType::Video
                        && concatContentType != MMSEngineDBFacade::ContentType::Audio)
                {
                    string errorMessage = __FILEREF__ + "It is not possible to concatenate a media that is not video or audio"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", concatContentType: " + MMSEngineDBFacade::toString(concatContentType)
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            else
            {
                if (concatContentType != contentType)
                {
                    string errorMessage = __FILEREF__ + "Not all the References have the same ContentType"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                            + ", concatContentType: " + MMSEngineDBFacade::toString(concatContentType)
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            
            // to manage a ffmpeg bug generating a corrupted/wrong avgFrameRate, we will
            // force the concat file to have the same avgFrameRate of the source media
            if (concatContentType == MMSEngineDBFacade::ContentType::Video
                    && forcedAvgFrameRate == "")
            {
                int64_t localDurationInMilliSeconds;
                long localBitRate;
                string localVideoCodecName;
                string localVideoProfile;
                int localVideoWidth;
                int localVideoHeight;
                // string localVideoAvgFrameRate;
                long localVideoBitRate;
                string localAudioCodecName;
                long localAudioSampleRate;
                int localAudioChannels;
                long localAudioBitRate;
                
                tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> videoDetails 
                    = _mmsEngineDBFacade->getVideoDetails(sourceMediaItemKey, sourcePhysicalPathKey);

                tie(localDurationInMilliSeconds, localBitRate, localVideoCodecName,
                    localVideoProfile, localVideoWidth, localVideoHeight, forcedAvgFrameRate,
                    localVideoBitRate, localAudioCodecName, localAudioSampleRate, localAudioChannels, localAudioBitRate)
                    = videoDetails;
            }
        }

        // this is a concat, so destination file name shall have the same
        // extension as the source file name
        string fileFormat;
        size_t extensionIndex = sourcePhysicalPaths.front().find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileFormat (extension of the file) found in sourcePhysicalPath"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", sourcePhysicalPaths.front(): " + sourcePhysicalPaths.front()
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        fileFormat = sourcePhysicalPaths.front().substr(extensionIndex + 1);

        string localSourceFileName = to_string(ingestionJobKey)
                + "_concat"
                + "." + fileFormat // + "_source"
                ;
        
        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
            workspace);
        string concatenatedMediaPathName = workspaceIngestionRepository + "/" 
                + localSourceFileName;
        
        FFMpeg ffmpeg (_configuration, _logger);
        ffmpeg.generateConcatMediaToIngest(ingestionJobKey, sourcePhysicalPaths, concatenatedMediaPathName);

        _logger->info(__FILEREF__ + "generateConcatMediaToIngest done"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", concatenatedMediaPathName: " + concatenatedMediaPathName
        );
                
        string mediaMetaDataContent = generateMediaMetadataToIngest(
                ingestionJobKey,
                // concatContentType == MMSEngineDBFacade::ContentType::Video ? true : false,
                fileFormat,
                parametersRoot
        );

        {
            shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                    ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

            localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
            localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
            localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

            localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
            localAssetIngestionEvent->setIngestionSourceFileName(localSourceFileName);
            localAssetIngestionEvent->setMMSSourceFileName(localSourceFileName);
            localAssetIngestionEvent->setWorkspace(workspace);
            localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);            
            localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

            // to manage a ffmpeg bug generating a corrupted/wrong avgFrameRate, we will
            // force the concat file to have the same avgFrameRate of the source media
            if (forcedAvgFrameRate != "" && concatContentType == MMSEngineDBFacade::ContentType::Video)
                localAssetIngestionEvent->setForcedAvgFrameRate(forcedAvgFrameRate);            


            localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

            shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
            _multiEventsSet->addEvent(event);

            _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", getEventKey().first: " + to_string(event->getEventKey().first)
                + ", getEventKey().second: " + to_string(event->getEventKey().second));
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestConcatenation failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestConcatenation failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::generateAndIngestCutMedia(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any media to be cut"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        int64_t sourceMediaItemKey;
        int64_t sourcePhysicalPathKey;
        string sourcePhysicalPath;
        pair<int64_t,Validator::DependencyType>& keyAndDependencyType = dependencies.back();
        if (keyAndDependencyType.second == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey = keyAndDependencyType.first;

            sourcePhysicalPathKey = -1;
            int64_t encodingProfileKey = -1;
            pair<int64_t,string> physicalPathKeyAndPhysicalPath
                    = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);
            tie(sourcePhysicalPathKey,sourcePhysicalPath) = physicalPathKeyAndPhysicalPath;
        }
        else
        {
            sourcePhysicalPathKey = keyAndDependencyType.first;
            
            bool warningIfMissing = false;
            pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            tie(sourceMediaItemKey,localContentType)
                    = mediaItemKeyContentTypeAndAvgFrameRate;

            sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
        }

        bool warningIfMissing = false;

        MMSEngineDBFacade::ContentType contentType = _mmsEngineDBFacade->getMediaItemKeyDetails(
            sourceMediaItemKey, warningIfMissing);

        if (contentType != MMSEngineDBFacade::ContentType::Video
                && contentType != MMSEngineDBFacade::ContentType::Audio)
        {
            string errorMessage = __FILEREF__ + "It is not possible to cut a media that is not video or audio"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        double startTimeInSeconds;
        string field = "StartTimeInSeconds";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        startTimeInSeconds = parametersRoot.get(field, "XXX").asDouble();

        double endTimeInSeconds = -1;
        field = "EndTimeInSeconds";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            endTimeInSeconds = parametersRoot.get(field, "XXX").asDouble();
        }
        
        int framesNumber = -1;
        field = "FramesNumber";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            framesNumber = parametersRoot.get(field, "XXX").asInt();
        }
        
        if (endTimeInSeconds == -1 && framesNumber == -1)
        {
            string errorMessage = __FILEREF__ + "Both 'EndTimeInSeconds' and 'FramesNumber' fields are not present or it is null"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        // to manage a ffmpeg bug generating a corrupted/wrong avgFrameRate, we will
        // force the cut file to have the same avgFrameRate of the source media
        string forcedAvgFrameRate;
        int64_t durationInMilliSeconds;
        try
        {
            if (contentType == MMSEngineDBFacade::ContentType::Video)
            {
                int videoWidth;
                int videoHeight;
                long bitRate;
                string videoCodecName;
                string videoProfile;
                long videoBitRate;
                string audioCodecName;
                long audioSampleRate;
                int audioChannels;
                long audioBitRate;

                tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
                    videoDetails = _mmsEngineDBFacade->getVideoDetails(sourceMediaItemKey, sourcePhysicalPathKey);

                tie(durationInMilliSeconds, bitRate,
                    videoCodecName, videoProfile, videoWidth, videoHeight, forcedAvgFrameRate, videoBitRate,
                    audioCodecName, audioSampleRate, audioChannels, audioBitRate) = videoDetails;
            }
            else if (contentType == MMSEngineDBFacade::ContentType::Audio)
            {
                string codecName;
                long bitRate;
                long sampleRate;
                int channels;

                tuple<int64_t,string,long,long,int> audioDetails = _mmsEngineDBFacade->getAudioDetails(
                    sourceMediaItemKey, sourcePhysicalPathKey);

                tie(durationInMilliSeconds, codecName, bitRate, sampleRate, channels) 
                        = audioDetails;
            }
        }
        catch(runtime_error e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (durationInMilliSeconds < startTimeInSeconds * 1000
                || (endTimeInSeconds != -1 && durationInMilliSeconds < endTimeInSeconds * 1000))
        {
            string errorMessage = __FILEREF__ + "Cut was not done because startTimeInSeconds or endTimeInSeconds is bigger than the video duration"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                    + ", startTimeInSeconds: " + to_string(startTimeInSeconds)
                    + ", endTimeInSeconds: " + to_string(endTimeInSeconds)
                    + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        // this is a cut so destination file name shall have the same
        // extension as the source file name
        string fileFormat;
        size_t extensionIndex = sourcePhysicalPath.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileFormat (extension of the file) found in sourcePhysicalPath"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", sourcePhysicalPath: " + sourcePhysicalPath
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        fileFormat = sourcePhysicalPath.substr(extensionIndex + 1);

        string localSourceFileName = to_string(ingestionJobKey)
                + "_cut"
                + "." + fileFormat // + "_source"
                ;
        
        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
                workspace);
        string cutMediaPathName = workspaceIngestionRepository + "/"
                + localSourceFileName;
        
        FFMpeg ffmpeg (_configuration, _logger);
        ffmpeg.generateCutMediaToIngest(ingestionJobKey, sourcePhysicalPath, 
                startTimeInSeconds, endTimeInSeconds, framesNumber,
                cutMediaPathName);

        _logger->info(__FILEREF__ + "generateCutMediaToIngest done"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", cutMediaPathName: " + cutMediaPathName
        );
        
        string mediaMetaDataContent = generateMediaMetadataToIngest(
                ingestionJobKey,
                // contentType == MMSEngineDBFacade::ContentType::Video ? true : false,
                fileFormat,
                parametersRoot
        );

        {
            shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                    ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

            localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
            localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
            localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

            localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
            localAssetIngestionEvent->setIngestionSourceFileName(localSourceFileName);
            localAssetIngestionEvent->setMMSSourceFileName(localSourceFileName);
            localAssetIngestionEvent->setWorkspace(workspace);
            localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
            localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
            // to manage a ffmpeg bug generating a corrupted/wrong avgFrameRate, we will
            // force the concat file to have the same avgFrameRate of the source media
            if (forcedAvgFrameRate != "" && contentType == MMSEngineDBFacade::ContentType::Video)
                localAssetIngestionEvent->setForcedAvgFrameRate(forcedAvgFrameRate);            

            localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

            shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
            _multiEventsSet->addEvent(event);

            _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", getEventKey().first: " + to_string(event->getEventKey().first)
                + ", getEventKey().second: " + to_string(event->getEventKey().second));
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestCutMedia failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestCutMedia failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::manageEncodeTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {        
        if (dependencies.size() != 1)
        {
            string errorMessage = __FILEREF__ + "Wrong media number to be encoded"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        // This task shall contain EncodingProfileKey or EncodingProfileLabel.
        // We cannot have EncodingProfilesSetKey because we replaced it with a GroupOfTasks
        //  having just EncodingProfileKey        
        
        string keyField = "EncodingProfileKey";
        int64_t encodingProfileKey = -1;
        string labelField = "EncodingProfileLabel";
        string encodingProfileLabel;
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, keyField))
        {
            encodingProfileKey = parametersRoot.get(keyField, "XXX").asInt64();
        }
        else if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, labelField))
        {
            encodingProfileLabel = parametersRoot.get(labelField, "XXX").asString();
        }
        else
        {
            string errorMessage = __FILEREF__ + "Both fields are not present or it is null"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", Field: " + keyField
                    + ", Field: " + labelField
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        string field = "EncodingPriority";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        MMSEngineDBFacade::EncodingPriority encodingPriority =
                MMSEngineDBFacade::toEncodingPriority(parametersRoot.get(field, "XXX").asString());

        int64_t sourceMediaItemKey;
        int64_t sourcePhysicalPathKey;
        pair<int64_t,Validator::DependencyType>& keyAndDependencyType = dependencies.back();
        if (keyAndDependencyType.second == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey = keyAndDependencyType.first;

            sourcePhysicalPathKey = -1;
        }
        else
        {
            sourcePhysicalPathKey = keyAndDependencyType.first;
            
            bool warningIfMissing = false;
            pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            tie(sourceMediaItemKey,localContentType)
                    = mediaItemKeyContentTypeAndAvgFrameRate;
        }
    
        if (encodingProfileKey == -1)
            _mmsEngineDBFacade->addEncodingJob (workspace->_workspaceKey, ingestionJobKey,
                encodingProfileLabel, sourceMediaItemKey, sourcePhysicalPathKey, encodingPriority);
        else
            _mmsEngineDBFacade->addEncodingJob (ingestionJobKey,
                encodingProfileKey, sourceMediaItemKey, sourcePhysicalPathKey, encodingPriority);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::manageOverlayImageOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() != 2)
        {
            string errorMessage = __FILEREF__ + "Wrong number of dependencies"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        MMSEngineDBFacade::EncodingPriority encodingPriority;
        string field = "EncodingPriority";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            encodingPriority = 
                    static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
        }
        else
        {
            encodingPriority =
                MMSEngineDBFacade::toEncodingPriority(parametersRoot.get(field, "XXX").asString());
        }

        field = "ImagePosition_X_InPixel";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string imagePosition_X_InPixel = parametersRoot.get(field, "XXX").asString();

        field = "ImagePosition_Y_InPixel";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string imagePosition_Y_InPixel = parametersRoot.get(field, "XXX").asString();

        int64_t sourceMediaItemKey_1;
        int64_t sourcePhysicalPathKey_1;
        pair<int64_t,Validator::DependencyType>& keyAndDependencyType_1 = dependencies[0];
        if (keyAndDependencyType_1.second == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey_1 = keyAndDependencyType_1.first;

            sourcePhysicalPathKey_1 = -1;
        }
        else
        {
            sourcePhysicalPathKey_1 = keyAndDependencyType_1.first;
            
            bool warningIfMissing = false;
            pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey_1, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            tie(sourceMediaItemKey_1,localContentType)
                    = mediaItemKeyContentTypeAndAvgFrameRate;
        }

        int64_t sourceMediaItemKey_2;
        int64_t sourcePhysicalPathKey_2;
        pair<int64_t,Validator::DependencyType>& keyAndDependencyType_2 = dependencies[1];
        if (keyAndDependencyType_2.second == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey_2 = keyAndDependencyType_2.first;

            sourcePhysicalPathKey_2 = -1;
        }
        else
        {
            sourcePhysicalPathKey_2 = keyAndDependencyType_2.first;
            
            bool warningIfMissing = false;
            pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey_2, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            tie(sourceMediaItemKey_2,localContentType)
                    = mediaItemKeyContentTypeAndAvgFrameRate;
        }

        _mmsEngineDBFacade->addEncoding_OverlayImageOnVideoJob (ingestionJobKey,
                sourceMediaItemKey_1, sourcePhysicalPathKey_1,
                sourceMediaItemKey_2, sourcePhysicalPathKey_2,
                imagePosition_X_InPixel, imagePosition_Y_InPixel,
                encodingPriority);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "manageOverlayImageOnVideoTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "manageOverlayImageOnVideoTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::manageOverlayTextOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() != 1)
        {
            string errorMessage = __FILEREF__ + "Wrong number of dependencies"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        MMSEngineDBFacade::EncodingPriority encodingPriority;
        string field = "EncodingPriority";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            encodingPriority = 
                    static_cast<MMSEngineDBFacade::EncodingPriority>(workspace->_maxEncodingPriority);
        }
        else
        {
            encodingPriority =
                MMSEngineDBFacade::toEncodingPriority(parametersRoot.get(field, "XXX").asString());
        }

        field = "Text";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string text = parametersRoot.get(field, "XXX").asString();

        string textPosition_X_InPixel;
        field = "TextPosition_X_InPixel";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            textPosition_X_InPixel = parametersRoot.get(field, "XXX").asString();
        }

        string textPosition_Y_InPixel;
        field = "TextPosition_Y_InPixel";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            textPosition_Y_InPixel = parametersRoot.get(field, "XXX").asString();
        }

        string fontType;
        field = "FontType";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            fontType = parametersRoot.get(field, "XXX").asString();
        }

        int fontSize = -1;
        field = "FontSize";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            fontSize = parametersRoot.get(field, -1).asInt();
        }

        string fontColor;
        field = "FontColor";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            fontColor = parametersRoot.get(field, "XXX").asString();
        }

        int textPercentageOpacity = -1;
        field = "TextPercentageOpacity";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            textPercentageOpacity = parametersRoot.get(field, -1).asInt();
        }

        bool boxEnable = false;
        field = "BoxEnable";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            boxEnable = parametersRoot.get(field, -1).asBool();
        }

        string boxColor;
        field = "BoxColor";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            boxColor = parametersRoot.get(field, "XXX").asString();
        }

        int boxPercentageOpacity = -1;
        field = "BoxPercentageOpacity";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            boxPercentageOpacity = parametersRoot.get(field, -1).asInt();
        }

        int64_t sourceMediaItemKey;
        int64_t sourcePhysicalPathKey;
        pair<int64_t,Validator::DependencyType>& keyAndDependencyType = dependencies[0];
        if (keyAndDependencyType.second == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey = keyAndDependencyType.first;

            sourcePhysicalPathKey = -1;
        }
        else
        {
            sourcePhysicalPathKey = keyAndDependencyType.first;
            
            bool warningIfMissing = false;
            pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyContentTypeAndAvgFrameRate =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            tie(sourceMediaItemKey,localContentType)
                    = mediaItemKeyContentTypeAndAvgFrameRate;
        }

        _mmsEngineDBFacade->addEncoding_OverlayTextOnVideoJob (
                ingestionJobKey, encodingPriority,
                
                sourceMediaItemKey, sourcePhysicalPathKey,
                text,
                textPosition_X_InPixel, textPosition_Y_InPixel,
                fontType, fontSize, fontColor, textPercentageOpacity,
                boxEnable, boxColor, boxPercentageOpacity
                );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "manageOverlayTextOnVideoTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "manageOverlayTextOnVideoTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::manageEmailNotification(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any IngestionJobKey in order to send an email"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string sIngestionJobJeyDependency;
        for (pair<int64_t,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            if (sIngestionJobJeyDependency == "")
                sIngestionJobJeyDependency = to_string(keyAndDependencyType.first);
            else
                sIngestionJobJeyDependency += (", " + to_string(keyAndDependencyType.first));
        }
        
        string field = "EmailAddress";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string emailAddress = parametersRoot.get(field, "XXX").asString();
    
        field = "Subject";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string subject = parametersRoot.get(field, "XXX").asString();

        field = "Message";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string message = parametersRoot.get(field, "XXX").asString();

        {
            string strToBeReplaced = "__INGESTIONJOBKEY__";
            string strToReplace = sIngestionJobJeyDependency;
            if (message.find(strToBeReplaced) != string::npos)
                message.replace(message.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
        }

        vector<string> emailBody;
        emailBody.push_back(message);
            
        EMailSender emailSender(_logger, _configuration);
        emailSender.sendEmail(emailAddress, "Task finished", emailBody);
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_TaskSuccess"
            + ", errorMessage: " + ""
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                MMSEngineDBFacade::IngestionStatus::End_TaskSuccess, 
                "", // errorMessage
                "" // ProcessorMMS
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "sendEmail failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), 
                "" // ProcessorMMS
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "sendEmail failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), 
                "" // ProcessorMMS
        );
        
        throw e;
    }
}

/*
void MMSEngineProcessor:: sendEmail(string to, string subject, vector<string>& emailBody)
{
    // curl --url 'smtps://smtp.gmail.com:465' --ssl-reqd   
    //      --mail-from 'giulianocatrambone@gmail.com' 
    //      --mail-rcpt 'giulianoc@catrasoftware.it'   
    //      --upload-file ~/tmp/1.txt 
    //      --user 'giulianocatrambone@gmail.com:XXXXXXXXXXXXX' 
    //      --insecure
    
    
    string emailServerURL = _emailProtocol + "://" + _emailServer + ":" + to_string(_emailPort);
    

    CURL *curl;
    CURLcode res = CURLE_OK;
    struct curl_slist *recipients = NULL;
    deque<string> emailLines;
  
    emailLines.push_back(string("From: <") + _emailFrom + ">" + "\r\n");
    emailLines.push_back(string("To: <") + to + ">" + "\r\n");
    emailLines.push_back(string("Subject: ") + subject + "\r\n");
    emailLines.push_back(string("Content-Type: text/html; charset=\"UTF-8\"") + "\r\n");
    emailLines.push_back("\r\n");   // empty line to divide headers from body, see RFC5322
    emailLines.insert(emailLines.end(), emailBody.begin(), emailBody.end());
    
    curl = curl_easy_init();

    if(curl) 
    {
        curl_easy_setopt(curl, CURLOPT_URL, emailServerURL.c_str());
        curl_easy_setopt(curl, CURLOPT_USERNAME, _emailUserName.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, _emailPassword.c_str());
        
//        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
//        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

//        * Note that this option isn't strictly required, omitting it will result
//         * in libcurl sending the MAIL FROM command with empty sender data. All
//         * autoresponses should have an empty reverse-path, and should be directed
//         * to the address in the reverse-path which triggered them. Otherwise,
//         * they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
//         * details.
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, _emailFrom.c_str());

//        * Add two recipients, in this particular case they correspond to the
//         * To: and Cc: addressees in the header, but they could be any kind of
//         * recipient. 
        recipients = curl_slist_append(recipients, to.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

//        * We're using a callback function to specify the payload (the headers and
//         * body of the message). You could just use the CURLOPT_READDATA option to
//         * specify a FILE pointer to read from.
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, MMSEngineProcessor::emailPayloadFeed);
        curl_easy_setopt(curl, CURLOPT_READDATA, &emailLines);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        // Send the message
        _logger->info(__FILEREF__ + "Sending email..."
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
        );
        res = curl_easy_perform(curl);

        // Check for errors
        if(res != CURLE_OK)
            _logger->error(__FILEREF__ + "curl_easy_perform() failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", curl_easy_strerror(res): " + curl_easy_strerror(res)
            );
        else
            _logger->info(__FILEREF__ + "Email sent successful"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            );

        // Free the list of recipients
        curl_slist_free_all(recipients);

//        * curl won't send the QUIT command until you call cleanup, so you should
//         * be able to re-use this connection for additional messages (setting
//         * CURLOPT_MAIL_FROM and CURLOPT_MAIL_RCPT as required, and calling
//         * curl_easy_perform() again. It may not be a good idea to keep the
//         * connection open for a very long time though (more than a few minutes
//         * may result in the server timing out the connection), and you do want to
//         * clean up in the end.
        curl_easy_cleanup(curl);
    }    
}

size_t MMSEngineProcessor:: emailPayloadFeed(void *ptr, size_t size, size_t nmemb, void *userp)
{
    deque<string>* pEmailLines = (deque<string>*) userp;
 
    if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) 
    {
        return 0;
    }
 
    if (pEmailLines->size() == 0)
        return 0; // no more lines
  
    string emailLine = pEmailLines->front();
    // cout << "emailLine: " << emailLine << endl;
 
    memcpy(ptr, emailLine.c_str(), emailLine.length());
    pEmailLines->pop_front();
 
    return emailLine.length();
}
*/

/*
string MMSEngineProcessor::generateImageMetadataToIngest(
        int64_t ingestionJobKey,
        bool mjpeg,
        string fileFormat,
        Json::Value frameRoot
)
{
    string title;
    string field = "title";
    if (_mmsEngineDBFacade->isMetadataPresent(frameRoot, field))
        title = frameRoot.get(field, "XXX").asString();
    
    string subTitle;
    field = "SubTitle";
    if (_mmsEngineDBFacade->isMetadataPresent(frameRoot, field))
        subTitle = frameRoot.get(field, "XXX").asString();

    string ingester;
    field = "Ingester";
    if (_mmsEngineDBFacade->isMetadataPresent(frameRoot, field))
        ingester = frameRoot.get(field, "XXX").asString();

    string keywords;
    field = "Keywords";
    if (_mmsEngineDBFacade->isMetadataPresent(frameRoot, field))
        keywords = frameRoot.get(field, "XXX").asString();

    string description;
    field = "Description";
    if (_mmsEngineDBFacade->isMetadataPresent(frameRoot, field))
        description = frameRoot.get(field, "XXX").asString();

    string logicalType;
    field = "LogicalType";
    if (_mmsEngineDBFacade->isMetadataPresent(frameRoot, field))
        logicalType = frameRoot.get(field, "XXX").asString();

    string contentProviderName;
    field = "ContentProviderName";
    if (_mmsEngineDBFacade->isMetadataPresent(frameRoot, field))
        contentProviderName = frameRoot.get(field, "XXX").asString();
    
    string territories;
    field = "Territories";
    if (_mmsEngineDBFacade->isMetadataPresent(frameRoot, field))
    {
        {
            Json::StreamWriterBuilder wbuilder;
            
            territories = Json::writeString(wbuilder, frameRoot[field]);
        }
    }
    
    string imageMetadata = string("")
        + "{"
            + "\"ContentType\": \"" + (mjpeg ? "video" : "image") + "\""
            + ", \"FileFormat\": \"" + fileFormat + "\""
            ;
    if (title != "")
        imageMetadata += ", \"Title\": \"" + title + "\"";
    if (subTitle != "")
        imageMetadata += ", \"SubTitle\": \"" + subTitle + "\"";
    if (ingester != "")
        imageMetadata += ", \"Ingester\": \"" + ingester + "\"";
    if (keywords != "")
        imageMetadata += ", \"Keywords\": \"" + keywords + "\"";
    if (description != "")
        imageMetadata += ", \"Description\": \"" + description + "\"";
    if (logicalType != "")
        imageMetadata += ", \"LogicalType\": \"" + logicalType + "\"";
    if (contentProviderName != "")
        imageMetadata += ", \"ContentProviderName\": \"" + contentProviderName + "\"";
    if (territories != "")
        imageMetadata += ", \"Territories\": \"" + territories + "\"";
                            
    imageMetadata +=
        string("}")
    ;
    
    _logger->info(__FILEREF__ + "Image metadata generated"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", imageMetadata: " + imageMetadata
            );

    return imageMetadata;
}
*/

string MMSEngineProcessor::generateMediaMetadataToIngest(
        int64_t ingestionJobKey,
        // bool video,
        string fileFormat,
        Json::Value parametersRoot
)
{
    /*
    string expectedContentType = (video ? "video" : "audio");
    string field = "ContentType";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        string contentType = parametersRoot.get(field, "XXX").asString();
        if (contentType != expectedContentType)
        {
            string errorMessage = string("Wrong contentType")
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", contentType: " + contentType
                + ", expectedContentType: " + expectedContentType
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else
    {
        parametersRoot[field] = expectedContentType;
    }
    */
    
    string field = "FileFormat";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        string fileFormatSpecifiedByUser = parametersRoot.get(field, "XXX").asString();
        if (fileFormatSpecifiedByUser != fileFormat)
        {
            string errorMessage = string("Wrong fileFormat")
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", fileFormatSpecifiedByUser: " + fileFormatSpecifiedByUser
                + ", fileFormat: " + fileFormat
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else
    {
        parametersRoot[field] = fileFormat;
    }

    // this scenario is for example for the Cut or Concat-Demux or Periodical-Frames
    // that generate a new content (or contents in case of Periodical-Frames)
    // and the Parameters json will contain the parameters
    // for the new content.
    // It will contain also parameters for the Cut or Concat-Demux or Periodical-Frames or ...,
    // we will leave there even because we know they will not be used by the
    // Add-Content task
    
    string mediaMetadata;
    {
        Json::StreamWriterBuilder wbuilder;
        mediaMetadata = Json::writeString(wbuilder, parametersRoot);
    }
                        
    _logger->info(__FILEREF__ + "Media metadata generated"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", mediaMetadata: " + mediaMetadata
            );

    return mediaMetadata;
}

void MMSEngineProcessor::handleCheckEncodingEvent ()
{
    vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> encodingItems;
        
    _mmsEngineDBFacade->getEncodingJobs(_processorMMS, encodingItems);

    _pActiveEncodingsManager->addEncodingItems(encodingItems);
}

void MMSEngineProcessor::handleContentRetentionEvent ()
{
    
    {
        _logger->info(__FILEREF__ + "Content Retention started"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
        );

        vector<pair<shared_ptr<Workspace>,int64_t>> mediaItemKeyToBeRemoved;
        bool moreRemoveToBeDone = true;

        while (moreRemoveToBeDone)
        {
            try
            {
                int maxMediaItemKeysNumber = 100;

                mediaItemKeyToBeRemoved.clear();
                _mmsEngineDBFacade->getExpiredMediaItemKeys(
                    _processorMMS, mediaItemKeyToBeRemoved, maxMediaItemKeysNumber);

                if (mediaItemKeyToBeRemoved.size() == 0)
                    moreRemoveToBeDone = false;
            }
            catch(runtime_error e)
            {
                _logger->error(__FILEREF__ + "getExpiredMediaItemKeys failed"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", exception: " + e.what()
                );

                // no throw since it is running in a detached thread
                // throw e;
                break;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "getExpiredMediaItemKeys failed"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", exception: " + e.what()
                );

                // no throw since it is running in a detached thread
                // throw e;
                break;
            }

            for (pair<shared_ptr<Workspace>,int64_t> workspaceAndMediaItemKey: mediaItemKeyToBeRemoved)
            {
                _logger->info(__FILEREF__ + "Removing because of Retention"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", workspace->_workspaceKey: " + to_string(workspaceAndMediaItemKey.first->_workspaceKey)
                    + ", workspace->_name: " + workspaceAndMediaItemKey.first->_name
                    + ", mediaItemKeyToBeRemoved: " + to_string(workspaceAndMediaItemKey.second)
                );

                try
                {
                    _mmsStorage->removeMediaItem(workspaceAndMediaItemKey.second);
                }
                catch(runtime_error e)
                {
                    _logger->error(__FILEREF__ + "_mmsStorage->removeMediaItem failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", workspace->_workspaceKey: " + to_string(workspaceAndMediaItemKey.first->_workspaceKey)
                        + ", workspace->_name: " + workspaceAndMediaItemKey.first->_name
                        + ", mediaItemKeyToBeRemoved: " + to_string(workspaceAndMediaItemKey.second)
                        + ", exception: " + e.what()
                    );

                    moreRemoveToBeDone = false;

                    break;
                    // no throw since it is running in a detached thread
                    // throw e;
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "_mmsStorage->removeMediaItem failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", workspace->_workspaceKey: " + to_string(workspaceAndMediaItemKey.first->_workspaceKey)
                        + ", workspace->_name: " + workspaceAndMediaItemKey.first->_name
                        + ", mediaItemKeyToBeRemoved: " + to_string(workspaceAndMediaItemKey.second)
                    );

                    moreRemoveToBeDone = false;

                    break;
                    // no throw since it is running in a detached thread
                    // throw e;
                }
            }
        }

        _logger->info(__FILEREF__ + "Content Retention finished"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
        );
    }

    {
        _logger->info(__FILEREF__ + "Staging Retention started"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", _mmsStorage->getStagingRootRepository(): " + _mmsStorage->getStagingRootRepository()
        );

        try
        {
            chrono::system_clock::time_point tpNow = chrono::system_clock::now();
    
            FileIO::DirectoryEntryType_t detDirectoryEntryType;
            shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (_mmsStorage->getStagingRootRepository());

            bool scanDirectoryFinished = false;
            while (!scanDirectoryFinished)
            {
                string directoryEntry;
                try
                {
                    string directoryEntry = FileIO::readDirectory (directory,
                        &detDirectoryEntryType);

//                    if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
//                        continue;

                    string pathName = _mmsStorage->getStagingRootRepository()
                            + directoryEntry;
                    chrono::system_clock::time_point tpLastModification =
                            FileIO:: getFileTime (pathName);
                    
                    int elapsedInHours = chrono::duration_cast<chrono::hours>(tpNow - tpLastModification).count();
                    double elapsedInDays =  elapsedInHours / 24;
                    if (elapsedInDays >= _stagingRetentionInDays)
                    {
                        if (detDirectoryEntryType == FileIO:: TOOLS_FILEIO_DIRECTORY) 
                        {
                            _logger->info(__FILEREF__ + "Removing staging directory because of Retention"
                                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                + ", pathName: " + pathName
                                + ", elapsedInDays: " + to_string(elapsedInDays)
                                + ", _stagingRetentionInDays: " + to_string(_stagingRetentionInDays)
                            );
                            
                            try
                            {
                                bool removeRecursively = true;

                                FileIO::removeDirectory(pathName, removeRecursively);
                            }
                            catch(runtime_error e)
                            {
                                _logger->warn(__FILEREF__ + "Error removing staging directory because of Retention"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", pathName: " + pathName
                                    + ", elapsedInDays: " + to_string(elapsedInDays)
                                    + ", _stagingRetentionInDays: " + to_string(_stagingRetentionInDays)
                                    + ", e.what(): " + e.what()
                                );
                            }
                            catch(exception e)
                            {
                                _logger->warn(__FILEREF__ + "Error removing staging directory because of Retention"
                                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                    + ", pathName: " + pathName
                                    + ", elapsedInDays: " + to_string(elapsedInDays)
                                    + ", _stagingRetentionInDays: " + to_string(_stagingRetentionInDays)
                                    + ", e.what(): " + e.what()
                                );
                            }
                        }
                        else
                        {
                            _logger->info(__FILEREF__ + "Removing staging file because of Retention"
                                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                + ", pathName: " + pathName
                                + ", elapsedInDays: " + to_string(elapsedInDays)
                                + ", _stagingRetentionInDays: " + to_string(_stagingRetentionInDays)
                            );
                            
                            bool exceptionInCaseOfError = false;

                            FileIO::remove(pathName, exceptionInCaseOfError);
                        }
                    }
                }
                catch(DirectoryListFinished e)
                {
                    scanDirectoryFinished = true;
                }
                catch(runtime_error e)
                {
                    string errorMessage = __FILEREF__ + "listing directory failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                           + ", e.what(): " + e.what()
                    ;
                    _logger->error(errorMessage);

                    throw e;
                }
                catch(exception e)
                {
                    string errorMessage = __FILEREF__ + "listing directory failed"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                           + ", e.what(): " + e.what()
                    ;
                    _logger->error(errorMessage);

                    throw e;
                }
            }

            FileIO::closeDirectory (directory);
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "removeHavingPrefixFileName failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", e.what(): " + e.what()
            );
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "removeHavingPrefixFileName failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            );
        }

        _logger->info(__FILEREF__ + "Staging Retention finished"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
        );
    }
}

tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> MMSEngineProcessor::getMediaSourceDetails(
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace, MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot)        
{
    MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
    string mediaSourceURL;
    string mediaFileFormat;
    
    string field;
    if (ingestionType == MMSEngineDBFacade::IngestionType::AddContent)
    {
        field = "SourceURL";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            mediaSourceURL = parametersRoot.get(field, "XXX").asString();
        
        field = "FileFormat";
        mediaFileFormat = parametersRoot.get(field, "XXX").asString();

        string httpPrefix ("http://");
        string httpsPrefix ("https://");
        string ftpPrefix ("ftp://");
        string ftpsPrefix ("ftps://");
        string movePrefix("move://");   // move:///dir1/dir2/.../file
        string copyPrefix("copy://");
        if ((mediaSourceURL.size() >= httpPrefix.size() && 0 == mediaSourceURL.compare(0, httpPrefix.size(), httpPrefix))
                || (mediaSourceURL.size() >= httpsPrefix.size() && 0 == mediaSourceURL.compare(0, httpsPrefix.size(), httpsPrefix))
                || (mediaSourceURL.size() >= ftpPrefix.size() && 0 == mediaSourceURL.compare(0, ftpPrefix.size(), ftpPrefix))
                || (mediaSourceURL.size() >= ftpsPrefix.size() && 0 == mediaSourceURL.compare(0, ftpsPrefix.size(), ftpsPrefix))
                )
        {
            nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress;
        }
        else if (mediaSourceURL.size() >= movePrefix.size() && 0 == mediaSourceURL.compare(0, movePrefix.size(), movePrefix))
        {
            nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress;            
        }
        else if (mediaSourceURL.size() >= copyPrefix.size() && 0 == mediaSourceURL.compare(0, copyPrefix.size(), copyPrefix))
        {
            nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress;
        }
        else
        {
            nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress;
        }
    }   
    else
    {
        string errorMessage = __FILEREF__ + "ingestionType is wrong"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string md5FileCheckSum;
    field = "MD5FileCheckSum";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        MD5         md5;
        char        md5RealDigest [32 + 1];

        md5FileCheckSum = parametersRoot.get(field, "XXX").asString();
    }

    int fileSizeInBytes = -1;
    field = "FileSizeInBytes";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        fileSizeInBytes = parametersRoot.get(field, 3).asInt();

    tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> mediaSourceDetails;
    get<0>(mediaSourceDetails) = nextIngestionStatus;
    get<1>(mediaSourceDetails) = mediaSourceURL;
    get<2>(mediaSourceDetails) = mediaFileFormat;
    get<3>(mediaSourceDetails) = md5FileCheckSum;
    get<4>(mediaSourceDetails) = fileSizeInBytes;

    _logger->info(__FILEREF__ + "media source details"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", nextIngestionStatus: " + MMSEngineDBFacade::toString(get<0>(mediaSourceDetails))
        + ", mediaSourceURL: " + get<1>(mediaSourceDetails)
        + ", mediaFileFormat: " + get<2>(mediaSourceDetails)
        + ", md5FileCheckSum: " + get<3>(mediaSourceDetails)
        + ", fileSizeInBytes: " + to_string(get<4>(mediaSourceDetails))
    );

    
    return mediaSourceDetails;
}

void MMSEngineProcessor::validateMediaSourceFile (int64_t ingestionJobKey,
        string ftpDirectoryMediaSourceFileName,
        string md5FileCheckSum, int fileSizeInBytes)
{
    if (!FileIO::fileExisting(ftpDirectoryMediaSourceFileName))
    {
        string errorMessage = __FILEREF__ + "Media Source file does not exist (it was not uploaded yet)"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", ftpDirectoryMediaSourceFileName: " + ftpDirectoryMediaSourceFileName;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    if (md5FileCheckSum != "")
    {
        MD5         md5;
        char        md5RealDigest [32 + 1];

        strcpy (md5RealDigest, md5.digestFile((char *) ftpDirectoryMediaSourceFileName.c_str()));

        if (md5FileCheckSum != md5RealDigest)
        {
            string errorMessage = __FILEREF__ + "MD5 check failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", ftpDirectoryMediaSourceFileName: " + ftpDirectoryMediaSourceFileName
                + ", md5FileCheckSum: " + md5FileCheckSum
                + ", md5RealDigest: " + md5RealDigest
                    ;
            _logger->error(errorMessage);
            throw runtime_error(errorMessage);
        }
    }
    
    if (fileSizeInBytes != -1)
    {
        bool inCaseOfLinkHasItToBeRead = false;
        unsigned long downloadedFileSizeInBytes = 
            FileIO:: getFileSizeInBytes (ftpDirectoryMediaSourceFileName, inCaseOfLinkHasItToBeRead);

        if (fileSizeInBytes != downloadedFileSizeInBytes)
        {
            string errorMessage = __FILEREF__ + "FileSize check failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", ftpDirectoryMediaSourceFileName: " + ftpDirectoryMediaSourceFileName
                + ", metadataFileSizeInBytes: " + to_string(fileSizeInBytes)
                + ", downloadedFileSizeInBytes: " + to_string(downloadedFileSizeInBytes)
            ;
            _logger->error(errorMessage);
            throw runtime_error(errorMessage);
        }
    }    
}


size_t curlDownloadCallback(char* ptr, size_t size, size_t nmemb, void *f)
{
    MMSEngineProcessor::CurlDownloadData* curlDownloadData = (MMSEngineProcessor::CurlDownloadData*) f;
    
    auto logger = spdlog::get("mmsEngineService");

    if (curlDownloadData->currentChunkNumber == 0)
    {
        (curlDownloadData->mediaSourceFileStream).open(
                curlDownloadData -> workspaceIngestionBinaryPathName, ofstream::binary | ofstream::trunc);
        curlDownloadData->currentChunkNumber += 1;
        
        logger->info(__FILEREF__ + "Opening binary file"
             + ", curlDownloadData -> workspaceIngestionBinaryPathName: " + curlDownloadData -> workspaceIngestionBinaryPathName
             + ", curlDownloadData->currentChunkNumber: " + to_string(curlDownloadData->currentChunkNumber)
             + ", curlDownloadData->currentTotalSize: " + to_string(curlDownloadData->currentTotalSize)
             + ", curlDownloadData->maxChunkFileSize: " + to_string(curlDownloadData->maxChunkFileSize)
        );
    }
    else if (curlDownloadData->currentTotalSize >= 
            curlDownloadData->currentChunkNumber * curlDownloadData->maxChunkFileSize)
    {
        (curlDownloadData->mediaSourceFileStream).close();

        /*
        string localPathFileName = curlDownloadData->workspaceIngestionBinaryPathName
                // + ".new"
                ;
        if (curlDownloadData->currentChunkNumber >= 2)
        {
            try
            {
                bool removeSrcFileAfterConcat = true;

                logger->info(__FILEREF__ + "Concat file"
                    + ", localPathFileName: " + localPathFileName
                    + ", curlDownloadData->workspaceIngestionBinaryPathName: " + curlDownloadData->workspaceIngestionBinaryPathName
                    + ", removeSrcFileAfterConcat: " + to_string(removeSrcFileAfterConcat)
                );

                FileIO::concatFile(curlDownloadData->workspaceIngestionBinaryPathName, localPathFileName, removeSrcFileAfterConcat);
            }
            catch(runtime_error e)
            {
                string errorMessage = string("Error to concat file")
                    + ", localPathFileName: " + localPathFileName
                    + ", curlDownloadData->workspaceIngestionBinaryPathName: " + curlDownloadData->workspaceIngestionBinaryPathName
                        + ", e.what(): " + e.what()
                ;
                logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);            
            }
            catch(exception e)
            {
                string errorMessage = string("Error to concat file")
                    + ", localPathFileName: " + localPathFileName
                    + ", curlDownloadData->workspaceIngestionBinaryPathName: " + curlDownloadData->workspaceIngestionBinaryPathName
                ;
                logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);            
            }
        }
         */
        // (curlDownloadData->mediaSourceFileStream).open(localPathFileName, ios::binary | ios::out | ios::trunc);
        (curlDownloadData->mediaSourceFileStream).open(curlDownloadData->workspaceIngestionBinaryPathName, ofstream::binary | ofstream::app);
        curlDownloadData->currentChunkNumber += 1;

        logger->info(__FILEREF__ + "Opening binary file"
             + ", curlDownloadData->workspaceIngestionBinaryPathName: " + curlDownloadData->workspaceIngestionBinaryPathName
             + ", curlDownloadData->currentChunkNumber: " + to_string(curlDownloadData->currentChunkNumber)
             + ", curlDownloadData->currentTotalSize: " + to_string(curlDownloadData->currentTotalSize)
             + ", curlDownloadData->maxChunkFileSize: " + to_string(curlDownloadData->maxChunkFileSize)
        );
    }
    
    curlDownloadData->mediaSourceFileStream.write(ptr, size * nmemb);
    curlDownloadData->currentTotalSize += (size * nmemb);
    

    return size * nmemb;        
};

void MMSEngineProcessor::downloadMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace)
{
    bool downloadingCompleted = false;

/*
    - aggiungere un timeout nel caso nessun pacchetto è ricevuto entro XXXX seconds
    - per il resume:
        l'apertura dello stream of dovrà essere fatta in append in questo caso
        usare l'opzione CURLOPT_RESUME_FROM o CURLOPT_RESUME_FROM_LARGE (>2GB) per dire da dove ripartire
    per ftp vedere https://raw.githubusercontent.com/curl/curl/master/docs/examples/ftpuploadresume.c
 
RESUMING FILE TRANSFERS 
  
 To continue a file transfer where it was previously aborted, curl supports 
 resume on http(s) downloads as well as ftp uploads and downloads. 
  
 Continue downloading a document: 
  
        curl -C - -o file ftp://ftp.server.com/path/file 
  
 Continue uploading a document(*1): 
  
        curl -C - -T file ftp://ftp.server.com/path/file 
  
 Continue downloading a document from a web server(*2): 
  
        curl -C - -o file http://www.server.com/ 
  
 (*1) = This requires that the ftp server supports the non-standard command 
        SIZE. If it doesn't, curl will say so. 
  
 (*2) = This requires that the web server supports at least HTTP/1.1. If it 
        doesn't, curl will say so. 
 */    

    string workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace);
    workspaceIngestionBinaryPathName
        .append("/")
        .append(to_string(ingestionJobKey))
        .append("_source")
        ;

        
    for (int attemptIndex = 0; attemptIndex < _maxDownloadAttemptNumber && !downloadingCompleted; attemptIndex++)
    {
        bool downloadingStoppedByUser = false;
        
        try 
        {
            _logger->info(__FILEREF__ + "Downloading"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", sourceReferenceURL: " + sourceReferenceURL
                + ", attempt: " + to_string(attemptIndex + 1)
                + ", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
            );
            
            if (attemptIndex == 0)
            {
                CurlDownloadData curlDownloadData;
                curlDownloadData.currentChunkNumber = 0;
                curlDownloadData.currentTotalSize = 0;
                curlDownloadData.workspaceIngestionBinaryPathName   = workspaceIngestionBinaryPathName;
                curlDownloadData.maxChunkFileSize    = _downloadChunkSizeInMegaBytes * 1000000;
                
                // fstream mediaSourceFileStream(workspaceIngestionBinaryPathName, ios::binary | ios::out);
                // mediaSourceFileStream.exceptions(ios::badbit | ios::failbit);   // setting the exception mask
                // FILE *mediaSourceFileStream = fopen(workspaceIngestionBinaryPathName.c_str(), "wb");

                curlpp::Cleanup cleaner;
                curlpp::Easy request;

                // Set the writer callback to enable cURL 
                // to write result in a memory area
                // request.setOpt(new curlpp::options::WriteStream(&mediaSourceFileStream));
                
                curlpp::options::WriteFunctionCurlFunction curlDownloadCallbackFunction(curlDownloadCallback);
                curlpp::OptionTrait<void *, CURLOPT_WRITEDATA> curlDownloadDataData(&curlDownloadData);
                request.setOpt(curlDownloadCallbackFunction);
                request.setOpt(curlDownloadDataData);

                // Setting the URL to retrive.
                request.setOpt(new curlpp::options::Url(sourceReferenceURL));
                string httpsPrefix("https");
                if (sourceReferenceURL.size() >= httpsPrefix.size() && 0 == sourceReferenceURL.compare(0, httpsPrefix.size(), httpsPrefix))
                {
                    // disconnect if we can't validate server's cert
                    bool bSslVerifyPeer = false;
                    curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                    request.setOpt(sslVerifyPeer);

                    curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                    request.setOpt(sslVerifyHost);
                }

                chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
                double lastPercentageUpdated = -1.0;
                curlpp::types::ProgressFunctionFunctor functor = bind(&MMSEngineProcessor::progressCallback, this,
                        ingestionJobKey, lastProgressUpdate, lastPercentageUpdated, downloadingStoppedByUser,
                        placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
                request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
                request.setOpt(new curlpp::options::NoProgress(0L));
                
                _logger->info(__FILEREF__ + "Downloading media file"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", sourceReferenceURL: " + sourceReferenceURL
                );
                request.perform();
                
                (curlDownloadData.mediaSourceFileStream).close();

                /*
                string localPathFileName = curlDownloadData.workspaceIngestionBinaryPathName
                        + ".new";
                if (curlDownloadData.currentChunkNumber >= 2)
                {
                    try
                    {
                        bool removeSrcFileAfterConcat = true;

                        _logger->info(__FILEREF__ + "Concat file"
                            + ", localPathFileName: " + localPathFileName
                            + ", curlDownloadData.workspaceIngestionBinaryPathName: " + curlDownloadData.workspaceIngestionBinaryPathName
                            + ", removeSrcFileAfterConcat: " + to_string(removeSrcFileAfterConcat)
                        );

                        FileIO::concatFile(curlDownloadData.workspaceIngestionBinaryPathName, localPathFileName, removeSrcFileAfterConcat);
                    }
                    catch(runtime_error e)
                    {
                        string errorMessage = string("Error to concat file")
                            + ", localPathFileName: " + localPathFileName
                            + ", curlDownloadData.workspaceIngestionBinaryPathName: " + curlDownloadData.workspaceIngestionBinaryPathName
                                + ", e.what(): " + e.what()
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);            
                    }
                    catch(exception e)
                    {
                        string errorMessage = string("Error to concat file")
                            + ", localPathFileName: " + localPathFileName
                            + ", curlDownloadData.workspaceIngestionBinaryPathName: " + curlDownloadData.workspaceIngestionBinaryPathName
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);            
                    }
                }
                */
            }
            else
            {
                _logger->warn(__FILEREF__ + "Coming from a download failure, trying to Resume"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                );
                
                // FILE *mediaSourceFileStream = fopen(workspaceIngestionBinaryPathName.c_str(), "wb+");
                long long fileSize;
                {
                    ofstream mediaSourceFileStream(workspaceIngestionBinaryPathName, ofstream::binary | ofstream::app);
                    fileSize = mediaSourceFileStream.tellp();
                    mediaSourceFileStream.close();
                }

                CurlDownloadData curlDownloadData;
                curlDownloadData.workspaceIngestionBinaryPathName   = workspaceIngestionBinaryPathName;
                curlDownloadData.maxChunkFileSize    = _downloadChunkSizeInMegaBytes * 1000000;

                curlDownloadData.currentChunkNumber = fileSize % curlDownloadData.maxChunkFileSize;
                // fileSize = curlDownloadData.currentChunkNumber * curlDownloadData.maxChunkFileSize;
                curlDownloadData.currentTotalSize = fileSize;

                curlpp::Cleanup cleaner;
                curlpp::Easy request;

                // Set the writer callback to enable cURL 
                // to write result in a memory area
                // request.setOpt(new curlpp::options::WriteStream(&mediaSourceFileStream));

                curlpp::options::WriteFunctionCurlFunction curlDownloadCallbackFunction(curlDownloadCallback);
                curlpp::OptionTrait<void *, CURLOPT_WRITEDATA> curlDownloadDataData(&curlDownloadData);
                request.setOpt(curlDownloadCallbackFunction);
                request.setOpt(curlDownloadDataData);

                // Setting the URL to retrive.
                request.setOpt(new curlpp::options::Url(sourceReferenceURL));
                string httpsPrefix("https");
                if (sourceReferenceURL.size() >= httpsPrefix.size() && 0 == sourceReferenceURL.compare(0, httpsPrefix.size(), httpsPrefix))
                {
                    _logger->info(__FILEREF__ + "Setting SslEngineDefault"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    );
                    request.setOpt(new curlpp::options::SslEngineDefault());
                }

                chrono::system_clock::time_point lastTimeProgressUpdate = chrono::system_clock::now();
                double lastPercentageUpdated = -1.0;
                curlpp::types::ProgressFunctionFunctor functor = bind(&MMSEngineProcessor::progressCallback, this,
                        ingestionJobKey, lastTimeProgressUpdate, lastPercentageUpdated, downloadingStoppedByUser,
                        placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
                request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
                request.setOpt(new curlpp::options::NoProgress(0L));
                
                if (fileSize > 2 * 1000 * 1000 * 1000)
                    request.setOpt(new curlpp::options::ResumeFromLarge(fileSize));
                else
                    request.setOpt(new curlpp::options::ResumeFrom(fileSize));
                
                _logger->info(__FILEREF__ + "Resume Download media file"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", sourceReferenceURL: " + sourceReferenceURL
                    + ", resuming from fileSize: " + to_string(fileSize)
                );
                request.perform();
                
                (curlDownloadData.mediaSourceFileStream).close();

                /*
                string localPathFileName = curlDownloadData.workspaceIngestionBinaryPathName
                        + ".new";
                if (curlDownloadData.currentChunkNumber >= 2)
                {
                    try
                    {
                        bool removeSrcFileAfterConcat = true;

                        _logger->info(__FILEREF__ + "Concat file"
                            + ", localPathFileName: " + localPathFileName
                            + ", curlDownloadData.workspaceIngestionBinaryPathName: " + curlDownloadData.workspaceIngestionBinaryPathName
                            + ", removeSrcFileAfterConcat: " + to_string(removeSrcFileAfterConcat)
                        );

                        FileIO::concatFile(curlDownloadData.workspaceIngestionBinaryPathName, localPathFileName, removeSrcFileAfterConcat);
                    }
                    catch(runtime_error e)
                    {
                        string errorMessage = string("Error to concat file")
                            + ", localPathFileName: " + localPathFileName
                            + ", curlDownloadData.workspaceIngestionBinaryPathName: " + curlDownloadData.workspaceIngestionBinaryPathName
                                + ", e.what(): " + e.what()
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);            
                    }
                    catch(exception e)
                    {
                        string errorMessage = string("Error to concat file")
                            + ", localPathFileName: " + localPathFileName
                            + ", curlDownloadData.workspaceIngestionBinaryPathName: " + curlDownloadData.workspaceIngestionBinaryPathName
                        ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);            
                    }
                }
                 */
            }

            downloadingCompleted = true;

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
                + ", downloadingCompleted: " + to_string(downloadingCompleted)
            );                            
            _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
                ingestionJobKey, downloadingCompleted);
        }
        catch (curlpp::LogicError & e) 
        {
            _logger->error(__FILEREF__ + "Download failed (LogicError)"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                + ", sourceReferenceURL: " + sourceReferenceURL 
                + ", exception: " + e.what()
            );

            if (downloadingStoppedByUser)
            {
                downloadingCompleted = true;
            }
            else
            {
                if (attemptIndex + 1 == _maxDownloadAttemptNumber)
                {
                    _logger->info(__FILEREF__ + "Reached the max number of download attempts"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
                    );
                    
                    _logger->info(__FILEREF__ + "Update IngestionJob"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", IngestionStatus: " + "End_IngestionFailure"
                        + ", errorMessage: " + e.what()
                    );                            
                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                            MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                            e.what(), "" /* processorMMS */ );

                    return;
                }
                else
                {
                    _logger->info(__FILEREF__ + "Download failed. sleeping before to attempt again"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                        + ", sourceReferenceURL: " + sourceReferenceURL 
                        + ", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
                    );
                    this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
                }
            }
        }
        catch (curlpp::RuntimeError & e) 
        {
            _logger->error(__FILEREF__ + "Download failed (RuntimeError)"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                + ", sourceReferenceURL: " + sourceReferenceURL 
                + ", exception: " + e.what()
            );

            if (downloadingStoppedByUser)
            {
                downloadingCompleted = true;
            }
            else
            {
                if (attemptIndex + 1 == _maxDownloadAttemptNumber)
                {
                    _logger->info(__FILEREF__ + "Reached the max number of download attempts"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
                    );
                    
                    _logger->info(__FILEREF__ + "Update IngestionJob"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", IngestionStatus: " + "End_IngestionFailure"
                        + ", errorMessage: " + e.what()
                    );                            
                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                            MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                            e.what(), "" /* processorMMS */);

                    return;
                }
                else
                {
                    _logger->info(__FILEREF__ + "Download failed. sleeping before to attempt again"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                        + ", sourceReferenceURL: " + sourceReferenceURL 
                        + ", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
                    );
                    this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
                }
            }
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Download failed (exception)"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                + ", sourceReferenceURL: " + sourceReferenceURL 
                + ", exception: " + e.what()
            );

            if (downloadingStoppedByUser)
            {
                downloadingCompleted = true;
            }
            else
            {
                if (attemptIndex + 1 == _maxDownloadAttemptNumber)
                {
                    _logger->info(__FILEREF__ + "Reached the max number of download attempts"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
                    );
                    
                    _logger->info(__FILEREF__ + "Update IngestionJob"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", IngestionStatus: " + "End_IngestionFailure"
                        + ", errorMessage: " + e.what()
                    );                            
                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                            MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                            e.what(), "" /* processorMMS */);
                    
                    return;
                }
                else
                {
                    _logger->info(__FILEREF__ + "Download failed. sleeping before to attempt again"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                        + ", sourceReferenceURL: " + sourceReferenceURL 
                        + ", _secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
                    );
                    this_thread::sleep_for(chrono::seconds(_secondsWaitingAmongDownloadingAttempt));
                }
            }
        }
    }
}

void MMSEngineProcessor::moveMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace)
{

    try 
    {
        string workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace);
        workspaceIngestionBinaryPathName
            .append("/")
            .append(to_string(ingestionJobKey))
            .append("_source")
            ;

        string movePrefix("move://");
        if (!(sourceReferenceURL.size() >= movePrefix.size() && 0 == sourceReferenceURL.compare(0, movePrefix.size(), movePrefix)))
        {
            string errorMessage = string("sourceReferenceURL is not a move reference")
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                + ", sourceReferenceURL: " + sourceReferenceURL 
            ;
            
            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }
        string sourcePathName = sourceReferenceURL.substr(movePrefix.length());
                
        _logger->info(__FILEREF__ + "Moving"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", sourcePathName: " + sourcePathName
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        
        FileIO::moveFile(sourcePathName, workspaceIngestionBinaryPathName);
            
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", movingCompleted: " + to_string(true)
        );                            
        _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
            ingestionJobKey, true);
    }
    catch (runtime_error& e) 
    {
        _logger->error(__FILEREF__ + "Moving failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), "" /* processorMMS */);
        
        return;
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "Moving failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), "" /* processorMMS */);

        return;
    }
}

void MMSEngineProcessor::copyMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace)
{

    try 
    {
        string workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(workspace);
        workspaceIngestionBinaryPathName
            .append("/")
            .append(to_string(ingestionJobKey))
            .append("_source")
            ;

        string copyPrefix("copy://");
        if (!(sourceReferenceURL.size() >= copyPrefix.size() && 0 == sourceReferenceURL.compare(0, copyPrefix.size(), copyPrefix)))
        {
            string errorMessage = string("sourceReferenceURL is not a copy reference")
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                + ", sourceReferenceURL: " + sourceReferenceURL 
            ;
            
            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }
        string sourcePathName = sourceReferenceURL.substr(copyPrefix.length());

        _logger->info(__FILEREF__ + "Coping"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", sourcePathName: " + sourcePathName
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        
        FileIO::copyFile(sourcePathName, workspaceIngestionBinaryPathName);
            
        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", movingCompleted: " + to_string(true)
        );              
        
        _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
            ingestionJobKey, true);
    }
    catch (runtime_error& e) 
    {
        _logger->error(__FILEREF__ + "Coping failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), "" /* processorMMS */);
        
        return;
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "Coping failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_IngestionFailure"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                MMSEngineDBFacade::IngestionStatus::End_IngestionFailure, 
                e.what(), 
                "" /* processorMMS */);

        return;
    }
}

int MMSEngineProcessor::progressCallback(
        int64_t ingestionJobKey,
        chrono::system_clock::time_point& lastTimeProgressUpdate, 
        double& lastPercentageUpdated, bool& downloadingStoppedByUser,
        double dltotal, double dlnow,
        double ultotal, double ulnow)
{

    chrono::system_clock::time_point now = chrono::system_clock::now();
            
    if (dltotal != 0 &&
            (dltotal == dlnow 
            || now - lastTimeProgressUpdate >= chrono::seconds(_progressUpdatePeriodInSeconds)))
    {
        double progress = (dlnow / dltotal) * 100;
        // int downloadingPercentage = floorf(progress * 100) / 100;
        // this is to have one decimal in the percentage
        double downloadingPercentage = ((double) ((int) (progress * 10))) / 10;

        _logger->info(__FILEREF__ + "Download still running"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", downloadingPercentage: " + to_string(downloadingPercentage)
            + ", dltotal: " + to_string(dltotal)
            + ", dlnow: " + to_string(dlnow)
            + ", ultotal: " + to_string(ultotal)
            + ", ulnow: " + to_string(ulnow)
        );
        
        lastTimeProgressUpdate = now;

        if (lastPercentageUpdated != downloadingPercentage)
        {
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", downloadingPercentage: " + to_string(downloadingPercentage)
            );                            
            downloadingStoppedByUser = _mmsEngineDBFacade->updateIngestionJobSourceDownloadingInProgress (
                ingestionJobKey, downloadingPercentage);

            lastPercentageUpdated = downloadingPercentage;
        }

        if (downloadingStoppedByUser)
            return 1;   // stop downloading
    }

    return 0;
}