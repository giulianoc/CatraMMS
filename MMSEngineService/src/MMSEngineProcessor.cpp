
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "catralibraries/System.h"
#include "FFMpeg.h"
#include "Validator.h"
#include "MMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "CheckEncodingTimes.h"
#include "catralibraries/md5.h"


MMSEngineProcessor::MMSEngineProcessor(
        shared_ptr<spdlog::logger> logger, 
        shared_ptr<MultiEventsSet> multiEventsSet,
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        shared_ptr<MMSStorage> mmsStorage,
        ActiveEncodingsManager* pActiveEncodingsManager,
        Json::Value configuration
)
{
    _logger             = logger;
    _configuration      = configuration;
    _multiEventsSet     = multiEventsSet;
    _mmsEngineDBFacade  = mmsEngineDBFacade;
    _mmsStorage         = mmsStorage;
    _pActiveEncodingsManager = pActiveEncodingsManager;

    _firstGetEncodingJob            = true;
    _processorMMS                   = System::getHostName();
    
    _maxDownloadAttemptNumber       = configuration["download"].get("maxDownloadAttemptNumber", 5).asInt();
    _progressUpdatePeriodInSeconds  = configuration["download"].get("progressUpdatePeriodInSeconds", 5).asInt();
    _secondsWaitingAmongDownloadingAttempt  = configuration["download"].get("secondsWaitingAmongDownloadingAttempt", 5).asInt();
    
    _maxIngestionJobsPerEvent       = configuration["mms"].get("maxIngestionJobsPerEvent", 5).asInt();
    // _maxIngestionJobsWithDependencyToCheckPerEvent = configuration["mms"].get("maxIngestionJobsWithDependencyToCheckPerEvent", 5).asInt();

    _dependencyExpirationInHours        = configuration["mms"].get("dependencyExpirationInHours", 5).asInt();
    _downloadChunkSizeInMegaBytes       = configuration["download"].get("downloadChunkSizeInMegaBytes", 5).asInt();
    
    _emailProtocol                      = _configuration["EmailNotification"].get("protocol", "XXX").asString();
    _emailServer                        = _configuration["EmailNotification"].get("server", "XXX").asString();
    _emailPort                          = _configuration["EmailNotification"].get("port", "XXX").asInt();
    _emailUserName                      = _configuration["EmailNotification"].get("userName", "XXX").asString();
    _emailPassword                      = _configuration["EmailNotification"].get("password", "XXX").asString();
    _emailFrom                          = _configuration["EmailNotification"].get("from", "XXX").asString();
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
    _logger->info(__FILEREF__ + "MMSEngineProcessor thread started");

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
                _logger->debug(__FILEREF__ + "1. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION");

                try
                {
                    handleCheckIngestionEvent ();
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleCheckIngestionEvent failed"
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

                _logger->debug(__FILEREF__ + "2. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION");
            }
            break;
            case MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT:	// 2
            {
                _logger->debug(__FILEREF__ + "1. Received LOCALASSETINGESTIONEVENT");

                shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = dynamic_pointer_cast<LocalAssetIngestionEvent>(event);

                try
                {
                    handleLocalAssetIngestionEvent (localAssetIngestionEvent);
                }
                catch(runtime_error e)
                {
                    _logger->error(__FILEREF__ + "handleLocalAssetIngestionEvent failed"
                        + ", exception: " + e.what()
                    );
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleLocalAssetIngestionEvent failed"
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<LocalAssetIngestionEvent>(localAssetIngestionEvent);

                _logger->debug(__FILEREF__ + "2. Received LOCALASSETINGESTIONEVENT");
            }
            break;
            case MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODINGEVENT:	// 3
            {
                _logger->debug(__FILEREF__ + "1. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING");

                try
                {
                    handleCheckEncodingEvent ();
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleCheckEncodingEvent failed"
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

                _logger->debug(__FILEREF__ + "2. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING");
            }
            break;
            /*
            case MMSENGINE_EVENTTYPEIDENTIFIER_GENERATEIMAGETOINGESTEVENT:	// 4
            {
                _logger->debug(__FILEREF__ + "Received MMSENGINE_EVENTTYPEIDENTIFIER_GENERATEIMAGETOINGESTEVENT");

                shared_ptr<GenerateImageToIngestEvent>    generateImageToIngestEvent =
                        dynamic_pointer_cast<GenerateImageToIngestEvent>(event);

                try
                {
                    handleGenerateImageToIngestEvent (generateImageToIngestEvent);
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleGenerateImageToIngestEvent failed"
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<GenerateImageToIngestEvent>(generateImageToIngestEvent);

            }
            break;
             */
            default:
                throw runtime_error(string("Event type identifier not managed")
                        + to_string(event->getEventKey().first));
        }
    }

    _logger->info(__FILEREF__ + "MMSEngineProcessor thread terminated");
}

