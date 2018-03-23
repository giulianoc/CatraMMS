
#include <fstream>
#include <sstream>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "catralibraries/System.h"
#include "FFMpeg.h"
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
    _maxIngestionJobsWithDependencyToCheckPerEvent = configuration["mms"].get("maxIngestionJobsWithDependencyToCheckPerEvent", 5).asInt();
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
        // cout << "Calling getAndRemoveFirstEvent" << endl;
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
                _logger->debug(__FILEREF__ + "Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION");

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

            }
            break;
            case MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT:	// 2
            {
                _logger->info(__FILEREF__ + "Received LOCALASSETINGESTIONEVENT");

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
            }
            break;
            case MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODINGEVENT:	// 3
            {
                _logger->debug(__FILEREF__ + "Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING");

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
        vector<tuple<int64_t,shared_ptr<Customer>,string,MMSEngineDBFacade::IngestionStatus,string>> 
                ingestionsToBeManaged;
        
        _mmsEngineDBFacade->getIngestionsToBeManaged(ingestionsToBeManaged, 
                _processorMMS, _maxIngestionJobsPerEvent, 
                _maxIngestionJobsWithDependencyToCheckPerEvent);
        
        for (tuple<int64_t,shared_ptr<Customer>,string,MMSEngineDBFacade::IngestionStatus,string> 
                ingestionToBeManaged: ingestionsToBeManaged)
        {
            int64_t ingestionJobKey;
            try
            {
                shared_ptr<Customer> customer;
                string metaDataContent;
                string sourceReference;
                MMSEngineDBFacade::IngestionStatus ingestionStatus;
                string mediaItemKeysDependency;

                tie(ingestionJobKey, customer, metaDataContent, ingestionStatus, 
                        mediaItemKeysDependency) = ingestionToBeManaged;
                
                _logger->info(__FILEREF__ + "json to be processed"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                );

                if (ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress
                        || ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress
                        || ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress
                        || ingestionStatus == MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
                {
                    // source binary download or uploaded terminated

                    {
                        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                                ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

                        localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
                        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
                        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

                        localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
                        localAssetIngestionEvent->setCustomer(customer);

                        localAssetIngestionEvent->setMetadataContent(metaDataContent);

                        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
                        _multiEventsSet->addEvent(event);

                        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", getEventKey().first: " + to_string(event->getEventKey().first)
                            + ", getEventKey().second: " + to_string(event->getEventKey().second));
                    }
                }
                else    // Start_Ingestion
                {
                    Json::Value metadataRoot;
                    try
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(metaDataContent.c_str(),
                                metaDataContent.c_str() + metaDataContent.size(), 
                                &metadataRoot, &errors);
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

                    tuple<MMSEngineDBFacade::IngestionType,MMSEngineDBFacade::ContentType,vector<int64_t>>
                            ingestionTypeContentTypeAndDependencies;
                    MMSEngineDBFacade::IngestionType ingestionType;
                    MMSEngineDBFacade::ContentType contentType;
                    vector<int64_t> dependencies;
                    try
                    {
                        ingestionTypeContentTypeAndDependencies = validateMetadata(metadataRoot);
                        
                        tie(ingestionType, contentType, dependencies) =
                                ingestionTypeContentTypeAndDependencies;
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

                    if (ingestionType ==
                            MMSEngineDBFacade::IngestionType::ContentIngestion)
                    {
                        MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
                        string mediaSourceURL;
                        string mediaSourceFileName;
                        string md5FileCheckSum;
                        int fileSizeInBytes;
                        try
                        {
                            tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> mediaSourceDetails;

                            mediaSourceDetails = getMediaSourceDetails(customer,
                                    ingestionType, metadataRoot);

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
                                + ", IngestionType: " + MMSEngineDBFacade::toString(ingestionType)
                                + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
                                + ", errorMessage: " + errorMessage
                                + ", processorMMS: " + ""
                            );                            
                            _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                    ingestionType,
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
                                + ", IngestionType: " + MMSEngineDBFacade::toString(ingestionType)
                                + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
                                + ", errorMessage: " + errorMessage
                                + ", processorMMS: " + ""
                            );                            
                            _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                    ingestionType,
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
                                    + ", IngestionType: " + MMSEngineDBFacade::toString(ingestionType)
                                    + ", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus)
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + processorMMS
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        ingestionType,
                                        nextIngestionStatus, 
                                        errorMessage,
                                        processorMMS
                                        );

                                thread downloadMediaSource(&MMSEngineProcessor::downloadMediaSourceFile, this, 
                                    mediaSourceURL, ingestionJobKey, customer);
                                downloadMediaSource.detach();
                            }
                            else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress)
                            {
                                string errorMessage = "";
                                string processorMMS = "";

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionType: " + MMSEngineDBFacade::toString(ingestionType)
                                    + ", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus)
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + processorMMS
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        ingestionType,
                                        nextIngestionStatus, 
                                        errorMessage,
                                        processorMMS
                                        );

                                thread moveMediaSource(&MMSEngineProcessor::moveMediaSourceFile, this, 
                                    mediaSourceURL, ingestionJobKey, customer);
                                moveMediaSource.detach();
                            }
                            else if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceCopingInProgress)
                            {
                                string errorMessage = "";
                                string processorMMS = "";

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionType: " + MMSEngineDBFacade::toString(ingestionType)
                                    + ", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus)
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + processorMMS
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        ingestionType,
                                        nextIngestionStatus, 
                                        errorMessage,
                                        processorMMS
                                        );

                                thread copyMediaSource(&MMSEngineProcessor::copyMediaSourceFile, this, 
                                    mediaSourceURL, ingestionJobKey, customer);
                                copyMediaSource.detach();
                            }
                            else // if (nextIngestionStatus == MMSEngineDBFacade::IngestionStatus::SourceUploadingInProgress)
                            {
                                string errorMessage = "";
                                string processorMMS = "";

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionType: " + MMSEngineDBFacade::toString(ingestionType)
                                    + ", IngestionStatus: " + MMSEngineDBFacade::toString(nextIngestionStatus)
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + processorMMS
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        ingestionType,
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
                    else if (ingestionType == 
                            MMSEngineDBFacade::IngestionType::Screenshot)
                    {
                        if (mediaItemKeysDependency == "")
                        {
                            // mediaItemKeysDependency will be filled
                            
                            // we are sure we have one element inside the vector because metadata were validated
                            string dependency = to_string(dependencies.back());
                            string processorMMS = "";

                            _logger->info(__FILEREF__ + "Update IngestionJob"
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", IngestionType: " + MMSEngineDBFacade::toString(ingestionType)
                                + ", dependency: " + dependency
                                + ", processorMMS: " + processorMMS
                            );                            
                            _mmsEngineDBFacade->updateIngestionJobTypeAndDependencies (ingestionJobKey, 
                                    ingestionType, dependency, processorMMS);
                        }
                        else
                        {
                            // mediaItemKeysDependency is present because checked by _mmsEngineDBFacade->getIngestionsToBeManaged
                            try
                            {
                                int64_t sourceMediaItemKey = stoll(mediaItemKeysDependency);
                                
                                generateAndIngestScreenshot(
                                        ingestionJobKey, 
                                        customer, 
                                        metadataRoot, 
                                        sourceMediaItemKey);
                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestScreenshot failed"
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionType: " + MMSEngineDBFacade::toString(ingestionType)
                                    + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        ingestionType,
                                        MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "generateAndIngestScreenshot failed"
                                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                        + ", exception: " + e.what()
                                );

                                string errorMessage = e.what();

                                _logger->info(__FILEREF__ + "Update IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", IngestionType: " + MMSEngineDBFacade::toString(ingestionType)
                                    + ", IngestionStatus: " + "End_ValidationMediaSourceFailed"
                                    + ", errorMessage: " + errorMessage
                                    + ", processorMMS: " + ""
                                );                            
                                _mmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                        ingestionType,
                                        MMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, 
                                        errorMessage,
                                        "" // processorMMS
                                        );

                                throw runtime_error(errorMessage);
                            }
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
        _logger->error(__FILEREF__ + "Error retrieving the Ingestion Jobs to be managed"
                );
    }
}

