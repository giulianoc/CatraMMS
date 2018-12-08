
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
        shared_ptr<long> processorsThreadsNumber,
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
    _processorsThreadsNumber = processorsThreadsNumber;
    _pActiveEncodingsManager = pActiveEncodingsManager;

    _processorMMS                   = System::getHostName();
    
    _processorThreads =  configuration["mms"].get("processorThreads", 1).asInt();
    _maxAdditionalProcessorThreads =  configuration["mms"].get("maxAdditionalProcessorThreads", 1).asInt();

    _maxDownloadAttemptNumber       = configuration["download"].get("maxDownloadAttemptNumber", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", download->maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
    );
    _progressUpdatePeriodInSeconds  = configuration["download"].get("progressUpdatePeriodInSeconds", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", download->progressUpdatePeriodInSeconds: " + to_string(_progressUpdatePeriodInSeconds)
    );
    _secondsWaitingAmongDownloadingAttempt  = configuration["download"].get("secondsWaitingAmongDownloadingAttempt", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", download->secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
    );
    
    _maxIngestionJobsPerEvent       = configuration["mms"].get("maxIngestionJobsPerEvent", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->maxIngestionJobsPerEvent: " + to_string(_maxIngestionJobsPerEvent)
    );
    // _maxIngestionJobsWithDependencyToCheckPerEvent = configuration["mms"].get("maxIngestionJobsWithDependencyToCheckPerEvent", 5).asInt();

    _dependencyExpirationInHours        = configuration["mms"].get("dependencyExpirationInHours", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->dependencyExpirationInHours: " + to_string(_dependencyExpirationInHours)
    );
    _stagingRetentionInDays             = configuration["mms"].get("stagingRetentionInDays", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->stagingRetentionInDays: " + to_string(_stagingRetentionInDays)
    );
    _downloadChunkSizeInMegaBytes       = configuration["download"].get("downloadChunkSizeInMegaBytes", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", download->downloadChunkSizeInMegaBytes: " + to_string(_downloadChunkSizeInMegaBytes)
    );
    
    _emailProtocol                      = _configuration["EmailNotification"].get("protocol", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", EmailNotification->protocol: " + _emailProtocol
    );
    _emailServer                        = _configuration["EmailNotification"].get("server", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", EmailNotification->server: " + _emailServer
    );
    _emailPort                          = _configuration["EmailNotification"].get("port", "XXX").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", EmailNotification->port: " + to_string(_emailPort)
    );
    _emailUserName                      = _configuration["EmailNotification"].get("userName", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", EmailNotification->userName: " + _emailUserName
    );
    string _emailPassword;
    {
        string encryptedPassword = _configuration["EmailNotification"].get("password", "XXX").asString();
        _emailPassword = Encrypt::decrypt(encryptedPassword);        
    }
    _emailFrom                          = _configuration["EmailNotification"].get("from", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", EmailNotification->from: " + _emailFrom
    );
    
    _facebookGraphAPIProtocol           = _configuration["FacebookGraphAPI"].get("protocol", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", FacebookGraphAPI->protocol: " + _facebookGraphAPIProtocol
    );
    _facebookGraphAPIHostName           = _configuration["FacebookGraphAPI"].get("hostName", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", FacebookGraphAPI->hostName: " + _facebookGraphAPIHostName
    );
    _facebookGraphAPIPort               = _configuration["FacebookGraphAPI"].get("port", 0).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", FacebookGraphAPI->port: " + to_string(_facebookGraphAPIPort)
    );
    _facebookGraphAPIVersion           = _configuration["FacebookGraphAPI"].get("version", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", FacebookGraphAPI->version: " + _facebookGraphAPIVersion
    );
    _facebookGraphAPITimeoutInSeconds   = _configuration["FacebookGraphAPI"].get("timeout", 0).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", FacebookGraphAPI->timeout: " + to_string(_facebookGraphAPITimeoutInSeconds)
    );

    _youTubeDataAPIProtocol           = _configuration["YouTubeDataAPI"].get("protocol", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", YouTubeDataAPI->protocol: " + _youTubeDataAPIProtocol
    );
    _youTubeDataAPIHostName           = _configuration["YouTubeDataAPI"].get("hostName", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", YouTubeDataAPI->hostName: " + _youTubeDataAPIHostName
    );
    _youTubeDataAPIPort               = _configuration["YouTubeDataAPI"].get("port", 0).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", YouTubeDataAPI->port: " + to_string(_youTubeDataAPIPort)
    );
    _youTubeDataAPIVersion           = _configuration["YouTubeDataAPI"].get("version", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", YouTubeDataAPI->version: " + _youTubeDataAPIVersion
    );
    _youTubeDataAPITimeoutInSeconds   = _configuration["YouTubeDataAPI"].get("timeout", 0).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", YouTubeDataAPI->timeout: " + to_string(_youTubeDataAPITimeoutInSeconds)
    );

    _localCopyTaskEnabled               =  _configuration["mms"].get("localCopyTaskEnabled", "XXX").asBool();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->localCopyTaskEnabled: " + to_string(_localCopyTaskEnabled)
    );

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
                    if (_processorsThreadsNumber.use_count() > _processorThreads + _maxAdditionalProcessorThreads)
                    {
                        // content retention is a periodical event, we will wait the next one
                        
                        _logger->info(__FILEREF__ + "Not enough available threads to manage handleContentRetentionEventThread, activity is postponed"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                            + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
                        );
                    }
                    else
                    {
                        thread contentRetention(&MMSEngineProcessor::handleContentRetentionEventThread, this,
                            _processorsThreadsNumber);
                        contentRetention.detach();
                    }
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleContentRetentionEventThread failed"
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

                    vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>> dependencies;
                    try
                    {
                        Validator validator(_logger, _mmsEngineDBFacade, _configuration);
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
                                    if (_processorsThreadsNumber.use_count() > _processorThreads + _maxAdditionalProcessorThreads)
                                    {
                                        _logger->info(__FILEREF__ + "Not enough available threads to manage downloadMediaSourceFileThread, activity is postponed"
                                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                            + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                                            + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
                                        );

                                        string errorMessage = "";
                                        string processorMMS = "";

                                        _logger->info(__FILEREF__ + "Update IngestionJob"
                                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                            + ", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                                            + ", errorMessage: " + errorMessage
                                            + ", processorMMS: " + processorMMS
                                        );                            
                                        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                                ingestionStatus,
                                                errorMessage,
                                                processorMMS
                                                );
                                    }
                                    else
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

                                        thread downloadMediaSource(&MMSEngineProcessor::downloadMediaSourceFileThread, this, 
                                            _processorsThreadsNumber, mediaSourceURL, ingestionJobKey, workspace);
                                        downloadMediaSource.detach();
                                    }                                    
                                }
                                else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress)
                                {
                                    if (_processorsThreadsNumber.use_count() > _processorThreads + _maxAdditionalProcessorThreads)
                                    {
                                        _logger->info(__FILEREF__ + "Not enough available threads to manage moveMediaSourceFileThread, activity is postponed"
                                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                            + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                                            + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
                                        );

                                        string errorMessage = "";
                                        string processorMMS = "";

                                        _logger->info(__FILEREF__ + "Update IngestionJob"
                                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                            + ", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                                            + ", errorMessage: " + errorMessage
                                            + ", processorMMS: " + processorMMS
                                        );                            
                                        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                                ingestionStatus,
                                                errorMessage,
                                                processorMMS
                                                );
                                    }
                                    else
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
                                        
                                        thread moveMediaSource(&MMSEngineProcessor::moveMediaSourceFileThread, this, 
                                            _processorsThreadsNumber, mediaSourceURL, ingestionJobKey, workspace);
                                        moveMediaSource.detach();
                                    }
                                }
                                else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress)
                                {
                                    if (_processorsThreadsNumber.use_count() > _processorThreads + _maxAdditionalProcessorThreads)
                                    {
                                        _logger->info(__FILEREF__ + "Not enough available threads to manage copyMediaSourceFileThread, activity is postponed"
                                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                            + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                                            + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
                                        );

                                        string errorMessage = "";
                                        string processorMMS = "";

                                        _logger->info(__FILEREF__ + "Update IngestionJob"
                                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                            + ", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                                            + ", errorMessage: " + errorMessage
                                            + ", processorMMS: " + processorMMS
                                        );                            
                                        _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                                ingestionStatus, 
                                                errorMessage,
                                                processorMMS
                                                );
                                    }
                                    else
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

                                        thread copyMediaSource(&MMSEngineProcessor::copyMediaSourceFileThread, this, 
                                            _processorsThreadsNumber, mediaSourceURL, ingestionJobKey, workspace);
                                        copyMediaSource.detach();
                                    }
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
                                removeContentTask(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "removeContentTask failed"
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
                                _logger->error(__FILEREF__ + "removeContentTask failed"
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
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::FTPDelivery)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                ftpDeliveryContentTask(
                                        ingestionJobKey, 
                                        ingestionStatus,
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "ftpDeliveryContentTask failed"
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
                                _logger->error(__FILEREF__ + "ftpDeliveryContentTask failed"
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
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::LocalCopy)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                if (!_localCopyTaskEnabled)
                                {
                                    string errorMessage = string("Local-Copy Task is not enabled in this MMS deploy")
                                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    ;
                                    _logger->error(__FILEREF__ + errorMessage);

                                    throw runtime_error(errorMessage);
                                }
                                
                                localCopyContentTask(
                                        ingestionJobKey,
                                        ingestionStatus,
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "localCopyContentTask failed"
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
                                _logger->error(__FILEREF__ + "localCopyContentTask failed"
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
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::HTTPCallback)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                httpCallbackTask(
                                        ingestionJobKey,
                                        ingestionStatus,
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "httpCallbackTask failed"
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
                                _logger->error(__FILEREF__ + "httpCallbackTask failed"
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
                                else // Frame
                                {
                                    generateAndIngestFramesTask(
                                        ingestionJobKey, 
                                        workspace, 
                                        ingestionType,
                                        parametersRoot, 
                                        dependencies);
                                }
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestFramesTask failed"
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
                                _logger->error(__FILEREF__ + "generateAndIngestFramesTask failed"
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
                                manageSlideShowTask(
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
                                generateAndIngestConcatenationTask(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestConcatenationTask failed"
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
                                _logger->error(__FILEREF__ + "generateAndIngestConcatenationTask failed"
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
                                generateAndIngestCutMediaTask(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestCutMediaTask failed"
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
                                _logger->error(__FILEREF__ + "generateAndIngestCutMediaTask failed"
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
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::ExtractTracks)
                        {
                            try
                            {
                                if (_processorsThreadsNumber.use_count() > _processorThreads + _maxAdditionalProcessorThreads)
                                {
                                    _logger->info(__FILEREF__ + "Not enough available threads to manage extractTracksContentThread, activity is postponed"
                                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                                        + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
                                    );

                                    string errorMessage = "";
                                    string processorMMS = "";

                                    _logger->info(__FILEREF__ + "Update IngestionJob"
                                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                                        + ", errorMessage: " + errorMessage
                                        + ", processorMMS: " + processorMMS
                                    );                            
                                    _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                            ingestionStatus, 
                                            errorMessage,
                                            processorMMS
                                            );
                                }
                                else
                                {
                                    thread extractTracksContentThread(&MMSEngineProcessor::extractTracksContentThread, this, 
                                        _processorsThreadsNumber, ingestionJobKey, 
                                            workspace, 
                                            parametersRoot,
                                            dependencies    // it cannot be passed as reference because it will change soon by the parent thread
                                            );
                                    extractTracksContentThread.detach();
                                }
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "extractTracksContentThread failed"
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
                                _logger->error(__FILEREF__ + "extractTracksContentThread failed"
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
                                manageEmailNotificationTask(
                                        ingestionJobKey, 
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "manageEmailNotificationTask failed"
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
                                _logger->error(__FILEREF__ + "manageEmailNotificationTask failed"
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
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::PostOnFacebook)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                postOnFacebookTask(
                                        ingestionJobKey, 
                                        ingestionStatus,
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "postOnFacebookTask failed"
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
                                _logger->error(__FILEREF__ + "postOnFacebookTask failed"
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
                        else if (ingestionType == MMSEngineDBFacade::IngestionType::PostOnYouTube)
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                postOnYouTubeTask(
                                        ingestionJobKey, 
                                        ingestionStatus,
                                        workspace, 
                                        parametersRoot, 
                                        dependencies);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "postOnYouTubeTask failed"
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
                                _logger->error(__FILEREF__ + "postOnYouTubeTask failed"
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
    vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>> dependencies;
    Json::Value parametersRoot;
    Validator validator(_logger, _mmsEngineDBFacade, _configuration);
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
                + ", errorMessage: " + e.what()
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

void MMSEngineProcessor::removeContentTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
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

        for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            int64_t key;
            MMSEngineDBFacade::ContentType referenceContentType;
            Validator::DependencyType dependencyType;
            
            tie(key, referenceContentType, dependencyType) = keyAndDependencyType;
            
            if (dependencyType == Validator::DependencyType::MediaItemKey)
            {
                _logger->info(__FILEREF__ + "removeMediaItem"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", mediaItemKey: " + to_string(key)
                );
                _mmsStorage->removeMediaItem(key);
            }
            else
            {
                _logger->info(__FILEREF__ + "removePhysicalPath"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", physicalPathKey: " + to_string(key)
                );
                _mmsStorage->removePhysicalPath(key);
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
        _logger->error(__FILEREF__ + "removeContentTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        // Update IngestionJob done in the calling method
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "removeContentTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::ftpDeliveryContentTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any media to be uploaded (FTP)"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (_processorsThreadsNumber.use_count() + dependencies.size() > _processorThreads + _maxAdditionalProcessorThreads)
        {
            _logger->info(__FILEREF__ + "Not enough available threads to manage ftpUploadMediaSourceThread, activity is postponed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
            );

            string errorMessage = "";
            string processorMMS = "";

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                    ingestionStatus, 
                    errorMessage,
                    processorMMS
                    );
            
            return;
        }

        string ftpServer;
        int ftpPort;
        string ftpUserName;
        string ftpPassword;
        string ftpRemoteDirectory;
        {
            string field = "Server";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            ftpServer = parametersRoot.get(field, "XXX").asString();

            field = "Port";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                ftpPort = 21;
            }
            else
                ftpPort = parametersRoot.get(field, "XXX").asInt();

            field = "UserName";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            ftpUserName = parametersRoot.get(field, "XXX").asString();

            field = "Password";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            ftpPassword = parametersRoot.get(field, "XXX").asString();

            field = "RemoteDirectory";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
                ftpRemoteDirectory = parametersRoot.get(field, "XXX").asString();
        }
        
        for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            int mmsPartitionNumber;
            string workspaceDirectoryName;
            string relativePath;
            string fileName;
            int64_t sizeInBytes;
            string deliveryFileName;
            
            int64_t key;
            MMSEngineDBFacade::ContentType referenceContentType;
            Validator::DependencyType dependencyType;
            
            tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

            if (dependencyType == Validator::DependencyType::MediaItemKey)
            {
                int64_t encodingProfileKey = -1;
                
                tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(
                        key, encodingProfileKey);

                int64_t physicalPathKey;
                shared_ptr<Workspace> workspace;
                string title;
                
                tie(physicalPathKey, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;
            }
            else
            {
                tuple<int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(key);

                shared_ptr<Workspace> workspace;
                string title;
                
                tie(mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;
            }

            _logger->info(__FILEREF__ + "getMMSAssetPathName ..."
                + ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
                + ", workspaceDirectoryName: " + workspaceDirectoryName
                + ", relativePath: " + relativePath
                + ", fileName: " + fileName
            );
            string mmsAssetPathName = _mmsStorage->getMMSAssetPathName(
                mmsPartitionNumber,
                workspaceDirectoryName,
                relativePath,
                fileName);
            
            // check on thread availability was done at the beginning in this method
            thread ftpUploadMediaSource(&MMSEngineProcessor::ftpUploadMediaSourceThread, this, 
                _processorsThreadsNumber, mmsAssetPathName, fileName, sizeInBytes, ingestionJobKey, workspace,
                    ftpServer, ftpPort, ftpUserName, ftpPassword,
                    ftpRemoteDirectory, deliveryFileName);
            ftpUploadMediaSource.detach();
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ftpDeliveryContentTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
 
        // Update IngestionJob done in the calling method
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ftpDeliveryContentTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::postOnFacebookTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any media to be posted on Facebook"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (_processorsThreadsNumber.use_count() + dependencies.size() > _processorThreads + _maxAdditionalProcessorThreads)
        {
            _logger->info(__FILEREF__ + "Not enough available threads to manage postOnFacebookTask, activity is postponed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
            );

            string errorMessage = "";
            string processorMMS = "";

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                    ingestionStatus, 
                    errorMessage,
                    processorMMS
                    );
            
            return;
        }

        string facebookAccessToken;
        string facebookNodeId;
        {
            string field = "AccessToken";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            facebookAccessToken = parametersRoot.get(field, "XXX").asString();

            field = "NodeId";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            facebookNodeId = parametersRoot.get(field, "XXX").asString();
        }
        
        for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            int mmsPartitionNumber;
            string workspaceDirectoryName;
            string relativePath;
            string fileName;
            int64_t sizeInBytes;
            string deliveryFileName;
            MMSEngineDBFacade::ContentType contentType;
            
            int64_t key;
            MMSEngineDBFacade::ContentType referenceContentType;
            Validator::DependencyType dependencyType;
            
            tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

            if (dependencyType == Validator::DependencyType::MediaItemKey)
            {
                int64_t encodingProfileKey = -1;
                
                tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(
                        key, encodingProfileKey);

                int64_t physicalPathKey;
                shared_ptr<Workspace> workspace;
                string title;
                
                tie(physicalPathKey, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;

                {
                    bool warningIfMissing = false;
                    pair<MMSEngineDBFacade::ContentType,string> contentTypeAndUserData =
                        _mmsEngineDBFacade->getMediaItemKeyDetails(
                            key, warningIfMissing);

                    string userData;
                    tie(contentType, userData) = contentTypeAndUserData;
                }
            }
            else
            {
                tuple<int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(key);

                shared_ptr<Workspace> workspace;
                string title;
                
                tie(mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;
                
                {
                    bool warningIfMissing = false;
                    tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                            key, warningIfMissing);

                    int64_t mediaItemKey;
                    string userData;
                    tie(mediaItemKey, contentType, userData)
                            = mediaItemKeyContentTypeAndUserData;
                }
            }

            _logger->info(__FILEREF__ + "getMMSAssetPathName ..."
                + ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
                + ", workspaceDirectoryName: " + workspaceDirectoryName
                + ", relativePath: " + relativePath
                + ", fileName: " + fileName
            );
            string mmsAssetPathName = _mmsStorage->getMMSAssetPathName(
                mmsPartitionNumber,
                workspaceDirectoryName,
                relativePath,
                fileName);

            // check on thread availability was done at the beginning in this method
            if (contentType == MMSEngineDBFacade::ContentType::Video)
            {
                thread postOnFacebook(&MMSEngineProcessor::postVideoOnFacebookThread, this,
                    _processorsThreadsNumber, mmsAssetPathName, 
                    sizeInBytes, ingestionJobKey, workspace,
                    facebookNodeId, facebookAccessToken);
                postOnFacebook.detach();
            }
            else // if (contentType == ContentType::Audio)
            {
                /*
                thread postOnFacebook(&MMSEngineProcessor::postVideoOnFacebookThread, this,
                    _processorsThreadsNumber, mmsAssetPathName, 
                    sizeInBytes, ingestionJobKey, workspace,
                    facebookNodeId, facebookAccessToken);
                postOnFacebook.detach();
                */
            }
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "postOnFacebookTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
 
        // Update IngestionJob done in the calling method
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "postOnFacebookTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::postOnYouTubeTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any media to be posted on YouTube"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (_processorsThreadsNumber.use_count() + dependencies.size() > _processorThreads + _maxAdditionalProcessorThreads)
        {
            _logger->info(__FILEREF__ + "Not enough available threads to manage postOnYouTubeTask, activity is postponed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
            );

            string errorMessage = "";
            string processorMMS = "";

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                    ingestionStatus, 
                    errorMessage,
                    processorMMS
                    );
            
            return;
        }

        string youTubeAuthorizationToken;
        string youTubeTitle;
        string youTubeDescription;
        Json::Value youTubeTags = Json::nullValue;
        int youTubeCategoryId = -1;
        {
            string field = "AuthorizationToken";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            youTubeAuthorizationToken = parametersRoot.get(field, "XXX").asString();

            field = "Title";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
                youTubeTitle = parametersRoot.get(field, "XXX").asString();

            field = "Description";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
                youTubeDescription = parametersRoot.get(field, "XXX").asString();
            
            field = "Tags";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
                youTubeTags = parametersRoot[field];
            
            field = "CategoryId";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
                youTubeCategoryId = parametersRoot.get(field, "XXX").asInt();
        }
        
        for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            int mmsPartitionNumber;
            string workspaceDirectoryName;
            string relativePath;
            string fileName;
            int64_t sizeInBytes;
            string deliveryFileName;
            MMSEngineDBFacade::ContentType contentType;
            string title;
            
            int64_t key;
            MMSEngineDBFacade::ContentType referenceContentType;
            Validator::DependencyType dependencyType;
            
            tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

            if (dependencyType == Validator::DependencyType::MediaItemKey)
            {
                int64_t encodingProfileKey = -1;
                
                tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(
                        key, encodingProfileKey);

                int64_t physicalPathKey;
                shared_ptr<Workspace> workspace;
                
                tie(physicalPathKey, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;

                {
                    bool warningIfMissing = false;
                    pair<MMSEngineDBFacade::ContentType,string> contentTypeAndUserData =
                        _mmsEngineDBFacade->getMediaItemKeyDetails(
                            key, warningIfMissing);

                    string userData;
                    tie(contentType, userData) = contentTypeAndUserData;
                }
            }
            else
            {
                tuple<int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(key);

                shared_ptr<Workspace> workspace;
                
                tie(mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;
                
                {
                    bool warningIfMissing = false;
                    tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                            key, warningIfMissing);

                    int64_t mediaItemKey;
                    string userData;
                    tie(mediaItemKey, contentType, userData)
                            = mediaItemKeyContentTypeAndUserData;
                }
            }
            
            if (youTubeTitle == "")
                youTubeTitle = title;

            _logger->info(__FILEREF__ + "getMMSAssetPathName ..."
                + ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
                + ", workspaceDirectoryName: " + workspaceDirectoryName
                + ", relativePath: " + relativePath
                + ", fileName: " + fileName
            );
            string mmsAssetPathName = _mmsStorage->getMMSAssetPathName(
                mmsPartitionNumber,
                workspaceDirectoryName,
                relativePath,
                fileName);

            // check on thread availability was done at the beginning in this method
            thread postOnYouTube(&MMSEngineProcessor::postVideoOnYouTubeThread, this,
                _processorsThreadsNumber, mmsAssetPathName, 
                sizeInBytes, ingestionJobKey, workspace,
                youTubeAuthorizationToken, youTubeTitle,
                youTubeDescription, youTubeTags,
                youTubeCategoryId);
            postOnYouTube.detach();
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "postOnYouTubeTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
 
        // Update IngestionJob done in the calling method
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "postOnYouTubeTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::httpCallbackTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
)
{
    try
    {
        /*
         * dependencies could be even empty
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any media to be notified (HTTP Callback)"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
         */

        if (_processorsThreadsNumber.use_count() > _processorThreads + _maxAdditionalProcessorThreads)
        {
            _logger->info(__FILEREF__ + "Not enough available threads to manage userHttpCallbackThread, activity is postponed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
            );

            string errorMessage = "";
            string processorMMS = "";

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                    ingestionStatus, 
                    errorMessage,
                    processorMMS
                    );
            
            return;
        }

        string httpProtocol;
        string httpHostName;
        int httpPort;
        string httpURI;
        string httpURLParameters;
        string httpMethod;
        long callbackTimeoutInSeconds;
        Json::Value httpHeadersRoot(Json::arrayValue);
        {
            string field = "Protocol";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                httpProtocol = "http";
            }
            else
            {
                httpProtocol = parametersRoot.get(field, "XXX").asString();
                if (httpProtocol == "")
                    httpProtocol = "http";
            }
            _logger->info(__FILEREF__ + "Retrieved configuration parameter"
                    + ", httpProtocol: " + httpProtocol
            );

            field = "HostName";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            httpHostName = parametersRoot.get(field, "XXX").asString();
            _logger->info(__FILEREF__ + "Retrieved configuration parameter"
                    + ", httpHostName: " + httpHostName
            );

            field = "Port";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                if (httpProtocol == "http")
                    httpPort = 80;
                else
                    httpPort = 443;
            }
            else
                httpPort = parametersRoot.get(field, "XXX").asInt();
            _logger->info(__FILEREF__ + "Retrieved configuration parameter"
                    + ", httpPort: " + to_string(httpPort)
            );

            field = "Timeout";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
                callbackTimeoutInSeconds = 120;
            else
                callbackTimeoutInSeconds = parametersRoot.get(field, "XXX").asInt();
            _logger->info(__FILEREF__ + "Retrieved configuration parameter"
                    + ", callbackTimeoutInSeconds: " + to_string(callbackTimeoutInSeconds)
            );
            
            field = "URI";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            httpURI = parametersRoot.get(field, "XXX").asString();
            _logger->info(__FILEREF__ + "Retrieved configuration parameter"
                    + ", httpURI: " + httpURI
            );

            field = "Parameters";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                httpURLParameters = parametersRoot.get(field, "XXX").asString();
            }
            _logger->info(__FILEREF__ + "Retrieved configuration parameter"
                    + ", httpURLParameters: " + httpURLParameters
            );

            field = "Method";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                httpMethod = "POST";
            }
            else
            {
                httpMethod = parametersRoot.get(field, "XXX").asString();
                if (httpMethod == "")
                    httpMethod = "POST";
            }
            _logger->info(__FILEREF__ + "Retrieved configuration parameter"
                    + ", httpMethod: " + httpMethod
            );
            
            field = "Headers";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                httpHeadersRoot = parametersRoot[field];
            }
        }

        Json::Value callbackMedatada;
        {
            for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
            {
                int64_t key;
                MMSEngineDBFacade::ContentType referenceContentType;
                Validator::DependencyType dependencyType;

                tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

                if (dependencyType == Validator::DependencyType::MediaItemKey)
                {
                    callbackMedatada["mediaItemKey"] = key;

                    bool warningIfMissing = false;
                    pair<MMSEngineDBFacade::ContentType,string> contentTypeAndUserData =
                        _mmsEngineDBFacade->getMediaItemKeyDetails(
                            key, warningIfMissing);

                    MMSEngineDBFacade::ContentType contentType;
                    string userData;
                    tie(contentType, userData) = contentTypeAndUserData;

                    if (userData == "")
                        callbackMedatada["userData"] = Json::nullValue;
                    else
                    {
                        Json::Value userDataRoot;
                        {
                            Json::CharReaderBuilder builder;
                            Json::CharReader* reader = builder.newCharReader();
                            string errors;

                            bool parsingSuccessful = reader->parse(userData.c_str(),
                                    userData.c_str() + userData.size(), 
                                    &userDataRoot, &errors);
                            delete reader;

                            if (!parsingSuccessful)
                            {
                                string errorMessage = __FILEREF__ + "failed to parse the userData"
                                        + ", errors: " + errors
                                        + ", userData: " + userData
                                        ;
                                _logger->error(errorMessage);

                                throw runtime_error(errors);
                            }
                        }

                        callbackMedatada["userData"] = userDataRoot;
                    }
                }
                else
                {
                    callbackMedatada["physicalPathKey"] = key;

                    bool warningIfMissing = false;
                    tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                        _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                            key, warningIfMissing);

                    int64_t mediaItemKey;
                    MMSEngineDBFacade::ContentType contentType;
                    string userData;
                    tie(mediaItemKey, contentType, userData)
                            = mediaItemKeyContentTypeAndUserData;

                    callbackMedatada["mediaItemKey"] = mediaItemKey;

                    if (userData == "")
                        callbackMedatada["userData"] = Json::nullValue;
                    else
                    {
                        Json::Value userDataRoot;
                        {
                            Json::CharReaderBuilder builder;
                            Json::CharReader* reader = builder.newCharReader();
                            string errors;

                            bool parsingSuccessful = reader->parse(userData.c_str(),
                                    userData.c_str() + userData.size(), 
                                    &userDataRoot, &errors);
                            delete reader;

                            if (!parsingSuccessful)
                            {
                                string errorMessage = __FILEREF__ + "failed to parse the userData"
                                        + ", errors: " + errors
                                        + ", userData: " + userData
                                        ;
                                _logger->error(errorMessage);

                                throw runtime_error(errors);
                            }
                        }

                        callbackMedatada["userData"] = userDataRoot;
                    }
                }
            }
        }
        
        // check on thread availability was done at the beginning in this method
        thread httpCallbackThread(&MMSEngineProcessor::userHttpCallbackThread, this, 
            _processorsThreadsNumber, ingestionJobKey, httpProtocol, httpHostName, 
            httpPort, httpURI, httpURLParameters, httpMethod, callbackTimeoutInSeconds,
            httpHeadersRoot, callbackMedatada);
        httpCallbackThread.detach();
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "httpCallbackTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "httpCallbackTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::localCopyContentTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured any media to be copied"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (_processorsThreadsNumber.use_count() + dependencies.size() > _processorThreads + _maxAdditionalProcessorThreads)
        {
            _logger->info(__FILEREF__ + "Not enough available threads to manage copyContentThread, activity is postponed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
                + ", _processorThreads + _maxAdditionalProcessorThreads: " + to_string(_processorThreads + _maxAdditionalProcessorThreads)
            );

            string errorMessage = "";
            string processorMMS = "";

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + MMSEngineDBFacade::toString(ingestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                    ingestionStatus, 
                    errorMessage,
                    processorMMS
                    );
            
            return;
        }

        string localPath;
        string localFileName;
        {
            string field = "LocalPath";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            localPath = parametersRoot.get(field, "XXX").asString();

            field = "LocalFileName";
            if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                localFileName = parametersRoot.get(field, "XXX").asString();
            }
        }
        
        for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            int mmsPartitionNumber;
            string workspaceDirectoryName;
            string relativePath;
            string fileName;
            int64_t sizeInBytes;
            string deliveryFileName;
            
            int64_t key;
            MMSEngineDBFacade::ContentType referenceContentType;
            Validator::DependencyType dependencyType;
            
            tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

            if (dependencyType == Validator::DependencyType::MediaItemKey)
            {
                int64_t encodingProfileKey = -1;
                
                tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(
                        key, encodingProfileKey);

                int64_t physicalPathKey;
                shared_ptr<Workspace> workspace;
                string title;
                
                tie(physicalPathKey, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;
            }
            else
            {
                tuple<int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(key);

                shared_ptr<Workspace> workspace;
                string title;
                
                tie(mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;
            }

            _logger->info(__FILEREF__ + "getMMSAssetPathName ..."
                + ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
                + ", workspaceDirectoryName: " + workspaceDirectoryName
                + ", relativePath: " + relativePath
                + ", fileName: " + fileName
            );
            string mmsAssetPathName = _mmsStorage->getMMSAssetPathName(
                mmsPartitionNumber,
                workspaceDirectoryName,
                relativePath,
                fileName);
            
            // check on thread availability was done at the beginning in this method
            thread copyContentThread(&MMSEngineProcessor::copyContentThread, this, 
                _processorsThreadsNumber, ingestionJobKey, mmsAssetPathName, localPath, localFileName);
            copyContentThread.detach();
        }
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "localCopyContentTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
                
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "localCopyContentTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::copyContentThread(
        shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string mmsAssetPathName, 
        string localPath, string localFileName)
{

    try 
    {
        _logger->info(__FILEREF__ + "Coping"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsAssetPathName: " + mmsAssetPathName
            + ", localPath: " + localPath
            + ", localFileName: " + localFileName
        );
        
        string localPathName = localPath;
        if (localFileName != "")
        {
            if (localPathName.back() != '/')
                localPathName += "/";
            localPathName += localFileName;
        }            
        
        FileIO::copyFile(mmsAssetPathName, localPathName);
            
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
    catch (runtime_error& e) 
    {
        _logger->error(__FILEREF__ + "Coping failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", mmsAssetPathName: " + mmsAssetPathName 
            + ", localPath: " + localPath
            + ", localFileName: " + localFileName
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
            + ", mmsAssetPathName: " + mmsAssetPathName 
            + ", localPath: " + localPath
            + ", localFileName: " + localFileName
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

void MMSEngineProcessor::extractTracksContentThread(
        shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>> dependencies)

{
    try 
    {
        _logger->info(__FILEREF__ + "Extracting Tracks"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
                
        if (dependencies.size() == 0)
        {
            string errorMessage = __FILEREF__ + "No configured media to be used to extract a track"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        vector<pair<string,int>> tracksToBeExtracted;
        string outputFileFormat;
        {
            {
                string field = "Tracks";
                Json::Value tracksToot = parametersRoot[field];
                if (tracksToot.size() == 0)
                {
                    string errorMessage = __FILEREF__ + "No correct number of Tracks"
                            + ", tracksToot.size: " + to_string(tracksToot.size());
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                for (int trackIndex = 0; trackIndex < tracksToot.size(); trackIndex++)
                {
                    Json::Value trackRoot = tracksToot[trackIndex];

                    field = "TrackType";
                    if (!_mmsEngineDBFacade->isMetadataPresent(trackRoot, field))
                    {
                        Json::StreamWriterBuilder wbuilder;
                        string sTrackRoot = Json::writeString(wbuilder, trackRoot);

                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", Field: " + field
                                + ", sTrackRoot: " + sTrackRoot
                                ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    string trackType = trackRoot.get(field, "XXX").asString();

                    int trackNumber = 0;
                    field = "TrackNumber";
                    if (_mmsEngineDBFacade->isMetadataPresent(trackRoot, field))
                        trackNumber = trackRoot.get(field, "XXX").asInt();

                    tracksToBeExtracted.push_back(make_pair(trackType, trackNumber));
                }
            }

            string field = "OutputFileFormat";
            if (!_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            outputFileFormat = parametersRoot.get(field, "XXX").asString();
        }

        for(vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>::iterator it = dependencies.begin(); 
                it != dependencies.end(); ++it) 
        {
            tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType = *it;

            int64_t key;
            MMSEngineDBFacade::ContentType referenceContentType;
            Validator::DependencyType dependencyType;

            tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

            int mmsPartitionNumber;
            string workspaceDirectoryName;
            string relativePath;
            string fileName;
            shared_ptr<Workspace> workspace;
            
            if (dependencyType == Validator::DependencyType::MediaItemKey)
            {

                int64_t encodingProfileKey = -1;
                
                tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(
                        key, encodingProfileKey);

                int64_t physicalPathKey;
                string deliveryFileName;
                string title;
                int64_t sizeInBytes;
                
                tie(physicalPathKey, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;
            }
            else
            {
                tuple<int,shared_ptr<Workspace>,string,string,string,string,int64_t> storageDetails 
                    = _mmsEngineDBFacade->getStorageDetails(key);

                string deliveryFileName;
                string title;
                int64_t sizeInBytes;
                
                tie(mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes) 
                        = storageDetails;
                workspaceDirectoryName = workspace->_directoryName;
            }

            _logger->info(__FILEREF__ + "getMMSAssetPathName ..."
                + ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
                + ", workspaceDirectoryName: " + workspaceDirectoryName
                + ", relativePath: " + relativePath
                + ", fileName: " + fileName
            );
            string mmsAssetPathName = _mmsStorage->getMMSAssetPathName(
                mmsPartitionNumber,
                workspaceDirectoryName,
                relativePath,
                fileName);
            
            {
                string localSourceFileName;
                string extractTrackMediaPathName;
                {
                    localSourceFileName = to_string(ingestionJobKey)
                            + "_" + to_string(key)
                            + "_extractTrack"
                            + "." + outputFileFormat
                            ;

                    string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
                        workspace);
                    extractTrackMediaPathName = workspaceIngestionRepository + "/" 
                            + localSourceFileName;
                }

                FFMpeg ffmpeg (_configuration, _logger);

                ffmpeg.extractTrackMediaToIngest(
                    ingestionJobKey,
                    mmsAssetPathName,
                    tracksToBeExtracted,
                    extractTrackMediaPathName);

                _logger->info(__FILEREF__ + "extractTrackMediaToIngest done"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", extractTrackMediaPathName: " + extractTrackMediaPathName
                );

                string title;
                string mediaMetaDataContent = generateMediaMetadataToIngest(
                        ingestionJobKey,
                        outputFileFormat,
                        title,
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
                    localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(
                        it + 1 == dependencies.end() ? true : false);

                    // to manage a ffmpeg bug generating a corrupted/wrong avgFrameRate, we will
                    // force the concat file to have the same avgFrameRate of the source media
                    // Uncomment next statements in case the problem is still present event in case of the ExtractTracks task
                    // if (forcedAvgFrameRate != "" && concatContentType == MMSEngineDBFacade::ContentType::Video)
                    //    localAssetIngestionEvent->setForcedAvgFrameRate(forcedAvgFrameRate);            

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
        }
    }
    catch (runtime_error& e) 
    {
        _logger->error(__FILEREF__ + "Extracting tracks failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
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
        _logger->error(__FILEREF__ + "Extracting tracks failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
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
            string generatedFrames_BaseFileName = to_string(multiLocalAssetIngestionEvent->getIngestionJobKey());

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

                    if (directoryEntry.size() >= generatedFrames_BaseFileName.size() && 0 == directoryEntry.compare(0, generatedFrames_BaseFileName.size(), generatedFrames_BaseFileName))
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
           
        // we have one ingestion job row and one or more generated frames to be ingested
        // One MIK in case of a .mjpeg
        // One or more MIKs in case of .jpg
        // We want to update the ingestion row just once at the end,
        // in case of success or when an error happens.
        // To do this we will add a field in the localAssetIngestionEvent structure (ingestionRowToBeUpdatedAsSuccess)
        // and we will set it to false except for the last frame where we will set to true
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

                string title;
                {
                    string field = "Title";
                    if (_mmsEngineDBFacade->isMetadataPresent(multiLocalAssetIngestionEvent->getParametersRoot(), field))
                        title = multiLocalAssetIngestionEvent->getParametersRoot().get(field, "XXX").asString();                    
                    title += (
                            " (" 
                            + to_string(it - generatedFramesFileNames.begin() + 1) 
                            + " / "
                            + to_string(generatedFramesFileNames.size())
                            + ")"
                            );
                }
                string imageMetaDataContent = generateMediaMetadataToIngest(
                        multiLocalAssetIngestionEvent->getIngestionJobKey(),
                        // mjpeg,
                        fileFormat,
                        title,
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
        
        /*
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
        */
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

void MMSEngineProcessor::generateAndIngestFramesTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
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
                mjpeg, imageWidth, imageHeight,
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
                to_string(ingestionJobKey),    // imageBaseFileName,
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

                    string title;
                    string imageMetaDataContent = generateMediaMetadataToIngest(
                            ingestionJobKey,
                            fileFormat,
                            title,
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
        
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::manageGenerateFramesTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
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
                mjpeg, imageWidth, imageHeight,
                sourcePhysicalPathKey, sourcePhysicalPath,
                durationInMilliSeconds);

        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
                workspace);

        _mmsEngineDBFacade->addEncoding_GenerateFramesJob (
                workspace,
                ingestionJobKey, encodingPriority,
                workspaceIngestionRepository, 
                startTimeInSeconds, maxFramesNumber, 
                videoFilter, periodInSeconds, 
                mjpeg, imageWidth, imageHeight,
                sourcePhysicalPathKey,
                durationInMilliSeconds
                );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "manageGenerateFramesTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "manageGenerateFramesTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::fillGenerateFramesParameters(
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    MMSEngineDBFacade::IngestionType ingestionType,
    Json::Value parametersRoot,
    vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies,
        
    int& periodInSeconds, double& startTimeInSeconds,
    int& maxFramesNumber, string& videoFilter,
    bool& mjpeg, int& imageWidth, int& imageHeight,
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
        tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType = dependencies.back();

        int64_t key;
        MMSEngineDBFacade::ContentType referenceContentType;
        Validator::DependencyType dependencyType;

        tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

        if (dependencyType == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey = key;

            sourcePhysicalPathKey = -1;
            int64_t encodingProfileKey = -1;
            pair<int64_t,string> physicalPathKeyAndPhysicalPath 
                    = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);
            
            /*
            int64_t localPhysicalPathKey;
            tie(localPhysicalPathKey,sourcePhysicalPath) = physicalPathKeyAndPhysicalPath;
             */
            tie(sourcePhysicalPathKey,sourcePhysicalPath) = physicalPathKeyAndPhysicalPath;
        }
        else
        {
            sourcePhysicalPathKey = key;
            
            bool warningIfMissing = false;
            tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey, warningIfMissing);
    
            MMSEngineDBFacade::ContentType localContentType;
            string userData;
            tie(sourceMediaItemKey,localContentType, userData)
                    = mediaItemKeyContentTypeAndUserData;
            
            sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
        }
        
        /*
        _logger->info(__FILEREF__ + "fillGenerateFramesParameters. Looking for the media key"
            + ", key: " + to_string(key)
            + ", referenceContentType: " + MMSEngineDBFacade::toString(referenceContentType)
            + ", dependencyType: " + to_string(static_cast<int>(dependencyType))
            + ", key: " + to_string(key)
            + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
            + ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
            + ", sourcePhysicalPath: " + sourcePhysicalPath
        );
         */

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

        {
            // imageFileName = to_string(ingestionJobKey) + /* "_source" + */ ".jpg";
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

void MMSEngineProcessor::manageSlideShowTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
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

        MMSEngineDBFacade::ContentType slideshowContentType;
        bool slideshowContentTypeInitialized = false;
        vector<string> sourcePhysicalPaths;
        
        for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            // int64_t encodingProfileKey = -1;
            // string sourcePhysicalPath = _mmsStorage->getPhysicalPath(keyAndDependencyType.first, encodingProfileKey);

            int64_t sourceMediaItemKey;
            int64_t sourcePhysicalPathKey;
            string sourcePhysicalPath;

            int64_t key;
            MMSEngineDBFacade::ContentType referenceContentType;
            Validator::DependencyType dependencyType;

            tie(key, referenceContentType, dependencyType) = keyAndDependencyType;
        
            if (dependencyType == Validator::DependencyType::MediaItemKey)
            {
                sourceMediaItemKey = key;

                sourcePhysicalPathKey = -1;
                int64_t encodingProfileKey = -1;
                pair<int64_t,string> physicalPathKeyAndPhysicalPath 
                        = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);

                int64_t localPhysicalPathKey;
                tie(localPhysicalPathKey,sourcePhysicalPath) = physicalPathKeyAndPhysicalPath;
            }
            else
            {
                sourcePhysicalPathKey = key;

                bool warningIfMissing = false;
                tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                    _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        sourcePhysicalPathKey, warningIfMissing);

                MMSEngineDBFacade::ContentType localContentType;
                string userData;
                tie(sourceMediaItemKey,localContentType, userData)
                        = mediaItemKeyContentTypeAndUserData;

                sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
            }
            
            sourcePhysicalPaths.push_back(sourcePhysicalPath);
            
            bool warningIfMissing = false;
            
            pair<MMSEngineDBFacade::ContentType,string> contentTypeAndUserData 
                    = _mmsEngineDBFacade->getMediaItemKeyDetails(sourceMediaItemKey, warningIfMissing);
           
            MMSEngineDBFacade::ContentType contentType;
            string userData;
            tie(contentType, userData) = contentTypeAndUserData;
            
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

        double durationOfEachSlideInSeconds = 2;
        field = "DurationOfEachSlideInSeconds";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            durationOfEachSlideInSeconds = parametersRoot.get(field, "XXX").asDouble();
        }

        int outputFrameRate = 25;
        
        _mmsEngineDBFacade->addEncoding_SlideShowJob(workspace, ingestionJobKey,
                sourcePhysicalPaths, durationOfEachSlideInSeconds, 
                outputFrameRate, encodingPriority);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "manageSlideShowTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "manageSlideShowTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

/*
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

        double durationOfEachSlideInSeconds = 2;
        string field = "DurationOfEachSlideInSeconds";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            durationOfEachSlideInSeconds = parametersRoot.get(field, "XXX").asDouble();
        }

        int outputFrameRate = 25;
        
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

//        size_t extensionIndex = sourceFileName.find_last_of(".");
//        if (extensionIndex != string::npos)
//            localSourceFileName.append(sourceFileName.substr(extensionIndex));
        
        string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
            workspace);
        string slideshowMediaPathName = workspaceIngestionRepository + "/" 
                + localSourceFileName;
        
        FFMpeg ffmpeg (_configuration, _logger);
        ffmpeg.generateSlideshowMediaToIngest(ingestionJobKey, 
                sourcePhysicalPaths, durationOfEachSlideInSeconds,
                outputFrameRate, slideshowMediaPathName);

        _logger->info(__FILEREF__ + "generateSlideshowMediaToIngest done"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", slideshowMediaPathName: " + slideshowMediaPathName
        );
                            
        string title;
        string mediaMetaDataContent = generateMediaMetadataToIngest(
                ingestionJobKey,
                // true,
                fileFormat,
                title,
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
*/

void MMSEngineProcessor::generateAndIngestConcatenationTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() < 1)
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
        
        for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            // int64_t encodingProfileKey = -1;
            // string sourcePhysicalPath = _mmsStorage->getPhysicalPath(keyAndDependencyType.first, encodingProfileKey);

            int64_t key;
            MMSEngineDBFacade::ContentType referenceContentType;
            Validator::DependencyType dependencyType;

            tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

            int64_t sourceMediaItemKey;
            int64_t sourcePhysicalPathKey;
            string sourcePhysicalPath;
            if (dependencyType == Validator::DependencyType::MediaItemKey)
            {
                sourceMediaItemKey = key;

                sourcePhysicalPathKey = -1;
                int64_t encodingProfileKey = -1;
                pair<int64_t,string> physicalPathInfo = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);
                
                tie(sourcePhysicalPathKey,sourcePhysicalPath) = physicalPathInfo;
            }
            else
            {
                sourcePhysicalPathKey = key;

                bool warningIfMissing = false;
                tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                    _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        sourcePhysicalPathKey, warningIfMissing);

                MMSEngineDBFacade::ContentType localContentType;
                string userData;
                tie(sourceMediaItemKey,localContentType,userData)
                        = mediaItemKeyContentTypeAndUserData;

                sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
            }

            sourcePhysicalPaths.push_back(sourcePhysicalPath);
            
            bool warningIfMissing = false;
            tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData 
                    = _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                        sourcePhysicalPathKey, warningIfMissing);
            
            MMSEngineDBFacade::ContentType contentType;
            {
                int64_t localMediaItemKey;
                string userData;
                tie(localMediaItemKey, contentType, userData) = mediaItemKeyContentTypeAndUserData;                
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
        
        if (sourcePhysicalPaths.size() == 1)
        {
            string sourcePhysicalPath = sourcePhysicalPaths.at(0);
            _logger->info(__FILEREF__ + "Coping"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", sourcePhysicalPath: " + sourcePhysicalPath
                + ", concatenatedMediaPathName: " + concatenatedMediaPathName
            );

            FileIO::copyFile(sourcePhysicalPath, concatenatedMediaPathName);
        }
        else
        {
            FFMpeg ffmpeg (_configuration, _logger);
            ffmpeg.generateConcatMediaToIngest(ingestionJobKey, sourcePhysicalPaths, concatenatedMediaPathName);
        }

        _logger->info(__FILEREF__ + "generateConcatMediaToIngest done"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", concatenatedMediaPathName: " + concatenatedMediaPathName
        );
                
        string title;
        string mediaMetaDataContent = generateMediaMetadataToIngest(
                ingestionJobKey,
                // concatContentType == MMSEngineDBFacade::ContentType::Video ? true : false,
                fileFormat,
                title,
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
        _logger->error(__FILEREF__ + "generateAndIngestConcatenationTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestConcatenationTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::generateAndIngestCutMediaTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
)
{
    try
    {
        if (dependencies.size() != 1)
        {
            string errorMessage = __FILEREF__ + "Wrong number of media to be cut"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", dependencies.size: " + to_string(dependencies.size());
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        int64_t sourceMediaItemKey;
        int64_t sourcePhysicalPathKey;
        string sourcePhysicalPath;
        tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType = dependencies.back();

        int64_t key;
        MMSEngineDBFacade::ContentType referenceContentType;
        Validator::DependencyType dependencyType;

        tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

        if (dependencyType == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey = key;

            sourcePhysicalPathKey = -1;
            int64_t encodingProfileKey = -1;
            pair<int64_t,string> physicalPathKeyAndPhysicalPath
                    = _mmsStorage->getPhysicalPath(sourceMediaItemKey, encodingProfileKey);
            tie(sourcePhysicalPathKey,sourcePhysicalPath) = physicalPathKeyAndPhysicalPath;
        }
        else
        {
            sourcePhysicalPathKey = key;
            
            bool warningIfMissing = false;
            tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            string userData;
            tie(sourceMediaItemKey,localContentType,userData)
                    = mediaItemKeyContentTypeAndUserData;

            sourcePhysicalPath = _mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
        }

        bool warningIfMissing = false;

        pair<MMSEngineDBFacade::ContentType,string> contentTypeAndUserData 
                = _mmsEngineDBFacade->getMediaItemKeyDetails(sourceMediaItemKey, warningIfMissing);
        
        MMSEngineDBFacade::ContentType contentType;
        string userData;
        tie(contentType, userData) = contentTypeAndUserData;
        
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

        string outputFileFormat;
        field = "OutputFileFormat";
        if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
        {
            outputFileFormat = parametersRoot.get(field, "XXX").asString();
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
        if (outputFileFormat == "")
        {
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
        }
        else
        {
            fileFormat = outputFileFormat;
        }

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
        
        string title;
        string mediaMetaDataContent = generateMediaMetadataToIngest(
                ingestionJobKey,
                fileFormat,
                title,
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
        _logger->error(__FILEREF__ + "generateAndIngestCutMediaTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestCutMediaTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::manageEncodeTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
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

        int64_t sourceMediaItemKey;
        int64_t sourcePhysicalPathKey;
        tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType = dependencies.back();

        int64_t key;
        MMSEngineDBFacade::ContentType referenceContentType;
        Validator::DependencyType dependencyType;

        tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

        if (dependencyType == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey = key;

            sourcePhysicalPathKey = -1;
        }
        else
        {
            sourcePhysicalPathKey = key;
            
            bool warningIfMissing = false;
            tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            string userData;
            tie(sourceMediaItemKey,localContentType, userData)
                    = mediaItemKeyContentTypeAndUserData;
        }
    
        if (encodingProfileKey == -1)
            _mmsEngineDBFacade->addEncodingJob (workspace, ingestionJobKey,
                encodingProfileLabel, sourceMediaItemKey, sourcePhysicalPathKey, encodingPriority);
        else
            _mmsEngineDBFacade->addEncodingJob (workspace, ingestionJobKey,
                encodingProfileKey, sourceMediaItemKey, sourcePhysicalPathKey, encodingPriority);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestFrame failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::manageOverlayImageOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
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
        tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType_1 = dependencies[0];

        int64_t key_1;
        MMSEngineDBFacade::ContentType referenceContentType_1;
        Validator::DependencyType dependencyType_1;

        tie(key_1, referenceContentType_1, dependencyType_1) = keyAndDependencyType_1;

        if (dependencyType_1 == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey_1 = key_1;

            sourcePhysicalPathKey_1 = -1;
        }
        else
        {
            sourcePhysicalPathKey_1 = key_1;
            
            bool warningIfMissing = false;
            tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey_1, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            string userData;
            tie(sourceMediaItemKey_1,localContentType, userData)
                    = mediaItemKeyContentTypeAndUserData;
        }

        int64_t sourceMediaItemKey_2;
        int64_t sourcePhysicalPathKey_2;
        tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType_2 = dependencies[1];

        int64_t key_2;
        MMSEngineDBFacade::ContentType referenceContentType_2;
        Validator::DependencyType dependencyType_2;

        tie(key_2, referenceContentType_2, dependencyType_2) = keyAndDependencyType_1;

        if (dependencyType_2 == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey_2 = key_2;

            sourcePhysicalPathKey_2 = -1;
        }
        else
        {
            sourcePhysicalPathKey_2 = key_2;
            
            bool warningIfMissing = false;
            tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey_2, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            string userData;
            tie(sourceMediaItemKey_2,localContentType, userData)
                    = mediaItemKeyContentTypeAndUserData;
        }

        _mmsEngineDBFacade->addEncoding_OverlayImageOnVideoJob (workspace, ingestionJobKey,
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
        
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "manageOverlayImageOnVideoTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::manageOverlayTextOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
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
        tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType = dependencies[0];

        int64_t key;
        MMSEngineDBFacade::ContentType referenceContentType;
        Validator::DependencyType dependencyType;

        tie(key, referenceContentType, dependencyType) = keyAndDependencyType;

        if (dependencyType == Validator::DependencyType::MediaItemKey)
        {
            sourceMediaItemKey = key;

            sourcePhysicalPathKey = -1;
        }
        else
        {
            sourcePhysicalPathKey = key;
            
            bool warningIfMissing = false;
            tuple<int64_t,MMSEngineDBFacade::ContentType,string> mediaItemKeyContentTypeAndUserData =
                _mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
                    sourcePhysicalPathKey, warningIfMissing);

            MMSEngineDBFacade::ContentType localContentType;
            string userData;
            tie(sourceMediaItemKey,localContentType, userData)
                    = mediaItemKeyContentTypeAndUserData;
        }

        _mmsEngineDBFacade->addEncoding_OverlayTextOnVideoJob (
                workspace, ingestionJobKey, encodingPriority,
                
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
        
        // Update IngestionJob done in the calling method

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "manageOverlayTextOnVideoTask failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method

        throw e;
    }
}

void MMSEngineProcessor::manageEmailNotificationTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies
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
        for (tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>& keyAndDependencyType: dependencies)
        {
            int64_t key;
            MMSEngineDBFacade::ContentType referenceContentType;
            Validator::DependencyType dependencyType;

            tie(key, referenceContentType, dependencyType) = keyAndDependencyType;
        
            if (sIngestionJobJeyDependency == "")
                sIngestionJobJeyDependency = to_string(key);
            else
                sIngestionJobJeyDependency += (", " + to_string(key));
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
        
        // Update IngestionJob done in the calling method
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "sendEmail failed"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );
        
        // Update IngestionJob done in the calling method
        
        throw e;
    }
}

string MMSEngineProcessor::generateMediaMetadataToIngest(
        int64_t ingestionJobKey,
        string fileFormat,
        string title,
        Json::Value parametersRoot
)
{    
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
    
    field = "Title";
    if (title != "")
        parametersRoot[field] = title;

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

void MMSEngineProcessor::handleContentRetentionEventThread (
        shared_ptr<long> processorsThreadsNumber)
{
    
    {
        _logger->info(__FILEREF__ + "Content Retention started"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
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

void MMSEngineProcessor::downloadMediaSourceFileThread(
        shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL,
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
                + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
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
                curlpp::types::ProgressFunctionFunctor functor = bind(&MMSEngineProcessor::progressDownloadCallback, this,
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
                curlpp::types::ProgressFunctionFunctor functor = bind(&MMSEngineProcessor::progressDownloadCallback, this,
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

void MMSEngineProcessor::ftpUploadMediaSourceThread(
        shared_ptr<long> processorsThreadsNumber,
        string mmsAssetPathName, string fileName, int64_t sizeInBytes,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        string ftpServer, int ftpPort, string ftpUserName, string ftpPassword, 
        string ftpRemoteDirectory, string ftpRemoteFileName)
{

    // curl -T localfile.ext ftp://username:password@ftp.server.com/remotedir/remotefile.zip


    try 
    {
        string ftpUrl = string("ftp://") + ftpUserName + ":" + ftpPassword + "@" 
                + ftpServer 
                + ":" + to_string(ftpPort) 
                + ftpRemoteDirectory;
        
        if (ftpRemoteDirectory.size() == 0 || ftpRemoteDirectory.back() != '/')
            ftpUrl  += "/";

        if (ftpRemoteFileName == "")
            ftpUrl  += fileName;
        else
            ftpUrl += ftpRemoteFileName;

        _logger->info(__FILEREF__ + "FTP Uploading"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsAssetPathName: " + mmsAssetPathName
            + ", sizeInBytes: " + to_string(sizeInBytes)
            + ", ftpUrl: " + ftpUrl
        );

        ifstream mmsAssetStream(mmsAssetPathName, ifstream::binary);
        // FILE *mediaSourceFileStream = fopen(workspaceIngestionBinaryPathName.c_str(), "wb");

        // 1. PORT-mode FTP (Active) - NO Firewall friendly
        //  - FTP client: Sends a request to open a command channel from its TCP port (i.e.: 6000) to the FTP server’s TCP port 21
        //  - FTP client: Sends a data request (PORT command) to the FTP server. The FTP client includes in the PORT command the data port number 
        //      it opened to receive data. In this example, the FTP client has opened TCP port 6001 to receive the data.
        //  - FTP server opens a new inbound connection to the FTP client on the port indicated by the FTP client in the PORT command. 
        //      The FTP server source port is TCP port 20. In this example, the FTP server sends data from its own TCP port 20 to the FTP client’s TCP port 6001.
        //  In this conversation, two connections were established: an outbound connection initiated by the FTP client and an inbound connection established by the FTP server.
        // 2. PASV-mode FTP (Passive) - Firewall friendly
        //  - FTP client sends a request to open a command channel from its TCP port (i.e.: 6000) to the FTP server’s TCP port 21
        //  - FTP client sends a PASV command requesting that the FTP server open a port number that the FTP client can connect to establish the data channel.
        //      FTP serve sends over the command channel the TCP port number that the FTP client can initiate a connection to establish the data channel (i.e.: 7000)
        //  - FTP client opens a new connection from its own response port TCP 6001 to the FTP server’s data channel 7000. Data transfer takes place through this channel.
        
        // Active/Passive... see the next URL, section 'FTP Peculiarities We Need'
        // https://curl.haxx.se/libcurl/c/libcurl-tutorial.html

        // https://curl.haxx.se/libcurl/c/ftpupload.html
        curlpp::Cleanup cleaner;
        curlpp::Easy request;

        request.setOpt(new curlpp::options::Url(ftpUrl));
        request.setOpt(new curlpp::options::Verbose(false)); 
        request.setOpt(new curlpp::options::Upload(true)); 
        
        request.setOpt(new curlpp::options::ReadStream(&mmsAssetStream));
        request.setOpt(new curlpp::options::InfileSizeLarge((curl_off_t) sizeInBytes));
        
        
        bool bFtpUseEpsv = false;
        curlpp::OptionTrait<bool, CURLOPT_FTP_USE_EPSV> ftpUseEpsv(bFtpUseEpsv);
        request.setOpt(ftpUseEpsv);

        // curl will default to binary transfer mode for FTP, 
        // and you ask for ascii mode instead with -B, --use-ascii or 
        // by making sure the URL ends with ;type=A.
        
        // timeout (CURLOPT_FTP_RESPONSE_TIMEOUT)
        
        bool bCreatingMissingDir = true;
        curlpp::OptionTrait<bool, CURLOPT_FTP_CREATE_MISSING_DIRS> creatingMissingDir(bCreatingMissingDir);
        request.setOpt(creatingMissingDir);

        string ftpsPrefix("ftps");
        if (ftpUrl.size() >= ftpsPrefix.size() && 0 == ftpUrl.compare(0, ftpsPrefix.size(), ftpsPrefix))
        {
            /* Next statements is in case we want ftp protocol to use SSL or TLS
             * google CURLOPT_FTPSSLAUTH and CURLOPT_FTP_SSL

            // disconnect if we can't validate server's cert
            bool bSslVerifyPeer = false;
            curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
            request.setOpt(sslVerifyPeer);

            curlpp::OptionTrait<curl_ftpssl, CURLOPT_FTP_SSL> ftpSsl(CURLFTPSSL_TRY);
            request.setOpt(ftpSsl);

            curlpp::OptionTrait<curl_ftpauth, CURLOPT_FTPSSLAUTH> ftpSslAuth(CURLFTPAUTH_TLS);
            request.setOpt(ftpSslAuth);
             */
        }

        // FTP progress works only in case of FTP Passive
        chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
        double lastPercentageUpdated = -1.0;
        bool uploadingStoppedByUser = false;
        curlpp::types::ProgressFunctionFunctor functor = bind(&MMSEngineProcessor::progressUploadCallback, this,
                ingestionJobKey, lastProgressUpdate, lastPercentageUpdated, uploadingStoppedByUser,
                placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4);
        request.setOpt(new curlpp::options::ProgressFunction(curlpp::types::ProgressFunctionFunctor(functor)));
        request.setOpt(new curlpp::options::NoProgress(0L));

        _logger->info(__FILEREF__ + "FTP Uploading media file"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsAssetPathName: " + mmsAssetPathName
            + ", sizeInBytes: " + to_string(sizeInBytes)
        );
        request.perform();

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
    catch (curlpp::LogicError & e) 
    {
        _logger->error(__FILEREF__ + "Download failed (LogicError)"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", mmsAssetPathName: " + mmsAssetPathName 
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
                "");    // processorMMS

        return;
    }
    catch (curlpp::RuntimeError & e) 
    {
        _logger->error(__FILEREF__ + "Download failed (RuntimeError)"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", mmsAssetPathName: " + mmsAssetPathName 
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
                ""); // processorMMS

        return;
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "Download failed (exception)"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey) 
            + ", mmsAssetPathName: " + mmsAssetPathName 
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
                ""); // processorMMS

        return;
    }
}

size_t curlUploadVideoOnFacebookCallback(char* ptr, size_t size, size_t nmemb, void *f)
{
    MMSEngineProcessor::CurlUploadFacebookData* curlUploadData = (MMSEngineProcessor::CurlUploadFacebookData*) f;
    
    auto logger = spdlog::get("mmsEngineService");


    if (!curlUploadData->bodyFirstPartSent)
    {
        if (curlUploadData->bodyFirstPart.size() > size * nmemb)
        {
            logger->error(__FILEREF__ + "Not enougth memory!!!"
                + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
                + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
                + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
                + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
                + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
                + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
                + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
                + ", curlUploadData->bodyFirstPart.size(): " + to_string(curlUploadData->bodyFirstPart.size())
                + ", size * nmemb: " + to_string(size * nmemb)
            );

            return CURL_READFUNC_ABORT;
        }
        
        strcpy(ptr, curlUploadData->bodyFirstPart.c_str());
        
        curlUploadData->bodyFirstPartSent = true;

        logger->info(__FILEREF__ + "First read"
             + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
             + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
             + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
             + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
             + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
             + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
             + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
        );
        
        return curlUploadData->bodyFirstPart.size();
    }
    else if (curlUploadData->currentOffset == curlUploadData->endOffset)
    {
        if (!curlUploadData->bodyLastPartSent)
        {
            if (curlUploadData->bodyLastPart.size() > size * nmemb)
            {
                logger->error(__FILEREF__ + "Not enougth memory!!!"
                    + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
                    + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
                    + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
                    + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
                    + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
                    + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
                    + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
                    + ", curlUploadData->bodyLastPart.size(): " + to_string(curlUploadData->bodyLastPart.size())
                    + ", size * nmemb: " + to_string(size * nmemb)
                );

                return CURL_READFUNC_ABORT;
            }

            strcpy(ptr, curlUploadData->bodyLastPart.c_str());

            curlUploadData->bodyLastPartSent = true;

            logger->info(__FILEREF__ + "Last read"
                + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
                + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
                + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
                + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
                + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
                + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
                + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
            );

            return curlUploadData->bodyLastPart.size();
        }
        else
        {
            logger->error(__FILEREF__ + "This scenario should never happen because Content-Length was set"
                + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
                + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
                + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
                + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
                + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
                + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
                + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
            );

            return CURL_READFUNC_ABORT;
        }
    }

    if(curlUploadData->currentOffset + (size * nmemb) <= curlUploadData->endOffset)
        curlUploadData->mediaSourceFileStream.read(ptr, size * nmemb);
    else
        curlUploadData->mediaSourceFileStream.read(ptr, curlUploadData->endOffset - curlUploadData->currentOffset);

    int64_t charsRead = curlUploadData->mediaSourceFileStream.gcount();
    
    curlUploadData->currentOffset += charsRead;

    return charsRead;        
};

void MMSEngineProcessor::postVideoOnFacebookThread(
        shared_ptr<long> processorsThreadsNumber,
        string mmsAssetPathName, int64_t sizeInBytes,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        string facebookNodeId, string facebookAccessToken
        )
{
            
    string facebookURL;
    string sResponse;
    
    try
    {
        _logger->info(__FILEREF__ + "postVideoOnFacebookThread"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsAssetPathName: " + mmsAssetPathName
            + ", sizeInBytes: " + to_string(sizeInBytes)
            + ", facebookNodeId: " + facebookNodeId
            + ", facebookAccessToken: " + facebookAccessToken
        );
        
        string fileFormat;
        {
            size_t extensionIndex = mmsAssetPathName.find_last_of(".");
            if (extensionIndex == string::npos)
            {
                string errorMessage = __FILEREF__ + "No fileFormat (extension of the file) found in mmsAssetPathName"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", mmsAssetPathName: " + mmsAssetPathName
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            fileFormat = mmsAssetPathName.substr(extensionIndex + 1);
        }
        
        /*
            curl \
                -X POST "https://graph-video.facebook.com/v2.3/1533641336884006/videos"  \
                -F "access_token=XXXXXXXXX" \
                -F "upload_phase=start" \
                -F "file_size=152043520"

                {"upload_session_id":"1564747013773438","video_id":"1564747010440105","start_offset":"0","end_offset":"52428800"}
        */
        string uploadSessionId;
        string videoId;
        int64_t startOffset;
        int64_t endOffset;
        // start
        {
            string facebookURI = string("/") + _facebookGraphAPIVersion + "/" + facebookNodeId + "/videos";
            
            facebookURL = _facebookGraphAPIProtocol
                + "://"
                + _facebookGraphAPIHostName
                + ":" + to_string(_facebookGraphAPIPort)
                + facebookURI;
            
            // we could apply md5 to utc time
            string boundary = to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()));
            string endOfLine = "\r\n";
            string body =
                    "--" + boundary + endOfLine                    
                    + "Content-Disposition: form-data; name=\"access_token\"" + endOfLine + endOfLine
                    + facebookAccessToken + endOfLine
                    
                    + "--" + boundary + endOfLine
                    + "Content-Disposition: form-data; name=\"upload_phase\"" + endOfLine + endOfLine
                    + "start" + endOfLine
                    
                    + "--" + boundary + endOfLine
                    + "Content-Disposition: form-data; name=\"file_size\"" + endOfLine + endOfLine
                    + to_string(sizeInBytes) + endOfLine

                    + "--" + boundary + "--" + endOfLine + endOfLine
                    ;

            list<string> header;
            string contentTypeHeader = "Content-Type: multipart/form-data; boundary=\"" + boundary + "\"";
            header.push_back(contentTypeHeader);

            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            request.setOpt(new curlpp::options::PostFields(body));
            request.setOpt(new curlpp::options::PostFieldSize(body.length()));

            request.setOpt(new curlpp::options::Url(facebookURL));
            request.setOpt(new curlpp::options::Timeout(_facebookGraphAPITimeoutInSeconds));

            if (_facebookGraphAPIProtocol == "https")
            {
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
    //                typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
    //                typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    


                // cert is stored PEM coded in file... 
                // since PEM is default, we needn't set it for PEM 
                // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                // curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                // equest.setOpt(sslCertType);

                // set the cert for client authentication
                // "testcert.pem"
                // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                // curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                // request.setOpt(sslCert);

                // sorry, for engine we must set the passphrase
                //   (if the key has one...)
                // const char *pPassphrase = NULL;
                // if(pPassphrase)
                //  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                // if we use a key stored in a crypto engine,
                //   we must set the key type to "ENG"
                // pKeyType  = "PEM";
                // curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                // set the private key (file or ID in engine)
                // pKeyName  = "testkey.pem";
                // curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                // set the file with the certs vaildating the server
                // *pCACertFile = "cacert.pem";
                // curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

                // disconnect if we can't validate server's cert
                bool bSslVerifyPeer = false;
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                request.setOpt(sslVerifyPeer);

                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                request.setOpt(sslVerifyHost);

                // request.setOpt(new curlpp::options::SslEngineDefault());                                              

            }
            request.setOpt(new curlpp::options::HttpHeader(header));

            ostringstream response;
            request.setOpt(new curlpp::options::WriteStream(&response));

            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "Calling facebook"
                    + ", facebookURL: " + facebookURL
                    + ", _facebookGraphAPIProtocol: " + _facebookGraphAPIProtocol
                    + ", _facebookGraphAPIHostName: " + _facebookGraphAPIHostName
                    + ", _facebookGraphAPIPort: " + to_string(_facebookGraphAPIPort)
                    + ", facebookURI: " + facebookURI
                    + ", contentTypeHeader: " + contentTypeHeader
                    + ", body: " + body
            );
            request.perform();

            sResponse = response.str();
            _logger->info(__FILEREF__ + "Called facebook"
                    + ", facebookURL: " + facebookURL
                    + ", contentTypeHeader: " + contentTypeHeader
                    + ", body: " + body
                    + ", sResponse: " + sResponse
            );
            
            Json::Value facebookResponseRoot;
            try
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &facebookResponseRoot, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the facebook response"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", errors: " + errors
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            catch(...)
            {
                string errorMessage = string("facebook json response is not well format")
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }
            
            string field = "upload_session_id";
            if (!_mmsEngineDBFacade->isMetadataPresent(facebookResponseRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field into the response is not present or it is null"
                        + ", Field: " + field
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            uploadSessionId = facebookResponseRoot.get(field, "XXX").asString();

            field = "video_id";
            if (!_mmsEngineDBFacade->isMetadataPresent(facebookResponseRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field into the response is not present or it is null"
                        + ", Field: " + field
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            videoId = facebookResponseRoot.get(field, "XXX").asString();
            
            field = "start_offset";
            if (!_mmsEngineDBFacade->isMetadataPresent(facebookResponseRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field into the response is not present or it is null"
                        + ", Field: " + field
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string sStartOffset = facebookResponseRoot.get(field, "XXX").asString();
            startOffset = stoll(sStartOffset);
            
            field = "end_offset";
            if (!_mmsEngineDBFacade->isMetadataPresent(facebookResponseRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field into the response is not present or it is null"
                        + ", Field: " + field
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string sEndOffset = facebookResponseRoot.get(field, "XXX").asString();
            endOffset = stoll(sEndOffset);
        }
        
        while (startOffset < endOffset)
        {
            /*
                curl \
                    -X POST "https://graph-video.facebook.com/v2.3/1533641336884006/videos"  \
                    -F "access_token=XXXXXXX" \
                    -F "upload_phase=transfer" \
                    -F “start_offset=0" \
                    -F "upload_session_id=1564747013773438" \
                    -F "video_file_chunk=@chunk1.mp4"
            */
            // transfer
            {
                string facebookURI = string("/") + _facebookGraphAPIVersion + "/" + facebookNodeId + "/videos";

                facebookURL = _facebookGraphAPIProtocol
                    + "://"
                    + _facebookGraphAPIHostName
                    + ":" + to_string(_facebookGraphAPIPort)
                    + facebookURI;

                string mediaContentType = string("video") + "/" + fileFormat;                    
                
                // we could apply md5 to utc time
                string boundary = to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()));
                string endOfLine = "\r\n";
                string bodyFirstPart =
                        "--" + boundary + endOfLine
                        + "Content-Disposition: form-data; name=\"access_token\"" + endOfLine + endOfLine
                        + facebookAccessToken + endOfLine

                        + "--" + boundary + endOfLine
                        + "Content-Disposition: form-data; name=\"upload_phase\"" + endOfLine + endOfLine
                        + "transfer" + endOfLine

                        + "--" + boundary + endOfLine
                        + "Content-Disposition: form-data; name=\"start_offset\"" + endOfLine + endOfLine
                        + to_string(startOffset) + endOfLine

                        + "--" + boundary + endOfLine
                        + "Content-Disposition: form-data; name=\"upload_session_id\"" + endOfLine + endOfLine
                        + uploadSessionId + endOfLine

                        + "--" + boundary + endOfLine
                        + "Content-Disposition: form-data; name=\"video_file_chunk\"" + endOfLine
                        + "Content-Type: " + mediaContentType
                        + "Content-Length: " + (to_string(endOffset - startOffset)) + endOfLine + endOfLine
                        ;

                string bodyLastPart =
                        endOfLine + "--" + boundary + "--" + endOfLine + endOfLine
                        ;

                list<string> header;
                string contentTypeHeader = "Content-Type: multipart/form-data; boundary=\"" + boundary + "\"";
                header.push_back(contentTypeHeader);

                curlpp::Cleanup cleaner;
                curlpp::Easy request;

                CurlUploadFacebookData curlUploadData;
                {
                    curlUploadData.mediaSourceFileStream.open(mmsAssetPathName);

                    curlUploadData.bodyFirstPartSent    = false;
                    curlUploadData.bodyFirstPart        = bodyFirstPart;
                    
                    curlUploadData.bodyLastPartSent     = false;
                    curlUploadData.bodyLastPart         = bodyLastPart;

                    curlUploadData.currentOffset        = startOffset;

                    curlUploadData.startOffset          = startOffset;
                    curlUploadData.endOffset            = endOffset;

                    curlpp::options::ReadFunctionCurlFunction curlUploadCallbackFunction(curlUploadVideoOnFacebookCallback);
                    curlpp::OptionTrait<void *, CURLOPT_READDATA> curlUploadDataData(&curlUploadData);
                    request.setOpt(curlUploadCallbackFunction);
                    request.setOpt(curlUploadDataData);
                }

                request.setOpt(new curlpp::options::Url(facebookURL));
                request.setOpt(new curlpp::options::Timeout(_facebookGraphAPITimeoutInSeconds));

                if (_facebookGraphAPIProtocol == "https")
                {
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
        //                typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
        //                typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    


                    // cert is stored PEM coded in file... 
                    // since PEM is default, we needn't set it for PEM 
                    // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                    // curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                    // equest.setOpt(sslCertType);

                    // set the cert for client authentication
                    // "testcert.pem"
                    // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                    // curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                    // request.setOpt(sslCert);

                    // sorry, for engine we must set the passphrase
                    //   (if the key has one...)
                    // const char *pPassphrase = NULL;
                    // if(pPassphrase)
                    //  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                    // if we use a key stored in a crypto engine,
                    //   we must set the key type to "ENG"
                    // pKeyType  = "PEM";
                    // curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                    // set the private key (file or ID in engine)
                    // pKeyName  = "testkey.pem";
                    // curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                    // set the file with the certs vaildating the server
                    // *pCACertFile = "cacert.pem";
                    // curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

                    // disconnect if we can't validate server's cert
                    bool bSslVerifyPeer = false;
                    curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                    request.setOpt(sslVerifyPeer);

                    curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                    request.setOpt(sslVerifyHost);

                    // request.setOpt(new curlpp::options::SslEngineDefault());                                              

                }
                request.setOpt(new curlpp::options::HttpHeader(header));

                ostringstream response;
                request.setOpt(new curlpp::options::WriteStream(&response));

                chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

                _logger->info(__FILEREF__ + "Calling facebook"
                        + ", facebookURL: " + facebookURL
                        + ", _facebookGraphAPIProtocol: " + _facebookGraphAPIProtocol
                        + ", _facebookGraphAPIHostName: " + _facebookGraphAPIHostName
                        + ", _facebookGraphAPIPort: " + to_string(_facebookGraphAPIPort)
                        + ", facebookURI: " + facebookURI
                        + ", bodyFirstPart: " + bodyFirstPart
                );
                request.perform();

                sResponse = response.str();
                _logger->info(__FILEREF__ + "Called facebook"
                        + ", facebookURL: " + facebookURL
                        + ", bodyFirstPart: " + bodyFirstPart
                        + ", sResponse: " + sResponse
                );

                Json::Value facebookResponseRoot;
                try
                {
                    Json::CharReaderBuilder builder;
                    Json::CharReader* reader = builder.newCharReader();
                    string errors;

                    bool parsingSuccessful = reader->parse(sResponse.c_str(),
                            sResponse.c_str() + sResponse.size(), 
                            &facebookResponseRoot, &errors);
                    delete reader;

                    if (!parsingSuccessful)
                    {
                        string errorMessage = __FILEREF__ + "failed to parse the facebook response"
                            + ", _processorIdentifier: " + to_string(_processorIdentifier)
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", errors: " + errors
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                catch(...)
                {
                    string errorMessage = string("facebook json response is not well format")
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(__FILEREF__ + errorMessage);

                    throw runtime_error(errorMessage);
                }

                string field = "start_offset";
                if (!_mmsEngineDBFacade->isMetadataPresent(facebookResponseRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", Field: " + field
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                string sStartOffset = facebookResponseRoot.get(field, "XXX").asString();
                startOffset = stoll(sStartOffset);

                field = "end_offset";
                if (!_mmsEngineDBFacade->isMetadataPresent(facebookResponseRoot, field))
                {
                    string errorMessage = __FILEREF__ + "Field is not present or it is null"
                            + ", Field: " + field
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                string sEndOffset = facebookResponseRoot.get(field, "XXX").asString();
                endOffset = stoll(sEndOffset);
            }
        }
        
        /*
            curl \
                -X POST "https://graph-video.facebook.com/v2.3/1533641336884006/videos"  \
                -F "access_token=XXXXXXXX" \
                -F "upload_phase=finish" \
                -F "upload_session_id=1564747013773438" 

            {"success":true}
        */
        // finish: pubblica il video e mettilo in coda per la codifica asincrona
        bool success;
        {
            string facebookURI = string("/") + _facebookGraphAPIVersion + "/" + facebookNodeId + "/videos";
            
            facebookURL = _facebookGraphAPIProtocol
                + "://"
                + _facebookGraphAPIHostName
                + ":" + to_string(_facebookGraphAPIPort)
                + facebookURI;
            
            // we could apply md5 to utc time
            string boundary = to_string(chrono::system_clock::to_time_t(chrono::system_clock::now()));
            string endOfLine = "\r\n";
            string body =
                    "--" + boundary + endOfLine                    
                    + "Content-Disposition: form-data; name=\"access_token\"" + endOfLine + endOfLine
                    + facebookAccessToken + endOfLine
                    
                    + "--" + boundary + endOfLine                    
                    + "Content-Disposition: form-data; name=\"upload_phase\"" + endOfLine + endOfLine
                    + "finish" + endOfLine
                    
                    + "--" + boundary + endOfLine                    
                    + "Content-Disposition: form-data; name=\"upload_session_id\"" + endOfLine + endOfLine
                    + uploadSessionId + endOfLine

                    + "--" + boundary + "--" + endOfLine + endOfLine
                    ;

            list<string> header;
            string contentTypeHeader = "Content-Type: multipart/form-data; boundary=\"" + boundary + "\"";
            header.push_back(contentTypeHeader);

            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            request.setOpt(new curlpp::options::PostFields(body));
            request.setOpt(new curlpp::options::PostFieldSize(body.length()));

            request.setOpt(new curlpp::options::Url(facebookURL));
            request.setOpt(new curlpp::options::Timeout(_facebookGraphAPITimeoutInSeconds));

            if (_facebookGraphAPIProtocol == "https")
            {
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
    //                typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
    //                typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    


                // cert is stored PEM coded in file... 
                // since PEM is default, we needn't set it for PEM 
                // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                // curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                // equest.setOpt(sslCertType);

                // set the cert for client authentication
                // "testcert.pem"
                // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                // curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                // request.setOpt(sslCert);

                // sorry, for engine we must set the passphrase
                //   (if the key has one...)
                // const char *pPassphrase = NULL;
                // if(pPassphrase)
                //  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                // if we use a key stored in a crypto engine,
                //   we must set the key type to "ENG"
                // pKeyType  = "PEM";
                // curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                // set the private key (file or ID in engine)
                // pKeyName  = "testkey.pem";
                // curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                // set the file with the certs vaildating the server
                // *pCACertFile = "cacert.pem";
                // curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

                // disconnect if we can't validate server's cert
                bool bSslVerifyPeer = false;
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                request.setOpt(sslVerifyPeer);

                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                request.setOpt(sslVerifyHost);

                // request.setOpt(new curlpp::options::SslEngineDefault());                                              

            }
            request.setOpt(new curlpp::options::HttpHeader(header));

            ostringstream response;
            request.setOpt(new curlpp::options::WriteStream(&response));

            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "Calling facebook"
                    + ", facebookURL: " + facebookURL
                    + ", _facebookGraphAPIProtocol: " + _facebookGraphAPIProtocol
                    + ", _facebookGraphAPIHostName: " + _facebookGraphAPIHostName
                    + ", _facebookGraphAPIPort: " + to_string(_facebookGraphAPIPort)
                    + ", facebookURI: " + facebookURI
                    + ", body: " + body
            );
            request.perform();

            sResponse = response.str();
            _logger->info(__FILEREF__ + "Called facebook"
                    + ", facebookURL: " + facebookURL
                    + ", body: " + body
                    + ", sResponse: " + sResponse
            );
            
            Json::Value facebookResponseRoot;
            try
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &facebookResponseRoot, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the facebook response"
                        + ", _processorIdentifier: " + to_string(_processorIdentifier)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", errors: " + errors
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            catch(...)
            {
                string errorMessage = string("facebook json response is not well format")
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }
            
            string field = "success";
            if (!_mmsEngineDBFacade->isMetadataPresent(facebookResponseRoot, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            success = facebookResponseRoot.get(field, "XXX").asBool();

            if (!success)
            {
                string errorMessage = __FILEREF__ + "Post Video on Facebook failed"
                        + ", Field: " + field
                        + ", success: " + to_string(success)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }        
        
        {
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
    }
    catch (curlpp::LogicError & e) 
    {
        _logger->error(__FILEREF__ + "Post video on Facebook failed (LogicError)"
            + ", facebookURL: " + facebookURL
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
    catch (curlpp::RuntimeError & e) 
    {
        _logger->error(__FILEREF__ + "Post video on Facebook failed (RuntimeError)"
            + ", facebookURL: " + facebookURL
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
    catch (runtime_error e)
    {
        _logger->error(__FILEREF__ + "Post Video on Facebook failed (runtime_error)"
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "Post Video on Facebook failed (exception)"
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
}

size_t curlUploadVideoOnYouTubeCallback(char* ptr, size_t size, size_t nmemb, void *f)
{
    MMSEngineProcessor::CurlUploadYouTubeData* curlUploadData = (MMSEngineProcessor::CurlUploadYouTubeData*) f;
    
    auto logger = spdlog::get("mmsEngineService");


    if (!curlUploadData->bodyFirstPartSent)
    {
        if (curlUploadData->bodyFirstPart.size() > size * nmemb)
        {
            logger->error(__FILEREF__ + "Not enougth memory!!!"
                + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
                + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
                + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
                + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
                + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
                + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
                + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
                + ", curlUploadData->bodyFirstPart.size(): " + to_string(curlUploadData->bodyFirstPart.size())
                + ", size * nmemb: " + to_string(size * nmemb)
            );

            return CURL_READFUNC_ABORT;
        }
        
        strcpy(ptr, curlUploadData->bodyFirstPart.c_str());
        
        curlUploadData->bodyFirstPartSent = true;

        logger->info(__FILEREF__ + "First read"
             + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
             + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
             + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
             + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
             + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
             + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
             + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
        );
        
        return curlUploadData->bodyFirstPart.size();
    }
    else if (curlUploadData->currentOffset == curlUploadData->endOffset)
    {
        if (!curlUploadData->bodyLastPartSent)
        {
            if (curlUploadData->bodyLastPart.size() > size * nmemb)
            {
                logger->error(__FILEREF__ + "Not enougth memory!!!"
                    + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
                    + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
                    + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
                    + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
                    + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
                    + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
                    + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
                    + ", curlUploadData->bodyLastPart.size(): " + to_string(curlUploadData->bodyLastPart.size())
                    + ", size * nmemb: " + to_string(size * nmemb)
                );

                return CURL_READFUNC_ABORT;
            }

            strcpy(ptr, curlUploadData->bodyLastPart.c_str());

            curlUploadData->bodyLastPartSent = true;

            logger->info(__FILEREF__ + "Last read"
                + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
                + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
                + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
                + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
                + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
                + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
                + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
            );

            return curlUploadData->bodyLastPart.size();
        }
        else
        {
            logger->error(__FILEREF__ + "This scenario should never happen because Content-Length was set"
                + ", curlUploadData->bodyFirstPartSent: " + to_string(curlUploadData->bodyFirstPartSent)
                + ", curlUploadData->bodyFirstPart: " + curlUploadData->bodyFirstPart
                + ", curlUploadData->bodyLastPartSent: " + to_string(curlUploadData->bodyLastPartSent)
                + ", curlUploadData->bodyLastPart: " + curlUploadData->bodyLastPart
                + ", curlUploadData->startOffset: " + to_string(curlUploadData->startOffset)
                + ", curlUploadData->endOffset: " + to_string(curlUploadData->endOffset)
                + ", curlUploadData->currentOffset: " + to_string(curlUploadData->currentOffset)
            );

            return CURL_READFUNC_ABORT;
        }
    }

    if(curlUploadData->currentOffset + (size * nmemb) <= curlUploadData->endOffset)
        curlUploadData->mediaSourceFileStream.read(ptr, size * nmemb);
    else
        curlUploadData->mediaSourceFileStream.read(ptr, curlUploadData->endOffset - curlUploadData->currentOffset);

    int64_t charsRead = curlUploadData->mediaSourceFileStream.gcount();
    
    curlUploadData->currentOffset += charsRead;

    return charsRead;        
};

void MMSEngineProcessor::postVideoOnYouTubeThread(
        shared_ptr<long> processorsThreadsNumber,
        string mmsAssetPathName, int64_t sizeInBytes,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        string youTubeAuthorizationToken, string youTubeTitle,
        string youTubeDescription, Json::Value youTubeTags,
        int youTubeCategoryId)
{

    string youTubeURL;
    string sResponse;
    
    try
    {
        _logger->info(__FILEREF__ + "postVideoOnYouTubeThread"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", mmsAssetPathName: " + mmsAssetPathName
            + ", sizeInBytes: " + to_string(sizeInBytes)
            + ", youTubeAuthorizationToken: " + youTubeAuthorizationToken
            + ", youTubeTitle: " + youTubeTitle
            + ", youTubeDescription: " + youTubeDescription
            + ", youTubeCategoryId: " + to_string(youTubeCategoryId)
        );
        
        string fileFormat;
        {
            size_t extensionIndex = mmsAssetPathName.find_last_of(".");
            if (extensionIndex == string::npos)
            {
                string errorMessage = __FILEREF__ + "No fileFormat (extension of the file) found in mmsAssetPathName"
                    + ", _processorIdentifier: " + to_string(_processorIdentifier)
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", mmsAssetPathName: " + mmsAssetPathName
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            fileFormat = mmsAssetPathName.substr(extensionIndex + 1);
        }
        
        /*
            POST /upload/youtube/v3/videos?uploadType=resumable&part=snippet,status,contentDetails HTTP/1.1
            Host: www.googleapis.com
            Authorization: Bearer AUTH_TOKEN
            Content-Length: 278
            Content-Type: application/json; charset=UTF-8
            X-Upload-Content-Length: 3000000
            X-Upload-Content-Type: video/*

            {
              "snippet": {
                "title": "My video title",
                "description": "This is a description of my video",
                "tags": ["cool", "video", "more keywords"],
                "categoryId": 22
              },
              "status": {
                "privacyStatus": "public",
                "embeddable": True,
                "license": "youtube"
              }
            }

            HTTP/1.1 200 OK
            Location: https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&upload_id=xa298sd_f&part=snippet,status,contentDetails
            Content-Length: 0
        */
        string videoContentType = "video/*";
        string youTubeUploadURL;
        {
            string youTubeURI = string("/upload/youtube/") + _youTubeDataAPIVersion + "/videos?uploadType=resumable&part=snippet,status,contentDetails";
            
            youTubeURL = _youTubeDataAPIProtocol
                + "://"
                + _youTubeDataAPIHostName
                + ":" + to_string(_youTubeDataAPIPort)
                + youTubeURI;
    
            string body;
            {
                Json::Value bodyRoot;
                Json::Value snippetRoot;

                string field = "title";
                snippetRoot[field] = youTubeTitle;

                if (youTubeDescription != "")
                {
                    field = "description";
                    snippetRoot[field] = youTubeDescription;
                }

                if (youTubeTags != Json::nullValue)
                {
                    field = "tags";
                    snippetRoot[field] = youTubeTags;
                }

                if (youTubeCategoryId != -1)
                {
                    field = "categoryId";
                    snippetRoot[field] = youTubeCategoryId;
                }
                
                field = "snippet";
                bodyRoot[field] = snippetRoot;
                

                Json::Value statusRoot;

                field = "privacyStatus";
                statusRoot[field] = "public";

                field = "embeddable";
                statusRoot[field] = true;

                field = "license";
                statusRoot[field] = "youtube";

                field = "status";
                bodyRoot[field] = statusRoot;

                {
                    Json::StreamWriterBuilder wbuilder;
                    
                    body = Json::writeString(wbuilder, bodyRoot);
                }
            }

            list<string> headerList;

            {
                string header = "Authorization: Bearer " + youTubeAuthorizationToken;
                headerList.push_back(header);

                header = "Content-Length: " + to_string(body.length());
                headerList.push_back(header);
                
                header = "Content-Type: application/json; charset=UTF-8";
                headerList.push_back(header);

                header = "X-Upload-Content-Length: " + to_string(sizeInBytes);
                headerList.push_back(header);
                
                header = string("X-Upload-Content-Type: ") + videoContentType;
                headerList.push_back(header);
            }                    

            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            request.setOpt(new curlpp::options::PostFields(body));
            request.setOpt(new curlpp::options::PostFieldSize(body.length()));

            request.setOpt(new curlpp::options::Url(youTubeURL));
            request.setOpt(new curlpp::options::Timeout(_youTubeDataAPITimeoutInSeconds));

            if (_youTubeDataAPIProtocol == "https")
            {
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
    //                typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
    //                typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
    //                typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    


                // cert is stored PEM coded in file... 
                // since PEM is default, we needn't set it for PEM 
                // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                // curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                // equest.setOpt(sslCertType);

                // set the cert for client authentication
                // "testcert.pem"
                // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                // curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                // request.setOpt(sslCert);

                // sorry, for engine we must set the passphrase
                //   (if the key has one...)
                // const char *pPassphrase = NULL;
                // if(pPassphrase)
                //  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                // if we use a key stored in a crypto engine,
                //   we must set the key type to "ENG"
                // pKeyType  = "PEM";
                // curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                // set the private key (file or ID in engine)
                // pKeyName  = "testkey.pem";
                // curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                // set the file with the certs vaildating the server
                // *pCACertFile = "cacert.pem";
                // curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

                // disconnect if we can't validate server's cert
                bool bSslVerifyPeer = false;
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                request.setOpt(sslVerifyPeer);

                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                request.setOpt(sslVerifyHost);

                // request.setOpt(new curlpp::options::SslEngineDefault());                                              

            }
            
            for (string headerMessage: headerList)
                _logger->info(__FILEREF__ + "Added header message" + headerMessage);
            request.setOpt(new curlpp::options::HttpHeader(headerList));

            ostringstream response;
            request.setOpt(new curlpp::options::WriteStream(&response));

            // store response headers in the response
            // You simply have to set next option to prefix the header to the normal body output. 
            request.setOpt(new curlpp::options::Header(true)); 
            
            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "Calling youTube (first call)"
                    + ", youTubeURL: " + youTubeURL
                    + ", _youTubeDataAPIProtocol: " + _youTubeDataAPIProtocol
                    + ", _youTubeDataAPIHostName: " + _youTubeDataAPIHostName
                    + ", _youTubeDataAPIPort: " + to_string(_youTubeDataAPIPort)
                    + ", youTubeURI: " + youTubeURI
                    + ", body: " + body
            );
            request.perform();

            long responseCode = curlpp::infos::ResponseCode::get(request);

            sResponse = response.str();
            _logger->info(__FILEREF__ + "Called youTube (first call)"
                    + ", youTubeURL: " + youTubeURL
                    + ", body: " + body
                    + ", responseCode: " + to_string(responseCode)
                    + ", sResponse: " + sResponse
            );
            
            if (responseCode != 200)
            {
                string errorMessage = __FILEREF__ + "youTube (first call) failed"
                        + ", youTubeURL: " + youTubeURL
                        + ", body: " + body
                        + ", responseCode: " + to_string(responseCode)
                        + ", sResponse: " + sResponse
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            // youTubeUploadURL = 
        }

        bool contentCompletelyUploaded = false;
        int64_t lastByteSent = -1;
        while (!contentCompletelyUploaded)
        {
            /*
                // In case of the first request
                PUT UPLOAD_URL HTTP/1.1
                Authorization: Bearer AUTH_TOKEN
                Content-Length: CONTENT_LENGTH
                Content-Type: CONTENT_TYPE

                BINARY_FILE_DATA

                // in case of resuming
                PUT UPLOAD_URL HTTP/1.1
                Authorization: Bearer AUTH_TOKEN
                Content-Length: REMAINING_CONTENT_LENGTH
                Content-Range: bytes FIRST_BYTE-LAST_BYTE/TOTAL_CONTENT_LENGTH

                PARTIAL_BINARY_FILE_DATA            
            */

            {                
                list<string> headerList;
                headerList.push_back(string("Authorization: Bearer ") + youTubeAuthorizationToken);
                if (lastByteSent == -1)
                    headerList.push_back(string("Content-Length: ") + to_string(sizeInBytes));
                else
                    headerList.push_back(string("Content-Length: ") + to_string(sizeInBytes - lastByteSent + 1));
                if (lastByteSent == -1)
                    headerList.push_back(string("Content-Type: ") + videoContentType);
                else
                    headerList.push_back(string("Content-Range: bytes ") + to_string(lastByteSent) + "-" + to_string(sizeInBytes - 1) + "/" + to_string(sizeInBytes));

                curlpp::Cleanup cleaner;
                curlpp::Easy request;

                CurlUploadYouTubeData curlUploadData;
                {
                    curlUploadData.mediaSourceFileStream.open(mmsAssetPathName);

                    /*
                    curlUploadData.currentOffset        = startOffset;

                    curlUploadData.startOffset          = startOffset;
                    curlUploadData.endOffset            = endOffset;
                    */

                    curlpp::options::ReadFunctionCurlFunction curlUploadCallbackFunction(curlUploadVideoOnYouTubeCallback);
                    curlpp::OptionTrait<void *, CURLOPT_READDATA> curlUploadDataData(&curlUploadData);
                    request.setOpt(curlUploadCallbackFunction);
                    request.setOpt(curlUploadDataData);
                }

                request.setOpt(new curlpp::options::CustomRequest{"PUT"});
                request.setOpt(new curlpp::options::Url(youTubeURL));
                request.setOpt(new curlpp::options::Timeout(_youTubeDataAPITimeoutInSeconds));

                if (_youTubeDataAPIProtocol == "https")
                {
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
        //                typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
        //                typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
        //                typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    


                    // cert is stored PEM coded in file... 
                    // since PEM is default, we needn't set it for PEM 
                    // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                    // curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                    // equest.setOpt(sslCertType);

                    // set the cert for client authentication
                    // "testcert.pem"
                    // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                    // curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                    // request.setOpt(sslCert);

                    // sorry, for engine we must set the passphrase
                    //   (if the key has one...)
                    // const char *pPassphrase = NULL;
                    // if(pPassphrase)
                    //  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                    // if we use a key stored in a crypto engine,
                    //   we must set the key type to "ENG"
                    // pKeyType  = "PEM";
                    // curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                    // set the private key (file or ID in engine)
                    // pKeyName  = "testkey.pem";
                    // curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                    // set the file with the certs vaildating the server
                    // *pCACertFile = "cacert.pem";
                    // curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

                    // disconnect if we can't validate server's cert
                    bool bSslVerifyPeer = false;
                    curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                    request.setOpt(sslVerifyPeer);

                    curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                    request.setOpt(sslVerifyHost);

                    // request.setOpt(new curlpp::options::SslEngineDefault());                                              

                }

                for (string headerMessage: headerList)
                    _logger->info(__FILEREF__ + "Added header message" + headerMessage);
                request.setOpt(new curlpp::options::HttpHeader(headerList));

                chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

                _logger->info(__FILEREF__ + "Calling youTube (upload)"
                        + ", youTubeURL: " + youTubeURL
                        + ", _youTubeDataAPIProtocol: " + _youTubeDataAPIProtocol
                        + ", _youTubeDataAPIHostName: " + _youTubeDataAPIHostName
                        + ", _youTubeDataAPIPort: " + to_string(_youTubeDataAPIPort)
                );
                request.perform();

                long responseCode = curlpp::infos::ResponseCode::get(request);
                
                _logger->info(__FILEREF__ + "Called youTube (upload)"
                        + ", youTubeURL: " + youTubeURL
                        + ", responseCode: " + to_string(responseCode)
                );
                
                if (responseCode == 201)
                {
                    _logger->info(__FILEREF__ + "youTube upload successful"
                            + ", youTubeURL: " + youTubeURL
                            + ", responseCode: " + to_string(responseCode)
                    );

                    contentCompletelyUploaded = true;
                }
                else if (responseCode == 500 
                        || responseCode == 502
                        || responseCode == 503
                        || responseCode == 504
                        )
                {                    
                    _logger->warn(__FILEREF__ + "youTube upload failed, trying to resume"
                            + ", youTubeURL: " + youTubeURL
                            + ", responseCode: " + to_string(responseCode)
                    );
                    
                    /*
                        PUT UPLOAD_URL HTTP/1.1
                        Authorization: Bearer AUTH_TOKEN
                        Content-Length: 0
                        Content-Range: bytes *\/CONTENT_LENGTH

                        308 Resume Incomplete
                        Content-Length: 0
                        Range: bytes=0-999999
                    */
                    {                
                        list<string> headerList;
                        headerList.push_back(string("Authorization: Bearer ") + youTubeAuthorizationToken);
                        headerList.push_back(string("Content-Length: 0"));
                        headerList.push_back(string("Content-Range: bytes */") + to_string(sizeInBytes));

                        curlpp::Cleanup cleaner;
                        curlpp::Easy request;

                        request.setOpt(new curlpp::options::CustomRequest{"PUT"});
                        request.setOpt(new curlpp::options::Url(youTubeURL));
                        request.setOpt(new curlpp::options::Timeout(_youTubeDataAPITimeoutInSeconds));

                        if (_youTubeDataAPIProtocol == "https")
                        {
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
                //                typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
                //                typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
                //                typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    


                            // cert is stored PEM coded in file... 
                            // since PEM is default, we needn't set it for PEM 
                            // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                            // curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                            // equest.setOpt(sslCertType);

                            // set the cert for client authentication
                            // "testcert.pem"
                            // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                            // curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                            // request.setOpt(sslCert);

                            // sorry, for engine we must set the passphrase
                            //   (if the key has one...)
                            // const char *pPassphrase = NULL;
                            // if(pPassphrase)
                            //  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                            // if we use a key stored in a crypto engine,
                            //   we must set the key type to "ENG"
                            // pKeyType  = "PEM";
                            // curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                            // set the private key (file or ID in engine)
                            // pKeyName  = "testkey.pem";
                            // curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                            // set the file with the certs vaildating the server
                            // *pCACertFile = "cacert.pem";
                            // curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

                            // disconnect if we can't validate server's cert
                            bool bSslVerifyPeer = false;
                            curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                            request.setOpt(sslVerifyPeer);

                            curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                            request.setOpt(sslVerifyHost);

                            // request.setOpt(new curlpp::options::SslEngineDefault());                                              

                        }
                        
                        for (string headerMessage: headerList)
                            _logger->info(__FILEREF__ + "Added header message" + headerMessage);
                        request.setOpt(new curlpp::options::HttpHeader(headerList));

                        ostringstream response;
                        request.setOpt(new curlpp::options::WriteStream(&response));

                        // store response headers in the response
                        // You simply have to set next option to prefix the header to the normal body output. 
                        request.setOpt(new curlpp::options::Header(true));
            
                        chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

                        _logger->info(__FILEREF__ + "Calling youTube check status"
                                + ", youTubeURL: " + youTubeURL
                                + ", _youTubeDataAPIProtocol: " + _youTubeDataAPIProtocol
                                + ", _youTubeDataAPIHostName: " + _youTubeDataAPIHostName
                                + ", _youTubeDataAPIPort: " + to_string(_youTubeDataAPIPort)
                        );
                        request.perform();

                        sResponse = response.str();
                        long responseCode = curlpp::infos::ResponseCode::get(request);

                        _logger->info(__FILEREF__ + "Called youTube check status"
                                + ", youTubeURL: " + youTubeURL
                                + ", responseCode: " + to_string(responseCode)
                                + ", sResponse: " + sResponse
                        );

                        if (responseCode == 308)
                        {
                            _logger->info(__FILEREF__ + "youTube check status successful"
                                + ", youTubeURL: " + youTubeURL
                                + ", responseCode: " + to_string(responseCode)
                                + ", sResponse: " + sResponse
                            );

                            // lastByteSent = ;
                        }
                        else
                        {   
                            // error
                            string errorMessage (__FILEREF__ + "youTube check status failed"
                                    + ", youTubeURL: " + youTubeURL
                                    + ", responseCode: " + to_string(responseCode)
                            );
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                }
                else
                {   
                    // error
                    string errorMessage (__FILEREF__ + "youTube upload failed"
                            + ", youTubeURL: " + youTubeURL
                            + ", responseCode: " + to_string(responseCode)
                    );
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
        }
                
        {
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
    }
    catch (curlpp::LogicError & e) 
    {
        _logger->error(__FILEREF__ + "Post video on Facebook failed (LogicError)"
            + ", youTubeURL: " + youTubeURL
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
    catch (curlpp::RuntimeError & e) 
    {
        _logger->error(__FILEREF__ + "Post video on Facebook failed (RuntimeError)"
            + ", youTubeURL: " + youTubeURL
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
    catch (runtime_error e)
    {
        _logger->error(__FILEREF__ + "Post Video on Facebook failed (runtime_error)"
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "Post Video on Facebook failed (exception)"
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
}

void MMSEngineProcessor::userHttpCallbackThread(
        shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey, string httpProtocol, string httpHostName,
        int httpPort, string httpURI, string httpURLParameters,
        string httpMethod, long callbackTimeoutInSeconds,
        Json::Value userHeadersRoot, 
        Json::Value callbackMedatada
        )
{
    string userURL;
    string sResponse;

    try
    {
        _logger->info(__FILEREF__ + "userHttpCallbackThread"
            + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", httpProtocol: " + httpProtocol
            + ", httpHostName: " + httpHostName
            + ", httpPort: " + to_string(httpPort)
            + ", httpURI: " + httpURI
        );

        userURL = httpProtocol
                + "://"
                + httpHostName
                + ":"
                + to_string(httpPort)
                + httpURI
                + httpURLParameters;

        string data;
        if (callbackMedatada.type() != Json::nullValue)
        {
            Json::StreamWriterBuilder wbuilder;

            data = Json::writeString(wbuilder, callbackMedatada);
        }

        list<string> header;

        if (httpMethod == "POST" && data != "")
            header.push_back("Content-Type: application/json");

        for (int userHeaderIndex = 0; userHeaderIndex < userHeadersRoot.size(); ++userHeaderIndex)
        {
            string userHeader = userHeadersRoot[userHeaderIndex].asString();

            header.push_back(userHeader);
        }

        curlpp::Cleanup cleaner;
        curlpp::Easy request;

        if (data != "")
        {
            if (httpMethod == "GET")
            {
                if (httpURLParameters == "")
                    userURL += "?";
                else
                    userURL += "&";
                userURL += ("data=" + curlpp::escape(data));
            }
            else    // POST
            {
                request.setOpt(new curlpp::options::PostFields(data));
                request.setOpt(new curlpp::options::PostFieldSize(data.length()));
            }
        }

        request.setOpt(new curlpp::options::Url(userURL));
        request.setOpt(new curlpp::options::Timeout(callbackTimeoutInSeconds));

        if (httpProtocol == "https")
        {
//                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
//                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
//                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
//                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
//                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
//                typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
//                typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
//                typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
//                typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
//                typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
//                typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
//                typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
//                typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    


            // cert is stored PEM coded in file... 
            // since PEM is default, we needn't set it for PEM 
            // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
            // curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
            // equest.setOpt(sslCertType);

            // set the cert for client authentication
            // "testcert.pem"
            // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
            // curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
            // request.setOpt(sslCert);

            // sorry, for engine we must set the passphrase
            //   (if the key has one...)
            // const char *pPassphrase = NULL;
            // if(pPassphrase)
            //  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

            // if we use a key stored in a crypto engine,
            //   we must set the key type to "ENG"
            // pKeyType  = "PEM";
            // curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

            // set the private key (file or ID in engine)
            // pKeyName  = "testkey.pem";
            // curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

            // set the file with the certs vaildating the server
            // *pCACertFile = "cacert.pem";
            // curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);

            // disconnect if we can't validate server's cert
            bool bSslVerifyPeer = false;
            curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
            request.setOpt(sslVerifyPeer);

            curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
            request.setOpt(sslVerifyHost);

            // request.setOpt(new curlpp::options::SslEngineDefault());                                              

        }
        request.setOpt(new curlpp::options::HttpHeader(header));

        ostringstream response;
        request.setOpt(new curlpp::options::WriteStream(&response));

        chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "Calling user callback"
                + ", userURL: " + userURL
                + ", httpProtocol: " + httpProtocol
                + ", httpHostName: " + httpHostName
                + ", httpPort: " + to_string(httpPort)
                + ", httpURI: " + httpURI
                + ", httpURLParameters: " + httpURLParameters
                + ", httpProtocol: " + httpProtocol
                + ", data: " + data
        );
        request.perform();

        sResponse = response.str();
        _logger->info(__FILEREF__ + "Called user callback"
                + ", userURL: " + userURL
                + ", data: " + data
                + ", sResponse: " + sResponse
        );        

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
    catch (curlpp::LogicError & e) 
    {
        _logger->error(__FILEREF__ + "User Callback URL failed (LogicError)"
            + ", userURL: " + userURL
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
    catch (curlpp::RuntimeError & e) 
    {
        _logger->error(__FILEREF__ + "User Callback URL failed (RuntimeError)"
            + ", userURL: " + userURL
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
    catch (runtime_error e)
    {
        _logger->error(__FILEREF__ + "User Callback URL failed (runtime_error)"
            + ", userURL: " + userURL
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "User Callback URL failed (exception)"
            + ", userURL: " + userURL
            + ", exception: " + e.what()
            + ", sResponse: " + sResponse
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
                "");    // processorMMS

        return;
    }
}


void MMSEngineProcessor::moveMediaSourceFileThread(
        shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL,
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
            + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
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

void MMSEngineProcessor::copyMediaSourceFileThread(
        shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL,
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
            + ", processors threads number: " + to_string(processorsThreadsNumber.use_count())
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

int MMSEngineProcessor::progressDownloadCallback(
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

int MMSEngineProcessor::progressUploadCallback(
        int64_t ingestionJobKey,
        chrono::system_clock::time_point& lastTimeProgressUpdate, 
        double& lastPercentageUpdated, bool& uploadingStoppedByUser,
        double dltotal, double dlnow,
        double ultotal, double ulnow)
{

    chrono::system_clock::time_point now = chrono::system_clock::now();
            
    if (ultotal != 0 &&
            (ultotal == ulnow 
            || now - lastTimeProgressUpdate >= chrono::seconds(_progressUpdatePeriodInSeconds)))
    {
        double progress = (ulnow / ultotal) * 100;
        // int uploadingPercentage = floorf(progress * 100) / 100;
        // this is to have one decimal in the percentage
        double uploadingPercentage = ((double) ((int) (progress * 10))) / 10;

        _logger->info(__FILEREF__ + "Upload still running"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", uploadingPercentage: " + to_string(uploadingPercentage)
            + ", dltotal: " + to_string(dltotal)
            + ", dlnow: " + to_string(dlnow)
            + ", ultotal: " + to_string(ultotal)
            + ", ulnow: " + to_string(ulnow)
        );
        
        lastTimeProgressUpdate = now;

        if (lastPercentageUpdated != uploadingPercentage)
        {
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", _processorIdentifier: " + to_string(_processorIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", uploadingPercentage: " + to_string(uploadingPercentage)
            );                            
            uploadingStoppedByUser = _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress (
                ingestionJobKey, uploadingPercentage);

            lastPercentageUpdated = uploadingPercentage;
        }

        if (uploadingStoppedByUser)
            return 1;   // stop downloading
    }
        
    return 0;
}