void MMSEngineProcessor::handleCheckIngestionEvent()
{
    
    try
    {
        vector<tuple<int64_t,string,shared_ptr<Workspace>,string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus>> 
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
                    + ", exception: " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "getIngestionsToBeManaged failed"
                    + ", exception: " + e.what()
            );

            throw e;
        }
        
        for (tuple<int64_t, string, shared_ptr<Workspace>, string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus> 
                ingestionToBeManaged: ingestionsToBeManaged)
        {
            int64_t ingestionJobKey;
            try
            {
                shared_ptr<Workspace> workspace;
                string startIngestion;
                string metaDataContent;
                string sourceReference;
                MMSEngineDBFacade::IngestionType ingestionType;
                MMSEngineDBFacade::IngestionStatus ingestionStatus;

                tie(ingestionJobKey, startIngestion, workspace, metaDataContent,
                        ingestionType, ingestionStatus) = ingestionToBeManaged;
                
                _logger->info(__FILEREF__ + "json to be processed"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", startIngestion: " + startIngestion
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
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", exception: " + e.what()
                    );
                    string errorMessage = e.what();

                    _logger->info(__FILEREF__ + "Update IngestionJob"
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", IngestionStatus: " + "End_WorkspaceReachedHisMaxIngestionNumber"
                        + ", errorMessage: " + e.what()
                    );                            
                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                            MMSEngineDBFacade::IngestionStatus::End_WorkspaceReachedHisMaxIngestionNumber,
                            e.what(), "" // processorMMS
                    );

                    throw e;
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "checkWorkspaceMaxIngestionNumber failed"
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", exception: " + e.what()
                    );
                    string errorMessage = e.what();

                    _logger->info(__FILEREF__ + "Update IngestionJob"
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", IngestionStatus: " + "End_WorkspaceReachedHisMaxIngestionNumber"
                        + ", errorMessage: " + e.what()
                    );                            
                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                            MMSEngineDBFacade::IngestionStatus::End_WorkspaceReachedHisMaxIngestionNumber,
                            e.what(), "" // processorMMS
                    );

                    throw e;
                }
                
                if (ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress
                        || ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress
                        || ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress
                        || ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
                {
                    // source binary download or uploaded terminated

                    string sourceFileName = to_string(ingestionJobKey) + ".binary";

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

                        localAssetIngestionEvent->setMetadataContent(metaDataContent);

                        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
                        _multiEventsSet->addEvent(event);

                        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
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
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", metaDataContent: " + metaDataContent
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        _logger->info(__FILEREF__ + "Update IngestionJob"
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

                    vector<int64_t> dependencies;
                    try
                    {
                        Validator validator(_logger, _mmsEngineDBFacade);
                        
                        dependencies = validator.validateTaskMetadata(
                                ingestionType, parametersRoot);                        
                    }
                    catch(runtime_error e)
                    {
                        _logger->error(__FILEREF__ + "validateMetadata failed"
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", exception: " + e.what()
                        );

                        string errorMessage = e.what();

                        _logger->info(__FILEREF__ + "Update IngestionJob"
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
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", exception: " + e.what()
                        );

                        string errorMessage = e.what();

                        _logger->info(__FILEREF__ + "Update IngestionJob"
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
                        if (ingestionType == MMSEngineDBFacade::IngestionType::ContentIngestion)
                        {
                            MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
                            string mediaSourceURL;
                            string mediaSourceFileName;
                            string md5FileCheckSum;
                            int fileSizeInBytes;
                            try
                            {
                                tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> mediaSourceDetails;

                                mediaSourceDetails = getMediaSourceDetails(
                                        ingestionJobKey, workspace,
                                        ingestionType, parametersRoot);

                                tie(nextIngestionStatus,
                                        mediaSourceURL, mediaSourceFileName, 
                                        md5FileCheckSum, fileSizeInBytes) = mediaSourceDetails;                        
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
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
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
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
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                ;
                                _logger->error(__FILEREF__ + errorMessage);

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
                                _logger->error(__FILEREF__ + "manageEncodeTask failed"
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
                                generateAndIngestFrames(
                                        ingestionJobKey, 
                                        workspace, 
                                        ingestionType,
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestFrames failed"
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
                                _logger->error(__FILEREF__ + "generateAndIngestFrames failed"
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
                                _logger->error(__FILEREF__ + "generateAndIngestSlideshow failed"
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
                                _logger->error(__FILEREF__ + "generateAndIngestConcatenation failed"
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
                                _logger->error(__FILEREF__ + "generateAndIngestCutMedia failed"
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
                                _logger->error(__FILEREF__ + "manageEmailNotification failed"
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
                        }
                        else
                        {
                            string errorMessage = string("Unknown IngestionType")
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType);
                            _logger->error(__FILEREF__ + errorMessage);

                            _logger->info(__FILEREF__ + "Update IngestionJob"
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
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", exception: " + e.what()
                );
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "Exception managing the Ingestion entry"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", exception: " + e.what()
                );
            }
        }
    }
    catch(...)
    {
        _logger->error(__FILEREF__ + "handleCheckIngestionEvent failed");
    }
}

void MMSEngineProcessor::handleLocalAssetIngestionEvent (
    shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent)
{
    string      metadataFileContent;
    vector<int64_t> dependencies;
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
                    + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                    + ", errors: " + errors
                    + ", metaDataContent: " + sMetadataContent
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        dependencies = validator.validateTaskMetadata(
                localAssetIngestionEvent->getIngestionType(), parametersRoot);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "validateMetadata failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", localAssetIngestionEvent->getMetadataContent(): " + localAssetIngestionEvent->getMetadataContent()
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMetadataFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what(), "" //ProcessorMMS
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "validateMetadata failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMetadataFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what(), "" // ProcessorMMS
        );

        throw e;
    }

    MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
    string mediaSourceURL;
    string mediaSourceFileName;
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
                mediaSourceURL, mediaSourceFileName, 
                md5FileCheckSum, fileSizeInBytes) = mediaSourceDetails;                        
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed,
            e.what(), "" // ProcessorMMS
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed,
            e.what(), "" // ProcessorMMS
        );

        throw e;
    }

    if (localAssetIngestionEvent->getMMSSourceFileName() != "") 
        mediaSourceFileName = localAssetIngestionEvent->getMMSSourceFileName();

    string workspaceIngestionBinaryPathName;
    try
    {
        workspaceIngestionBinaryPathName = _mmsStorage->getWorkspaceIngestionRepository(
                localAssetIngestionEvent->getWorkspace());
        workspaceIngestionBinaryPathName
                .append("/")
                .append(localAssetIngestionEvent->getIngestionSourceFileName())
                ;

        validateMediaSourceFile(
                localAssetIngestionEvent->getIngestionJobKey(),
                workspaceIngestionBinaryPathName,
                md5FileCheckSum, fileSizeInBytes);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "validateMediaSourceFile failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed,
            e.what(), "" // ProcessorMMS
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "validateMediaSourceFile failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );

        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
            MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed,
            e.what(), "" // ProcessorMMS
        );

        throw e;
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
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed");
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
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
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed");
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
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
    if (validator.isVideoAudioMedia(mediaSourceFileName))
    {
        try
        {
            FFMpeg ffmpeg (_configuration, _logger);
            tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> mediaInfo =
                ffmpeg.getMediaInfo(mmsAssetPathName);

            tie(durationInMilliSeconds, bitRate, 
                videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
                audioCodecName, audioSampleRate, audioChannels, audioBitRate) = mediaInfo;
            
            if (videoCodecName == "")
                contentType = MMSEngineDBFacade::ContentType::Audio;
            else
                contentType = MMSEngineDBFacade::ContentType::Video;
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "EncoderVideoAudioProxy::getMediaInfo failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
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
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
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
    else if (validator.isImageMedia(mediaSourceFileName))
    {
        try
        {
            _logger->info(__FILEREF__ + "Processing through Magick"
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
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", e.what(): " + e.what()
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
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
        catch( Magick::Warning &e )
        {
            _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", e.what(): " + e.what()
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
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
        catch( Magick::ErrorFileOpen &e ) 
        { 
            _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", e.what(): " + e.what()
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
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
            _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            );

            _logger->info(__FILEREF__ + "Remove file"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            FileIO::remove(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Update IngestionJob"
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
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mediaSourceFileName: " + mediaSourceFileName
        ;

        _logger->error(__FILEREF__ + errorMessage);
        
        _logger->info(__FILEREF__ + "Remove file"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mmsAssetPathName: " + mmsAssetPathName
        );
        FileIO::remove(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Update IngestionJob"
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
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mediaItemKey: " + to_string(mediaItemKeyAndPhysicalPathKey.first)
            + ", physicalPathKey: " + to_string(mediaItemKeyAndPhysicalPathKey.second)
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata failed"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", e.what: " + e.what()
        );

        _logger->info(__FILEREF__ + "Remove file"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mmsAssetPathName: " + mmsAssetPathName
        );
        FileIO::remove(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Update IngestionJob"
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
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
        );

        _logger->info(__FILEREF__ + "Remove file"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", mmsAssetPathName: " + mmsAssetPathName
        );
        FileIO::remove(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Update IngestionJob"
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

void MMSEngineProcessor::manageEncodeTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<int64_t>& dependencies
)
{
    try
    {        
        // This task shall contain EncodingProfileKey.
        // We cannot have EncodingProfilesSetKey because we replaced it with a GroupOfTasks
        //  having just EncodingProfileKey        
        
        string field = "EncodingProfileKey";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        int64_t encodingProfileKey = parametersRoot.get(field, "XXX").asInt64();

        field = "EncodingPriority";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        MMSEngineDBFacade::EncodingPriority encodingPriority =
                MMSEngineDBFacade::toEncodingPriority(parametersRoot.get(field, "XXX").asString());

        int64_t sourceMediaItemKey = dependencies.back();
    
        _mmsEngineDBFacade->addEncodingJob (ingestionJobKey,
            encodingProfileKey, sourceMediaItemKey, encodingPriority);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::generateAndIngestFrames(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<int64_t>& dependencies
)
{
    try
    {
        string field;
        
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

        int64_t sourceMediaItemKey = dependencies.back();
        
        int64_t encodingProfileKey = -1;
        string sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);

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
                videoDetails = _mmsEngineDBFacade->getVideoDetails(sourceMediaItemKey);
            
            tie(durationInMilliSeconds, bitRate,
                videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
                audioCodecName, audioSampleRate, audioChannels, audioBitRate) = videoDetails;
        }
        catch(runtime_error e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (durationInMilliSeconds < startTimeInSeconds * 1000)
        {
            string errorMessage = __FILEREF__ + "Frame was not generated because instantInSeconds is bigger than the video duration"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                    + ", startTimeInSeconds: " + to_string(startTimeInSeconds)
                    + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        field = "SourceFileName";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string sourceFileName = parametersRoot.get(field, "XXX").asString();

        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
                workspace);

        string temporaryFileName;
        string textToBeReplaced;
        string textToReplace;
        {
            temporaryFileName = to_string(ingestionJobKey) + ".binary";
            size_t extensionIndex = sourceFileName.find_last_of(".");
            if (extensionIndex != string::npos)
                temporaryFileName.append(sourceFileName.substr(extensionIndex));

            textToBeReplaced = to_string(ingestionJobKey) + ".binary";
            textToReplace = sourceFileName.substr(0, extensionIndex);
        }
        
        FFMpeg ffmpeg (_configuration, _logger);

        vector<string> generatedFramesFileNames = ffmpeg.generateFramesToIngest(
                ingestionJobKey,
                workspaceIngestionRepository,
                temporaryFileName,
                startTimeInSeconds,
                maxFramesNumber,
                videoFilter,
                periodInSeconds,
                mjpeg,
                width == -1 ? videoWidth : width, 
                height == -1 ? videoHeight : height,
                sourcePhysicalPath
        );

        _logger->info(__FILEREF__ + "generateFramesToIngest done"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", generatedFramesFileNames.size: " + to_string(generatedFramesFileNames.size())
        );
        for (string generatedFrameFileName: generatedFramesFileNames)
        {
            _logger->info(__FILEREF__ + "Generated Frame to ingest"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", generatedFrameFileName: " + generatedFrameFileName
                + ", textToBeReplaced: " + textToBeReplaced
                + ", textToReplace: " + textToReplace
            );

            string mmsSourceFileName = generatedFrameFileName;
            
            if (mmsSourceFileName.find(textToBeReplaced) != string::npos)
                mmsSourceFileName.replace(mmsSourceFileName.find(textToBeReplaced), textToBeReplaced.length(), textToReplace);

            _logger->info(__FILEREF__ + "Generated Frame to ingest"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", new generatedFrameFileName: " + generatedFrameFileName
            );
            
            string imageMetaDataContent = generateImageMetadataToIngest(
                    ingestionJobKey,
                    mjpeg,
                    generatedFrameFileName,
                    parametersRoot
            );

            {
                shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                        ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

                localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
                localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
                localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

                localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
                localAssetIngestionEvent->setIngestionSourceFileName(generatedFrameFileName);
                // localAssetIngestionEvent->setMMSSourceFileName(mmsSourceFileName);
                localAssetIngestionEvent->setMMSSourceFileName(generatedFrameFileName);
                localAssetIngestionEvent->setWorkspace(workspace);
                localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::ContentIngestion);

                localAssetIngestionEvent->setMetadataContent(imageMetaDataContent);

                shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
                _multiEventsSet->addEvent(event);

                _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", getEventKey().first: " + to_string(event->getEventKey().first)
                    + ", getEventKey().second: " + to_string(event->getEventKey().second));
            }
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::generateAndIngestSlideshow(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<int64_t>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No images found"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        MMSEngineDBFacade::ContentType slideshowContentType;
        bool slideshowContentTypeInitialized = false;
        vector<string> sourcePhysicalPaths;
        
        for (int64_t sourceMediaItemKey: dependencies)
        {
            int64_t encodingProfileKey = -1;
            string sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);

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

        field = "SourceFileName";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string sourceFileName = parametersRoot.get(field, "XXX").asString();

        string localSourceFileName = to_string(ingestionJobKey)
                + ".binary"
                ;
        size_t extensionIndex = sourceFileName.find_last_of(".");
        if (extensionIndex != string::npos)
            localSourceFileName.append(sourceFileName.substr(extensionIndex));
        
        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
            workspace);
        string slideshowMediaPathName = workspaceIngestionRepository + "/" 
                + localSourceFileName;
        
        FFMpeg ffmpeg (_configuration, _logger);
        ffmpeg.generateSlideshowMediaToIngest(ingestionJobKey, 
                sourcePhysicalPaths, durationOfEachSlideInSeconds, outputFrameRate,
                slideshowMediaPathName);

        _logger->info(__FILEREF__ + "generateSlideshowMediaToIngest done"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", slideshowMediaPathName: " + slideshowMediaPathName
        );
                
        string mediaMetaDataContent = generateMediaMetadataToIngest(
                ingestionJobKey,
                true,
                sourceFileName,
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
            localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::ContentIngestion);

            localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

            shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
            _multiEventsSet->addEvent(event);

            _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", getEventKey().first: " + to_string(event->getEventKey().first)
                + ", getEventKey().second: " + to_string(event->getEventKey().second));
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestSlideshow failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestSlideshow failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::generateAndIngestConcatenation(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<int64_t>& dependencies
)
{
    try
    {
        if (dependencies.size() < 2)
        {
            string errorMessage = __FILEREF__ + "No enough media to be concatenated"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        MMSEngineDBFacade::ContentType concatContentType;
        bool concatContentTypeInitialized = false;
        vector<string> sourcePhysicalPaths;
        
        for (int64_t sourceMediaItemKey: dependencies)
        {
            int64_t encodingProfileKey = -1;
            string sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);

            sourcePhysicalPaths.push_back(sourcePhysicalPath);
            
            bool warningIfMissing = false;
            
            MMSEngineDBFacade::ContentType contentType = _mmsEngineDBFacade->getMediaItemKeyDetails(
                sourceMediaItemKey, warningIfMissing);
            
            if (!concatContentTypeInitialized)
            {
                concatContentType = contentType;
                if (concatContentType != MMSEngineDBFacade::ContentType::Video
                        && concatContentType != MMSEngineDBFacade::ContentType::Audio)
                {
                    string errorMessage = __FILEREF__ + "It is not possible to concatenate a media that is not video or audio"
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
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                            + ", concatContentType: " + MMSEngineDBFacade::toString(concatContentType)
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }

        string sourceFileName;
        string field = "SourceFileName";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            sourceFileName = parametersRoot.get(field, "XXX").asString();
        }
        
        // this is a concat, so destination file name shall have the same
        // extension as the source file name
        string localSourceFileName = to_string(ingestionJobKey)
                + "." // + ".binary"
                ;
        size_t extensionIndex = sourcePhysicalPaths.front().find_last_of(".");
        if (extensionIndex != string::npos)
            localSourceFileName.append(sourcePhysicalPaths.front().substr(extensionIndex));
        
        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
            workspace);
        string concatenatedMediaPathName = workspaceIngestionRepository + "/" 
                + localSourceFileName;
        
        FFMpeg ffmpeg (_configuration, _logger);
        ffmpeg.generateConcatMediaToIngest(ingestionJobKey, sourcePhysicalPaths, concatenatedMediaPathName);

        _logger->info(__FILEREF__ + "generateConcatMediaToIngest done"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", concatenatedMediaPathName: " + concatenatedMediaPathName
        );
                
        string mediaMetaDataContent = generateMediaMetadataToIngest(
                ingestionJobKey,
                concatContentType == MMSEngineDBFacade::ContentType::Video ? true : false,
                sourceFileName == "" ? localSourceFileName : sourceFileName,
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
            localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::ContentIngestion);

            localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

            shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
            _multiEventsSet->addEvent(event);

            _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", getEventKey().first: " + to_string(event->getEventKey().first)
                + ", getEventKey().second: " + to_string(event->getEventKey().second));
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestConcatenation failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestConcatenation failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::generateAndIngestCutMedia(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<int64_t>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any media to be cut"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        int64_t sourceMediaItemKey = dependencies.back();

        int64_t encodingProfileKey = -1;
        string sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);

        bool warningIfMissing = false;

        MMSEngineDBFacade::ContentType contentType = _mmsEngineDBFacade->getMediaItemKeyDetails(
            sourceMediaItemKey, warningIfMissing);

        if (contentType != MMSEngineDBFacade::ContentType::Video
                && contentType != MMSEngineDBFacade::ContentType::Audio)
        {
            string errorMessage = __FILEREF__ + "It is not possible to cut a media that is not video or audio"
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
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        int64_t durationInMilliSeconds;
        try
        {
            int videoWidth;
            int videoHeight;
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
                videoDetails = _mmsEngineDBFacade->getVideoDetails(sourceMediaItemKey);
            
            tie(durationInMilliSeconds, bitRate,
                videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
                audioCodecName, audioSampleRate, audioChannels, audioBitRate) = videoDetails;
        }
        catch(runtime_error e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getVideoDetails failed"
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
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                    + ", startTimeInSeconds: " + to_string(startTimeInSeconds)
                    + ", endTimeInSeconds: " + to_string(endTimeInSeconds)
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

        // this is a cut so destination file name shall have the same
        // extension as the source file name
        string localSourceFileName = to_string(ingestionJobKey)
                + "." // + ".binary"
                ;
        size_t extensionIndex = sourcePhysicalPath.find_last_of(".");
        if (extensionIndex != string::npos)
            localSourceFileName.append(sourcePhysicalPath.substr(extensionIndex));
        
        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
                workspace);
        string cutMediaPathName = workspaceIngestionRepository + "/"
                + localSourceFileName;
        
        FFMpeg ffmpeg (_configuration, _logger);
        ffmpeg.generateCutMediaToIngest(ingestionJobKey, sourcePhysicalPath, 
                startTimeInSeconds, endTimeInSeconds, framesNumber,
                cutMediaPathName);

        _logger->info(__FILEREF__ + "generateCutMediaToIngest done"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", cutMediaPathName: " + cutMediaPathName
        );
        
        string mediaMetaDataContent = generateMediaMetadataToIngest(
                ingestionJobKey,
                contentType == MMSEngineDBFacade::ContentType::Video ? true : false,
                sourceFileName == "" ? localSourceFileName : sourceFileName,
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
            localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::ContentIngestion);

            localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

            shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
            _multiEventsSet->addEvent(event);

            _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", getEventKey().first: " + to_string(event->getEventKey().first)
                + ", getEventKey().second: " + to_string(event->getEventKey().second));
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestCutMedia failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestCutMedia failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

void MMSEngineProcessor::manageEmailNotification(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<int64_t>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any IngestionJobKey in order to send an email"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string sIngestionJobJeyDependency;
        for (int64_t ingestionJobKeyDependency: dependencies)
        {
            if (sIngestionJobJeyDependency == "")
                sIngestionJobJeyDependency = to_string(ingestionJobKeyDependency);
            else
                sIngestionJobJeyDependency += (", " + to_string(ingestionJobKeyDependency));
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
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string subject = parametersRoot.get(field, "XXX").asString();

        field = "Message";
        if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
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
            
        sendEmail(emailAddress, "Task finished", emailBody);
        
        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", IngestionStatus: " + "End_TaskSuccess"
            + ", errorMessage: " + ""
        );                            
        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey,
                MMSEngineDBFacade::IngestionStatus::End_TaskSuccess, 
                "", "" // ProcessorMMS
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "sendEmail failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "sendEmail failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        throw e;
    }
}

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

        /* Note that this option isn't strictly required, omitting it will result
         * in libcurl sending the MAIL FROM command with empty sender data. All
         * autoresponses should have an empty reverse-path, and should be directed
         * to the address in the reverse-path which triggered them. Otherwise,
         * they could cause an endless loop. See RFC 5321 Section 4.5.5 for more
         * details.
         */
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, _emailFrom.c_str());

        /* Add two recipients, in this particular case they correspond to the
         * To: and Cc: addressees in the header, but they could be any kind of
         * recipient. */
        recipients = curl_slist_append(recipients, to.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        /* We're using a callback function to specify the payload (the headers and
         * body of the message). You could just use the CURLOPT_READDATA option to
         * specify a FILE pointer to read from. */
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, MMSEngineProcessor::emailPayloadFeed);
        curl_easy_setopt(curl, CURLOPT_READDATA, &emailLines);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        /* Send the message */
        _logger->info(__FILEREF__ + "Sending email...");
        res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
            _logger->error(__FILEREF__ + "curl_easy_perform() failed"
                + ", curl_easy_strerror(res): " + curl_easy_strerror(res)
            );
        else
            _logger->info(__FILEREF__ + "Email sent successful");

        /* Free the list of recipients */
        curl_slist_free_all(recipients);

        /* curl won't send the QUIT command until you call cleanup, so you should
         * be able to re-use this connection for additional messages (setting
         * CURLOPT_MAIL_FROM and CURLOPT_MAIL_RCPT as required, and calling
         * curl_easy_perform() again. It may not be a good idea to keep the
         * connection open for a very long time though (more than a few minutes
         * may result in the server timing out the connection), and you do want to
         * clean up in the end.
         */
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

string MMSEngineProcessor::generateImageMetadataToIngest(
        int64_t ingestionJobKey,
        bool mjpeg,
        string sourceFileName,
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
            + ", \"SourceFileName\": \"" + sourceFileName + "\""
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
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", imageMetadata: " + imageMetadata
            );

    return imageMetadata;
}

string MMSEngineProcessor::generateMediaMetadataToIngest(
        int64_t ingestionJobKey,
        bool video,
        string sourceFileName,
        Json::Value parametersRoot
)
{
    string title;
    string field = "title";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        title = parametersRoot.get(field, "XXX").asString();
    
    string subTitle;
    field = "SubTitle";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        subTitle = parametersRoot.get(field, "XXX").asString();

    string ingester;
    field = "Ingester";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        ingester = parametersRoot.get(field, "XXX").asString();

    string keywords;
    field = "Keywords";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        keywords = parametersRoot.get(field, "XXX").asString();

    string description;
    field = "Description";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        description = parametersRoot.get(field, "XXX").asString();

    string logicalType;
    field = "LogicalType";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        logicalType = parametersRoot.get(field, "XXX").asString();

    string contentProviderName;
    field = "ContentProviderName";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        contentProviderName = parametersRoot.get(field, "XXX").asString();
    
    string territories;
    field = "Territories";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        {
            Json::StreamWriterBuilder wbuilder;
            
            territories = Json::writeString(wbuilder, parametersRoot[field]);
        }
    }
    
    string mediaMetadata = string("")
        + "{"
            + "\"ContentType\": \"" + (video ? "video" : "audio") + "\""
            + ", \"SourceFileName\": \"" + sourceFileName + "\""
            ;
    if (title != "")
        mediaMetadata += ", \"Title\": \"" + title + "\"";
    if (subTitle != "")
        mediaMetadata += ", \"SubTitle\": \"" + subTitle + "\"";
    if (ingester != "")
        mediaMetadata += ", \"Ingester\": \"" + ingester + "\"";
    if (keywords != "")
        mediaMetadata += ", \"Keywords\": \"" + keywords + "\"";
    if (description != "")
        mediaMetadata += ", \"Description\": \"" + description + "\"";
    if (logicalType != "")
        mediaMetadata += ", \"LogicalType\": \"" + logicalType + "\"";
    if (contentProviderName != "")
        mediaMetadata += ", \"ContentProviderName\": \"" + contentProviderName + "\"";
    if (territories != "")
        mediaMetadata += ", \"Territories\": \"" + territories + "\"";
                            
    mediaMetadata +=
        string("}")
    ;
    
    _logger->info(__FILEREF__ + "Media metadata generated"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", mediaMetadata: " + mediaMetadata
            );

    return mediaMetadata;
}

void MMSEngineProcessor::handleCheckEncodingEvent ()
{
    vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> encodingItems;
    
    bool resetToBeDone = _firstGetEncodingJob ? true : false;
    
    _mmsEngineDBFacade->getEncodingJobs(resetToBeDone,
        _processorMMS, encodingItems);

    _firstGetEncodingJob = false;

    _pActiveEncodingsManager->addEncodingItems(encodingItems);
}

tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> MMSEngineProcessor::getMediaSourceDetails(
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace, MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot)        
{
    MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
    string mediaSourceURL;
    string mediaSourceFileName;
    
    string field;
    if (ingestionType == MMSEngineDBFacade::IngestionType::ContentIngestion)
    {
        field = "SourceURL";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            mediaSourceURL = parametersRoot.get(field, "XXX").asString();
        
        field = "SourceFileName";
        mediaSourceFileName = parametersRoot.get(field, "XXX").asString();

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
    get<2>(mediaSourceDetails) = mediaSourceFileName;
    get<3>(mediaSourceDetails) = md5FileCheckSum;
    get<4>(mediaSourceDetails) = fileSizeInBytes;

    _logger->info(__FILEREF__ + "media source details"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", nextIngestionStatus: " + MMSEngineDBFacade::toString(get<0>(mediaSourceDetails))
        + ", mediaSourceURL: " + get<1>(mediaSourceDetails)
        + ", mediaSourceFileName: " + get<2>(mediaSourceDetails)
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
        .append(".binary")
        ;

        
    for (int attemptIndex = 0; attemptIndex < _maxDownloadAttemptNumber && !downloadingCompleted; attemptIndex++)
    {
        bool downloadingStoppedByUser = false;
        
        try 
        {
            _logger->info(__FILEREF__ + "Downloading"
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
                    _logger->info(__FILEREF__ + "Setting SslEngineDefault");
                    request.setOpt(new curlpp::options::SslEngineDefault());
                }

                chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
                double lastPercentageUpdated = -1.0;
                curlpp::types::ProgressFunctionFunctor functor = bind(&MMSEngineProcessor::progressCallback, this,
                        ingestionJobKey, lastProgressUpdate, lastPercentageUpdated, downloadingStoppedByUser,
                        placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
                request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
                request.setOpt(new curlpp::options::NoProgress(0L));
                
                _logger->info(__FILEREF__ + "Downloading media file"
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
                    _logger->info(__FILEREF__ + "Setting SslEngineDefault");
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
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
                    );
                    
                    _logger->info(__FILEREF__ + "Update IngestionJob"
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
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
                    );
                    
                    _logger->info(__FILEREF__ + "Update IngestionJob"
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
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
                    );
                    
                    _logger->info(__FILEREF__ + "Update IngestionJob"
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
            .append(".binary")
            ;

        string movePrefix("move://");
        if (!(sourceReferenceURL.size() >= movePrefix.size() && 0 == sourceReferenceURL.compare(0, movePrefix.size(), movePrefix)))
        {
            string errorMessage = string("sourceReferenceURL is not a move reference")
                + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                + ", sourceReferenceURL: " + sourceReferenceURL 
            ;
            
            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }
        string sourcePathName = sourceReferenceURL.substr(movePrefix.length());
                
        _logger->info(__FILEREF__ + "Moving"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", sourcePathName: " + sourcePathName
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        
        FileIO::moveFile(sourcePathName, workspaceIngestionBinaryPathName);
            
        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", movingCompleted: " + to_string(true)
        );                            
        _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
            ingestionJobKey, true);
    }
    catch (runtime_error& e) 
    {
        _logger->error(__FILEREF__ + "Moving failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        _logger->info(__FILEREF__ + "Update IngestionJob"
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        _logger->info(__FILEREF__ + "Update IngestionJob"
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
            .append(".binary")
            ;

        string copyPrefix("copy://");
        if (!(sourceReferenceURL.size() >= copyPrefix.size() && 0 == sourceReferenceURL.compare(0, copyPrefix.size(), copyPrefix)))
        {
            string errorMessage = string("sourceReferenceURL is not a copy reference")
                + ", ingestionJobKey: " + to_string(ingestionJobKey) 
                + ", sourceReferenceURL: " + sourceReferenceURL 
            ;
            
            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }
        string sourcePathName = sourceReferenceURL.substr(copyPrefix.length());

        _logger->info(__FILEREF__ + "Coping"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", sourcePathName: " + sourcePathName
            + ", workspaceIngestionBinaryPathName: " + workspaceIngestionBinaryPathName
        );
        
        FileIO::copyFile(sourcePathName, workspaceIngestionBinaryPathName);
            
        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", movingCompleted: " + to_string(true)
        );              
        
        _mmsEngineDBFacade->updateIngestionJobSourceBinaryTransferred (
            ingestionJobKey, true);
    }
    catch (runtime_error& e) 
    {
        _logger->error(__FILEREF__ + "Coping failed"
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        _logger->info(__FILEREF__ + "Update IngestionJob"
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        _logger->info(__FILEREF__ + "Update IngestionJob"
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