void MMSEngineProcessor::handleLocalAssetIngestionEvent (
    shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent)
{
    string relativePathToBeUsed;
    try
    {
        relativePathToBeUsed = _mmsEngineDBFacade->checkCustomerMaxIngestionNumber (
                localAssetIngestionEvent->getCustomer()->_customerKey);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "checkCustomerMaxIngestionNumber failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );
        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_CustomerReachedHisMaxIngestionNumber"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                MMSEngineDBFacade::IngestionStatus::End_CustomerReachedHisMaxIngestionNumber,
                e.what(), "" // processorMMS
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "checkCustomerMaxIngestionNumber failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", exception: " + e.what()
        );
        string errorMessage = e.what();

        _logger->info(__FILEREF__ + "Update IngestionJob"
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            + ", IngestionStatus: " + "End_CustomerReachedHisMaxIngestionNumber"
            + ", errorMessage: " + e.what()
        );                            
        _mmsEngineDBFacade->updateIngestionJob (localAssetIngestionEvent->getIngestionJobKey(),
                MMSEngineDBFacade::IngestionStatus::End_CustomerReachedHisMaxIngestionNumber,
                e.what(), "" // processorMMS
        );

        throw e;
    }
                    
    string      metadataFileContent;
    tuple<MMSEngineDBFacade::IngestionType,MMSEngineDBFacade::ContentType,vector<int64_t>>
            ingestionTypeContentTypeAndDependencies;
    MMSEngineDBFacade::IngestionType ingestionType;
    MMSEngineDBFacade::ContentType contentType;
    vector<int64_t> dependencies;
    Json::Value metadataRoot;
    try
    {
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();
        string errors;

        bool parsingSuccessful = reader->parse(localAssetIngestionEvent->getMetadataContent().c_str(),
                localAssetIngestionEvent->getMetadataContent().c_str() + localAssetIngestionEvent->getMetadataContent().size(), 
                &metadataRoot, &errors);
        delete reader;

        if (!parsingSuccessful)
        {
            string errorMessage = __FILEREF__ + "failed to parse the metadata"
                    + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                    + ", errors: " + errors
                    + ", metaDataContent: " + localAssetIngestionEvent->getMetadataContent()
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        ingestionTypeContentTypeAndDependencies = validateMetadata(metadataRoot);
        
        tie(ingestionType, contentType, dependencies) =
                ingestionTypeContentTypeAndDependencies;
    }
    catch(runtime_error e)
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
                localAssetIngestionEvent->getCustomer(),
                ingestionType, metadataRoot);
        
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

    string customerIngestionBinaryPathName;
    try
    {
        customerIngestionBinaryPathName = _mmsStorage->getCustomerIngestionRepository(
                localAssetIngestionEvent->getCustomer());
        customerIngestionBinaryPathName
                .append("/")
                .append(to_string(localAssetIngestionEvent->getIngestionJobKey()))
                .append(".binary")
                ;

        validateMediaSourceFile(customerIngestionBinaryPathName,
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

    unsigned long mmsPartitionIndexUsed;
    string mmsAssetPathName;
    try
    {
        bool partitionIndexToBeCalculated   = true;
        bool deliveryRepositoriesToo        = true;
        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            customerIngestionBinaryPathName,
            localAssetIngestionEvent->getCustomer()->_directoryName,
            mediaSourceFileName,
            relativePathToBeUsed,
            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed,
            deliveryRepositoriesToo,
            localAssetIngestionEvent->getCustomer()->_territories
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

    int64_t durationInMilliSeconds = -1;
    long bitRate;
    string videoCodecName;
    string videoProfile;
    int videoWidth = -1;
    int videoHeight = -1;
    string videoAvgFrameRate;
    long videoBitRate;
    string audioCodecName;
    long audioSampleRate;
    int audioChannels;
    long audioBitRate;

    int imageWidth = -1;
    int imageHeight = -1;
    string imageFormat;
    int imageQuality;
    if (contentType == MMSEngineDBFacade::ContentType::Video 
            || contentType == MMSEngineDBFacade::ContentType::Audio)
    {
        try
        {
            FFMpeg ffmpeg (_configuration, _logger);
            tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> mediaInfo =
                ffmpeg.getMediaInfo(mmsAssetPathName);

            tie(durationInMilliSeconds, bitRate, 
                videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
                audioCodecName, audioSampleRate, audioChannels, audioBitRate) = mediaInfo;
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "EncoderVideoAudioProxy::getMediaInfo failed"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
            );

            _logger->info(__FILEREF__ + "Remove file"
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
    else if (contentType == MMSEngineDBFacade::ContentType::Image)
    {
        try
        {
            _logger->info(__FILEREF__ + "Processing through Magick"
                + ", mmsAssetPathName: " + mmsAssetPathName
            );
            Magick::Image      imageToEncode;

            imageToEncode.read (mmsAssetPathName.c_str());

            imageWidth	= imageToEncode.columns();
            imageHeight	= imageToEncode.rows();
            imageFormat = imageToEncode.magick();
            imageQuality = imageToEncode.quality();
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

    int64_t mediaItemKey;
    try
    {
        bool inCaseOfLinkHasItToBeRead = false;
        unsigned long sizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                inCaseOfLinkHasItToBeRead);   

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata..."
            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
        );

        pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey =
                _mmsEngineDBFacade->saveIngestedContentMetadata (
                    localAssetIngestionEvent->getCustomer(),
                    localAssetIngestionEvent->getIngestionJobKey(),
                    metadataRoot,
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
        );

        _logger->info(__FILEREF__ + "Remove file"
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
    
    /*
    // ingest Screenshots if present
    if (ingestionTypeAndContentType.second == MMSEngineDBFacade::ContentType::Video)
    {        
        string field = "ContentIngestion";
        Json::Value contentIngestion = metadataRoot[field]; 

        field = "Screenshots";
        if (_mmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        {
            const Json::Value screenshots = contentIngestion[field];

            field = "Title";
            if (!_mmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string videoTitle = contentIngestion.get(field, "XXX").asString();

            _logger->info(__FILEREF__ + "Processing the screenshots"
                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                + ", screenshots.size(): " + to_string(screenshots.size())
            );
            
            for(int screenshotIndex = 0; screenshotIndex < screenshots.size(); screenshotIndex++) 
            {
                try
                {
                    Json::Value screenshot = screenshots[screenshotIndex];

                    field = "TimePositionInSeconds";
                    if (!_mmsEngineDBFacade->isMetadataPresent(screenshot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    double timePositionInSeconds = screenshot.get(field, "XXX").asDouble();

                    field = "EncodingProfilesSet";
                    string encodingProfilesSet;
                    if (!_mmsEngineDBFacade->isMetadataPresent(screenshot, field))
                        encodingProfilesSet = "customerDefault";
                    else
                        encodingProfilesSet = screenshot.get(field, "XXX").asString();

                    field = "SourceImageWidth";
                    if (!_mmsEngineDBFacade->isMetadataPresent(screenshot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    int sourceImageWidth = screenshot.get(field, "XXX").asInt();

                    field = "SourceImageHeight";
                    if (!_mmsEngineDBFacade->isMetadataPresent(screenshot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    int sourceImageHeight = screenshot.get(field, "XXX").asInt();

                    if (videoOrAudioDurationInMilliSeconds < timePositionInSeconds * 1000)
                    {
                        string errorMessage = __FILEREF__ + "Screenshot was not generated because timePositionInSeconds is bigger than the video duration"
                                + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                                + ", video mediaItemKey: " + to_string(mediaItemKey)
                                + ", timePositionInSeconds: " + to_string(timePositionInSeconds)
                                + ", videoOrAudioDurationInMilliSeconds: " + to_string(videoOrAudioDurationInMilliSeconds)
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    
                    string imageFileName;
                    {
                        size_t fileExtensionIndex = mediaSourceFileName.find_last_of(".");
                        if (fileExtensionIndex == string::npos)
                            imageFileName.append(mediaSourceFileName);
                        else
                            imageFileName.append(mediaSourceFileName.substr(0, fileExtensionIndex));
                        imageFileName
                                .append("_")
                                .append(to_string(screenshotIndex + 1))
                                .append(".jpg")
                        ;
                    }

                    {
                        shared_ptr<GenerateImageToIngestEvent>    generateImageToIngestEvent = _multiEventsSet->getEventsFactory()
                                ->getFreeEvent<GenerateImageToIngestEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_GENERATEIMAGETOINGESTEVENT);

                        generateImageToIngestEvent->setSource(MMSENGINEPROCESSORNAME);
                        generateImageToIngestEvent->setDestination(MMSENGINEPROCESSORNAME);
                        generateImageToIngestEvent->setExpirationTimePoint(chrono::system_clock::now());

                        generateImageToIngestEvent->setCmsVideoPathName(mmsAssetPathName);
                        generateImageToIngestEvent->setCustomer(localAssetIngestionEvent->getCustomer());
                        generateImageToIngestEvent->setImageFileName(imageFileName);
                        generateImageToIngestEvent->setImageTitle(videoTitle + " image #" + to_string(screenshotIndex + 1));

                        generateImageToIngestEvent->setTimePositionInSeconds(timePositionInSeconds);
                        generateImageToIngestEvent->setEncodingProfilesSet(encodingProfilesSet);
                        generateImageToIngestEvent->setSourceImageWidth(sourceImageWidth);
                        generateImageToIngestEvent->setSourceImageHeight(sourceImageHeight);

                        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(generateImageToIngestEvent);
                        _multiEventsSet->addEvent(event);

                        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (GENERATEIMAGETOINGESTEVENT) to generate the screenshot"
                            + ", ingestionJobKey: " + to_string(localAssetIngestionEvent->getIngestionJobKey())
                            + ", imageFileName: " + imageFileName
                            + ", getEventKey().first: " + to_string(event->getEventKey().first)
                            + ", getEventKey().second: " + to_string(event->getEventKey().second));
                    }
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "Prepare generation image to ingest failed"
                        + ", screenshotIndex: " + to_string(screenshotIndex)
                    );
                }
            }
        }
    }
     */
}

void MMSEngineProcessor::generateAndIngestScreenshot(
        int64_t ingestionJobKey,
        shared_ptr<Customer> customer,
        Json::Value metadataRoot,
        int64_t sourceMediaItemKey
)
{
    try
    {
        string field = "Screenshot";
        Json::Value screenshotRoot = metadataRoot[field];

        field = "TimePositionInSeconds";
        if (!_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        double timePositionInSeconds = screenshotRoot.get(field, "XXX").asDouble();

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
            string errorMessage = __FILEREF__ + "EncoderVideoAudioProxy::getMediaInfo failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        catch(exception e)
        {
            string errorMessage = __FILEREF__ + "EncoderVideoAudioProxy::getMediaInfo failed"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", e.what(): " + e.what()
            ;

            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        if (durationInMilliSeconds < timePositionInSeconds * 1000)
        {
            string errorMessage = __FILEREF__ + "Screenshot was not generated because timePositionInSeconds is bigger than the video duration"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", video sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                    + ", timePositionInSeconds: " + to_string(timePositionInSeconds)
                    + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        string imagePathName = _mmsStorage->getCustomerIngestionRepository(
                customer)
                + "/"
                + to_string(ingestionJobKey)
                + ".binary"
        ;

        FFMpeg ffmpeg (_configuration, _logger);

        ffmpeg.generateScreenshotToIngest(
                imagePathName,
                timePositionInSeconds,
                videoWidth, 
                videoHeight,
                sourcePhysicalPath
        );

        _logger->info(__FILEREF__ + "Generated Screenshot to ingest"
            + ", imagePathName: " + imagePathName
        );

        string imageMetaDataContent = generateImageMetadataToIngest(
                ingestionJobKey,
                screenshotRoot
        );
        
        {
            shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                    ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

            localAssetIngestionEvent->setSource(MMSENGINEPROCESSORNAME);
            localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
            localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

            localAssetIngestionEvent->setIngestionJobKey(ingestionJobKey);
            localAssetIngestionEvent->setCustomer(customer);

            localAssetIngestionEvent->setMetadataContent(imageMetaDataContent);

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
        _logger->error(__FILEREF__ + "generateAndIngestScreenshot failed"
            + ", e.what(): " + e.what()
        );
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "generateAndIngestScreenshot failed"
        );
        
        throw e;
    }
}

string MMSEngineProcessor::generateImageMetadataToIngest(
        int64_t ingestionJobKey,
        Json::Value screenshotRoot
)
{
    string field = "SourceFileName";
    if (!_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    string imageFileName = screenshotRoot.get(field, "XXX").asString();
        
    string title;
    field = "title";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        title = screenshotRoot.get(field, "XXX").asString();
    
    string subTitle;
    field = "SubTitle";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        subTitle = screenshotRoot.get(field, "XXX").asString();

    string ingester;
    field = "Ingester";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        ingester = screenshotRoot.get(field, "XXX").asString();

    string keywords;
    field = "Keywords";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        keywords = screenshotRoot.get(field, "XXX").asString();

    string description;
    field = "Description";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        description = screenshotRoot.get(field, "XXX").asString();

    string logicalType;
    field = "LogicalType";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        logicalType = screenshotRoot.get(field, "XXX").asString();

    string encodingProfilesSet;
    field = "EncodingProfilesSet";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        encodingProfilesSet = screenshotRoot.get(field, "XXX").asString();

    string encodingPriority;
    field = "EncodingPriority";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        encodingPriority = screenshotRoot.get(field, "XXX").asString();

    string contentProviderName;
    field = "ContentProviderName";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
        contentProviderName = screenshotRoot.get(field, "XXX").asString();
    
    string territories;
    field = "Territories";
    if (_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, field))
    {
        {
            Json::StreamWriterBuilder wbuilder;
            
            territories = Json::writeString(wbuilder, screenshotRoot[field]);
        }
    }
    
    string imageMetadata = string("")
        + "{"
            + "\"Type\": \"ContentIngestion\""
            + ", \"ContentIngestion\": {"
                + "\"ContentType\": \"image\""
                + ", \"SourceFileName\": \"" + imageFileName + "\""
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
    if (encodingProfilesSet != "")
        imageMetadata += ", \"EncodingProfilesSet\": \"" + encodingProfilesSet + "\"";
    if (encodingPriority != "")
        imageMetadata += ", \"EncodingPriority\": \"" + encodingPriority + "\"";
    if (contentProviderName != "")
        imageMetadata += ", \"ContentProviderName\": \"" + contentProviderName + "\"";
    if (territories != "")
        imageMetadata += ", \"Territories\": \"" + territories + "\"";
                            
    imageMetadata +=
            string("}")
        + "}"
    ;
    
    _logger->info(__FILEREF__ + "Image metadata generated"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", imageMetadata: " + imageMetadata
            );

    return imageMetadata;
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

tuple<MMSEngineDBFacade::IngestionType,MMSEngineDBFacade::ContentType,vector<int64_t>> 
        MMSEngineProcessor::validateMetadata(Json::Value metadataRoot)
{
    tuple<MMSEngineDBFacade::IngestionType,MMSEngineDBFacade::ContentType,vector<int64_t>> 
            ingestionTypeContentTypeAndDependencies;
    
    MMSEngineDBFacade::IngestionType    ingestionType;
    MMSEngineDBFacade::ContentType      contentType;
    vector<int64_t>                     dependencies;

    string field = "Type";
    if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string type = metadataRoot.get("Type", "XXX").asString();
    if (type == "ContentIngestion")
        ingestionType = MMSEngineDBFacade::IngestionType::ContentIngestion;
    else if (type == "Screenshot")
        ingestionType = MMSEngineDBFacade::IngestionType::Screenshot;
    /*
    else if (type == "ContentRemove")
        ingestionType = MMSEngineDBFacade::IngestionType::ContentRemove;
    */
    else
    {
        string errorMessage = __FILEREF__ + "Field 'Type' is wrong"
                + ", Type: " + type;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    if (ingestionType == MMSEngineDBFacade::IngestionType::ContentIngestion)
    {
        string field = "ContentIngestion";
        if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value contentIngestionRoot = metadataRoot[field]; 

        contentType = validateContentIngestionMetadata(contentIngestionRoot);
    }
    else if (ingestionType == MMSEngineDBFacade::IngestionType::Screenshot)
    {
        string field = "Screenshot";
        if (!_mmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value screenshotRoot = metadataRoot[field]; 

        contentType = validateScreenshotMetadata(screenshotRoot, dependencies);
    }
    
    ingestionTypeContentTypeAndDependencies = make_tuple(ingestionType, contentType, dependencies);

    
    return ingestionTypeContentTypeAndDependencies;
}

MMSEngineDBFacade::ContentType MMSEngineProcessor::validateContentIngestionMetadata(
    Json::Value contentIngestion)
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
        if (!_mmsEngineDBFacade->isMetadataPresent(contentIngestion, mandatoryField))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string sContentType = contentIngestion.get("ContentType", "XXX").asString();
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
        if (_mmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        {
            string encodingPriority = contentIngestion.get(field, "XXX").asString();
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

MMSEngineDBFacade::ContentType MMSEngineProcessor::validateScreenshotMetadata(
    Json::Value screenshotRoot, vector<int64_t>& dependencies)
{
    // see sample in directory samples
    
    MMSEngineDBFacade::ContentType         contentType;
    
    vector<string> mandatoryFields = {
        "UniqueName",
        "SourceFileName"
    };
    for (string mandatoryField: mandatoryFields)
    {
        if (!_mmsEngineDBFacade->isMetadataPresent(screenshotRoot, mandatoryField))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + mandatoryField;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string field = "UniqueName";
    string uniqueName = screenshotRoot.get(field, "XXX").asString();
    
    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndcontentType;
    try
    {
        mediaItemKeyAndcontentType = _mmsEngineDBFacade->getMediaItemKeyDetails(uniqueName);        
    }
    catch(MediaItemKeyNotFound e)
    {
        string errorMessage = __FILEREF__ + "UniqueName was not found"
                + ", uniqueName: " + uniqueName;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    catch(exception e)
    {
        string errorMessage = __FILEREF__ + "_mmsEngineDBFacade->getMediaItemKeyDetails failed"
                + ", uniqueName: " + uniqueName;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    if (mediaItemKeyAndcontentType.second != MMSEngineDBFacade::ContentType::Video)
    {
        string errorMessage = __FILEREF__ + "UniqueName does not refer a video content"
                + ", uniqueName: " + uniqueName;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    dependencies.push_back(mediaItemKeyAndcontentType.first);

    contentType = MMSEngineDBFacade::ContentType::Image;

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

tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> MMSEngineProcessor::getMediaSourceDetails(
        shared_ptr<Customer> customer, MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value root)        
{
    MMSEngineDBFacade::IngestionStatus nextIngestionStatus;
    string mediaSourceURL;
    string mediaSourceFileName;
    
    string field = "ContentIngestion";
    Json::Value contentIngestion = root[field]; 

    if (ingestionType == MMSEngineDBFacade::IngestionType::ContentIngestion)
    {
        field = "SourceURL";
        if (_mmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
            mediaSourceURL = contentIngestion.get(field, "XXX").asString();
        
        field = "SourceFileName";
        mediaSourceFileName = contentIngestion.get(field, "XXX").asString();

        string httpPrefix ("http://");
        string httpsPrefix ("https://");
        string ftpPrefix ("ftp://");
        string ftpsPrefix ("ftps://");
        string movePrefix("move://");   // move:///dir1/dir2/.../file
        string copyPrefix("copy://");
        if (!mediaSourceURL.compare(0, httpPrefix.size(), httpPrefix)
                || !mediaSourceURL.compare(0, httpsPrefix.size(), httpsPrefix)
                || !mediaSourceURL.compare(0, ftpPrefix.size(), ftpPrefix)
                || !mediaSourceURL.compare(0, ftpsPrefix.size(), ftpsPrefix)
                )
        {
            nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress;
        }
        else if (!mediaSourceURL.compare(0, movePrefix.size(), movePrefix))
        {
            nextIngestionStatus = MMSEngineDBFacade::IngestionStatus::SourceMovingInProgress;            
        }
        else if (!mediaSourceURL.compare(0, copyPrefix.size(), copyPrefix))
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
                + ", ingestionType: " + to_string(static_cast<int>(ingestionType));
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string md5FileCheckSum;
    field = "MD5FileCheckSum";
    if (_mmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
    {
        MD5         md5;
        char        md5RealDigest [32 + 1];

        md5FileCheckSum = contentIngestion.get(field, "XXX").asString();
    }

    int fileSizeInBytes = -1;
    field = "FileSizeInBytes";
    if (_mmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        fileSizeInBytes = contentIngestion.get(field, 3).asInt();

    tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> mediaSourceDetails;
    get<0>(mediaSourceDetails) = nextIngestionStatus;
    get<1>(mediaSourceDetails) = mediaSourceURL;
    get<2>(mediaSourceDetails) = mediaSourceFileName;
    get<3>(mediaSourceDetails) = md5FileCheckSum;
    get<4>(mediaSourceDetails) = fileSizeInBytes;

    _logger->info(__FILEREF__ + "media source details"
        + ", nextIngestionStatus: " + MMSEngineDBFacade::toString(get<0>(mediaSourceDetails))
        + ", mediaSourceURL: " + get<1>(mediaSourceDetails)
        + ", mediaSourceFileName: " + get<2>(mediaSourceDetails)
        + ", md5FileCheckSum: " + get<3>(mediaSourceDetails)
        + ", fileSizeInBytes: " + to_string(get<4>(mediaSourceDetails))
    );

    
    return mediaSourceDetails;
}

void MMSEngineProcessor::validateMediaSourceFile (string ftpDirectoryMediaSourceFileName,
        string md5FileCheckSum, int fileSizeInBytes)
{
    if (!FileIO::fileExisting(ftpDirectoryMediaSourceFileName))
    {
        string errorMessage = __FILEREF__ + "Media Source file does not exist (it was not uploaded yet)"
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
        unsigned long realFileSizeInBytes = 
            FileIO:: getFileSizeInBytes (ftpDirectoryMediaSourceFileName, inCaseOfLinkHasItToBeRead);

        if (fileSizeInBytes != realFileSizeInBytes)
        {
            string errorMessage = __FILEREF__ + "FileSize check failed"
                + ", ftpDirectoryMediaSourceFileName: " + ftpDirectoryMediaSourceFileName
                + ", fileSizeInBytes: " + to_string(fileSizeInBytes)
                + ", realFileSizeInBytes: " + to_string(realFileSizeInBytes)
            ;
            _logger->error(errorMessage);
            throw runtime_error(errorMessage);
        }
    }    
}

void MMSEngineProcessor::downloadMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Customer> customer)
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
 
        
    for (int attemptIndex = 0; attemptIndex < _maxDownloadAttemptNumber && !downloadingCompleted; attemptIndex++)
    {
        bool downloadingStoppedByUser = false;
        
        try 
        {
            _logger->info(__FILEREF__ + "Downloading"
                + ", sourceReferenceURL: " + sourceReferenceURL
                + ", attempt: " + to_string(attemptIndex + 1)
                + ", _maxDownloadAttemptNumber: " + to_string(_maxDownloadAttemptNumber)
            );

            string customerIngestionBinaryPathName = _mmsStorage->getCustomerIngestionRepository(customer);
            customerIngestionBinaryPathName
                .append("/")
                .append(to_string(ingestionJobKey))
                .append(".binary")
                ;
            
            if (attemptIndex == 0)
            {
                ofstream mediaSourceFileStream(customerIngestionBinaryPathName, ofstream::binary | ofstream::trunc);

                curlpp::Cleanup cleaner;
                curlpp::Easy request;

                // Set the writer callback to enable cURL 
                // to write result in a memory area
                request.setOpt(new curlpp::options::WriteStream(&mediaSourceFileStream));

                // Setting the URL to retrive.
                request.setOpt(new curlpp::options::Url(sourceReferenceURL));
                string httpsPrefix("https");
                if (sourceReferenceURL.compare(0, httpsPrefix.size(), httpsPrefix) == 0)
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

                request.setOpt(new curlpp::options::NoProgress(0L));

                _logger->info(__FILEREF__ + "Downloading media file"
                    + ", sourceReferenceURL: " + sourceReferenceURL
                );
                request.perform();
            }
            else
            {
                _logger->warn(__FILEREF__ + "Coming from a download failure, trying to Resume");
                
                ofstream mediaSourceFileStream(customerIngestionBinaryPathName, ofstream::binary | ofstream::app);

                curlpp::Cleanup cleaner;
                curlpp::Easy request;

                // Set the writer callback to enable cURL 
                // to write result in a memory area
                request.setOpt(new curlpp::options::WriteStream(&mediaSourceFileStream));

                // Setting the URL to retrive.
                request.setOpt(new curlpp::options::Url(sourceReferenceURL));
                string httpsPrefix("https");
                if (sourceReferenceURL.compare(0, httpsPrefix.size(), httpsPrefix) == 0)
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

                long long fileSize = mediaSourceFileStream.tellp();
                if (fileSize > 2 * 1000 * 1000 * 1000)
                    request.setOpt(new curlpp::options::ResumeFromLarge(fileSize));
                else
                    request.setOpt(new curlpp::options::ResumeFrom(fileSize));
                
                _logger->info(__FILEREF__ + "Resume Download media file"
                    + ", sourceReferenceURL: " + sourceReferenceURL
                    + ", resuming from fileSize: " + to_string(fileSize)
                );
                request.perform();
            }

            downloadingCompleted = true;

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", customerIngestionBinaryPathName: " + customerIngestionBinaryPathName
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
        int64_t ingestionJobKey, shared_ptr<Customer> customer)
{

    try 
    {
        string customerIngestionBinaryPathName = _mmsStorage->getCustomerIngestionRepository(customer);
        customerIngestionBinaryPathName
            .append("/")
            .append(to_string(ingestionJobKey))
            .append(".binary")
            ;

        string movePrefix("move://");
        if (sourceReferenceURL.compare(0, movePrefix.size(), movePrefix))
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
            + ", sourcePathName: " + sourcePathName
            + ", customerIngestionBinaryPathName: " + customerIngestionBinaryPathName
        );
        
        FileIO::moveFile(sourcePathName, customerIngestionBinaryPathName);
            
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
        int64_t ingestionJobKey, shared_ptr<Customer> customer)
{

    try 
    {
        string customerIngestionBinaryPathName = _mmsStorage->getCustomerIngestionRepository(customer);
        customerIngestionBinaryPathName
            .append("/")
            .append(to_string(ingestionJobKey))
            .append(".binary")
            ;

        string copyPrefix("copy://");
        if (sourceReferenceURL.compare(0, copyPrefix.size(), copyPrefix))
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
            + ", sourcePathName: " + sourcePathName
            + ", customerIngestionBinaryPathName: " + customerIngestionBinaryPathName
        );
        
        FileIO::copyFile(sourcePathName, customerIngestionBinaryPathName);
            
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