
#include <fstream>
#include <sstream>
#include <regex>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include "catralibraries/System.h"
#include "CMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "CheckEncodingTimes.h"
#include "catralibraries/md5.h"

CMSEngineProcessor::CMSEngineProcessor(
        shared_ptr<spdlog::logger> logger, 
        shared_ptr<MultiEventsSet> multiEventsSet,
        shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
        shared_ptr<CMSStorage> cmsStorage,
        ActiveEncodingsManager* pActiveEncodingsManager
)
{
    _logger             = logger;
    _multiEventsSet     = multiEventsSet;
    _cmsEngineDBFacade  = cmsEngineDBFacade;
    _cmsStorage      = cmsStorage;
    _pActiveEncodingsManager = pActiveEncodingsManager;

    _ulIngestionLastCustomerIndex   = 0;
    _firstGetEncodingJob            = true;
    _processorCMS                   = System::getHostName();
    _maxDownloadAttemptNumber       = 3;
    
    _ulMaxIngestionsNumberPerCustomerEachIngestionPeriod        = 2;
    _ulJsonToBeProcessedAfterSeconds                            = 10;
    _ulRetentionPeriodInDays                                    = 10;
}

CMSEngineProcessor::~CMSEngineProcessor()
{
    
}

void CMSEngineProcessor::operator ()() 
{
    bool blocking = true;
    chrono::milliseconds milliSecondsToBlock(100);

    //SPDLOG_DEBUG(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    // SPDLOG_TRACE(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    _logger->info(__FILEREF__ + "CMSEngineProcessor thread started");

    bool endEvent = false;
    while(!endEvent)
    {
        // cout << "Calling getAndRemoveFirstEvent" << endl;
        shared_ptr<Event> event = _multiEventsSet->getAndRemoveFirstEvent(CMSENGINEPROCESSORNAME, blocking, milliSecondsToBlock);
        if (event == nullptr)
        {
            // cout << "No event found or event not yet expired" << endl;

            continue;
        }

        switch(event->getEventKey().first)
        {
            case CMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTIONEVENT:	// 1
            {
                _logger->debug(__FILEREF__ + "Received CMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION");

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

                _multiEventsSet->getEventsFactory()->releaseEvent<Event>(event);

            }
            break;
            case CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT:	// 2
            {
                _logger->info(__FILEREF__ + "Received CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT");

                shared_ptr<IngestAssetEvent>    ingestAssetEvent = dynamic_pointer_cast<IngestAssetEvent>(event);

                try
                {
                    handleIngestAssetEvent (ingestAssetEvent);
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "handleIngestAssetEvent failed"
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<IngestAssetEvent>(ingestAssetEvent);
            }
            break;
            case CMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODINGEVENT:	// 3
            {
                _logger->debug(__FILEREF__ + "Received CMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING");

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

                _multiEventsSet->getEventsFactory()->releaseEvent<Event>(event);

            }
            break;
            case CMSENGINE_EVENTTYPEIDENTIFIER_GENERATEIMAGETOINGESTEVENT:	// 4
            {
                _logger->debug(__FILEREF__ + "Received CMSENGINE_EVENTTYPEIDENTIFIER_GENERATEIMAGETOINGESTEVENT");

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
            default:
                throw runtime_error(string("Event type identifier not managed")
                        + to_string(event->getEventKey().first));
        }
    }

    _logger->info(__FILEREF__ + "CMSEngineProcessor thread terminated");
}

void CMSEngineProcessor::handleCheckIngestionEvent()
{
    
    vector<shared_ptr<Customer>> customers = _cmsEngineDBFacade->getCustomers();
    
    unsigned long ulCurrentCustomerIndex = 0;
    vector<shared_ptr<Customer>>::iterator itCustomers;
    
    for (itCustomers = customers.begin();
            itCustomers != customers.end() && ulCurrentCustomerIndex < _ulIngestionLastCustomerIndex;
            ++itCustomers, ulCurrentCustomerIndex++)
    {
    }

    for (; itCustomers != customers.end(); ++itCustomers)
    {
        shared_ptr<Customer> customer = *itCustomers;

        try
        {
            _logger->debug(__FILEREF__ + "Looking for ingestions"
                + ", customer->_customerKey: " + to_string(customer->_customerKey)
                + ", customer->_customerKey: " + customer->_name
            );
            _ulIngestionLastCustomerIndex++;

            string customerFTPDirectory = _cmsStorage->getCustomerFTPRepository(customer);

            shared_ptr<FileIO::Directory> directory = FileIO:: openDirectory(customerFTPDirectory);

            unsigned long ulCurrentCustomerIngestionsNumber		= 0;

            bool moreContentsToBeProcessed = true;
            while (moreContentsToBeProcessed)
            {
                FileIO:: DirectoryEntryType_t	detDirectoryEntryType;
                string directoryEntry;
                
                try
                {
                    directoryEntry = FileIO::readDirectory (directory,
                        &detDirectoryEntryType);
                }
                catch(DirectoryListFinished dlf)
                {
                    moreContentsToBeProcessed = false;

                    continue;
                }
                catch(...)
                {
                    _logger->error(__FILEREF__ + "FileIO::readDirectory failed");

                    moreContentsToBeProcessed = false;

                    break;
                }

                if (detDirectoryEntryType != FileIO:: TOOLS_FILEIO_REGULARFILE)
                {
                    continue;
                }

                // managing the FTP entry
                try
                {
                    string srcPathName(customerFTPDirectory);
                    srcPathName
                        .append("/")
                        .append(directoryEntry);

                    chrono::system_clock::time_point lastModificationTime = 
                        FileIO:: getFileTime (srcPathName);

                    if (chrono::duration_cast<chrono::hours>(chrono::system_clock::now() - lastModificationTime) 
                            >= chrono::hours(_ulRetentionPeriodInDays * 24))
                    {
                        _logger->info(__FILEREF__ + "Remove obsolete FTP file"
                            + ", srcPathName: " + srcPathName
                            + ", _ulRetentionPeriodInDays: " + to_string(_ulRetentionPeriodInDays)
                        );

                        FileIO::remove (srcPathName);

                        continue;
                    }

                    // check if the file has ".json" as extension.
                    // We do not accept also the ".json" file (without name)
                    string jsonExtension(".json");
                    if (directoryEntry.length() < 6 ||
                            !equal(jsonExtension.rbegin(), jsonExtension.rend(), directoryEntry.rbegin()))
                            continue;

                    if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastModificationTime) 
                            < chrono::seconds(_ulJsonToBeProcessedAfterSeconds))
                    {
                        // only the json files having the last modification older
                        // at least of _ulJsonToBeProcessedAfterSeconds seconds
                        // are considered

                        continue;
                    }

                    _logger->info(__FILEREF__ + "json to be processed"
                        + ", directoryEntry: " + directoryEntry
                    );

                    string ftpDirectoryWorkingMetadataPathName =
                            _cmsStorage->moveFTPRepositoryEntryToWorkingArea(customer, directoryEntry);

                    string      metadataFileContent;
                    pair<CMSEngineDBFacade::IngestionType,CMSEngineDBFacade::ContentType> ingestionTypeAndContentType;
                    Json::Value metadataRoot;
                    try
                    {
                        {
                            ifstream medatataFile(ftpDirectoryWorkingMetadataPathName);
                            stringstream buffer;
                            buffer << medatataFile.rdbuf();

                            metadataFileContent = buffer.str();
                        }

                        ifstream ingestAssetJson(ftpDirectoryWorkingMetadataPathName, std::ifstream::binary);
                        try
                        {
                            ingestAssetJson >> metadataRoot;
                        }
                        catch(...)
                        {
                            throw runtime_error(string("wrong ingestion metadata json format")
                                    + ", directoryEntry: " + directoryEntry
                                    );
                        }

                        ingestionTypeAndContentType = validateMetadata(metadataRoot);
                    }
                    catch(runtime_error e)
                    {
                        _logger->error(__FILEREF__ + "validateMetadata failed"
                                + ", exception: " + e.what()
                        );
                        string ftpDirectoryErrorEntryPathName =
                            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);

                        string errorMessage = e.what();

                        _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                directoryEntry, metadataFileContent, CMSEngineDBFacade::IngestionType::Unknown, 
                                CMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, 
                                errorMessage);

                        throw e;
                    }
                    catch(exception e)
                    {
                        _logger->error(__FILEREF__ + "validateMetadata failed"
                                + ", exception: " + e.what()
                        );
                        string ftpDirectoryErrorEntryPathName =
                            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);

                        string errorMessage = e.what();

                        _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                directoryEntry, metadataFileContent, CMSEngineDBFacade::IngestionType::Unknown, 
                                CMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed, 
                                errorMessage);

                        throw e;
                    }

                    tuple<bool, string, string, string, int> mediaSourceDetails;
                    try
                    {
                        mediaSourceDetails = getMediaSourceDetails(
                                ingestionTypeAndContentType.first, metadataRoot);
                    }
                    catch(runtime_error e)
                    {
                        _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                                + ", exception: " + e.what()
                        );
                        string ftpDirectoryErrorEntryPathName =
                            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);

                        string errorMessage = e.what();

                        _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                directoryEntry, metadataFileContent, ingestionTypeAndContentType.first, 
                                CMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, 
                                errorMessage);

                        throw e;
                    }
                    catch(exception e)
                    {
                        _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                                + ", exception: " + e.what()
                        );
                        string ftpDirectoryErrorEntryPathName =
                            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);

                        string errorMessage = e.what();

                        _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                directoryEntry, metadataFileContent, ingestionTypeAndContentType.first, 
                                CMSEngineDBFacade::IngestionStatus::End_ValidationMediaSourceFailed, 
                                errorMessage);

                        throw e;
                    }

                    int64_t ingestionJobKey;
                    try
                    {
                        string errorMessage = "";
                        ingestionJobKey = _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                            directoryEntry, metadataFileContent, ingestionTypeAndContentType.first, 
                            CMSEngineDBFacade::IngestionStatus::StartIngestion, 
                            errorMessage);
                    }
                    catch(exception e)
                    {
                        _logger->error(__FILEREF__ + "_cmsEngineDBFacade->addIngestionJob failed"
                                + ", exception: " + e.what()
                        );

                        string ftpDirectoryErrorEntryPathName =
                            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);
                    
                        throw e;
                    }

                    try
                    {
                        // mediaSourceReference could be a URL or a filename.
                        // In this last case, it will be the same as mediaSourceFileName
                        
                        bool mediaSourceToBeDownload;
                        string mediaSourceReference;
                        string mediaSourceFileName;
                        string md5FileCheckSum;
                        int fileSizeInBytes;

                        tie(mediaSourceToBeDownload, mediaSourceReference,
                                mediaSourceFileName, md5FileCheckSum, fileSizeInBytes) 
                                = mediaSourceDetails;                        

                        if (mediaSourceToBeDownload)
                        {          
                            _cmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                                    CMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress, "");
                            
                            thread downloadMediaSource(&CMSEngineProcessor::downloadMediaSourceFile, this, 
                                mediaSourceReference, ingestionJobKey, customer,
                                directoryEntry, ftpDirectoryWorkingMetadataPathName,
                                customerFTPDirectory, mediaSourceFileName);
                            downloadMediaSource.detach();
                            
                            /*
                            string ftpDirectoryWorkingMetadataPathName =
                                _cmsStorage->moveFTPRepositoryEntryToSourceDownloadingArea(
                                    customer, directoryEntry, _processorCMS);

                            _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                directoryEntry, metadataFileContent, ingestionTypeAndContentType.first, 
                                CMSEngineDBFacade::IngestionStatus::SourceDownloadingInProgress, 
                                errorMessage);

                             */
                            // start downloading
                        }
                        else
                        {            
                            string ftpMediaSourcePathName   =
                                    customerFTPDirectory
                                    + "/"
                                    + mediaSourceFileName;
                            
                            shared_ptr<IngestAssetEvent>    ingestAssetEvent = _multiEventsSet->getEventsFactory()
                                    ->getFreeEvent<IngestAssetEvent>(CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT);

                            ingestAssetEvent->setSource(CMSENGINEPROCESSORNAME);
                            ingestAssetEvent->setDestination(CMSENGINEPROCESSORNAME);
                            ingestAssetEvent->setExpirationTimePoint(chrono::system_clock::now());

                            ingestAssetEvent->setIngestionJobKey(ingestionJobKey);
                            ingestAssetEvent->setCustomer(customer);

                            ingestAssetEvent->setMetadataFileName(directoryEntry);
                            ingestAssetEvent->setFTPWorkingMetadataPathName(ftpDirectoryWorkingMetadataPathName);
                            ingestAssetEvent->setFTPMediaSourcePathName(ftpMediaSourcePathName);
                            ingestAssetEvent->setMediaSourceFileName(mediaSourceFileName);

                            // ingestAssetEvent->setFTPDirectoryMediaSourceFileName(ftpDirectoryMediaSourceFileName);
//                            ingestAssetEvent->setMediaSourceFileName(mediaSourceFileName);
//
//                            ingestAssetEvent->setRelativePath(relativePathToBeUsed);
//                            ingestAssetEvent->setMetadataRoot(metadataRoot);

                            shared_ptr<Event>    event = dynamic_pointer_cast<Event>(ingestAssetEvent);
                            _multiEventsSet->addEvent(event);

                            _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
                                + ", mediaSourceReference: " + mediaSourceReference
                                + ", getEventKey().first: " + to_string(event->getEventKey().first)
                                + ", getEventKey().second: " + to_string(event->getEventKey().second));
                        }
                    }
                    catch(exception e)
                    {
                        _logger->error(__FILEREF__ + "add IngestAssetEvent failed"
                                + ", exception: " + e.what()
                        );

                        string ftpDirectoryErrorEntryPathName =
                            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);

                        _cmsEngineDBFacade->updateIngestionJob (ingestionJobKey, CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
                    }

                    ulCurrentCustomerIngestionsNumber++;

                    if (ulCurrentCustomerIngestionsNumber >=
                        _ulMaxIngestionsNumberPerCustomerEachIngestionPeriod)
                        break;
                }
                catch(exception e)
                {
                    _logger->error(__FILEREF__ + "Exception managing the FTP entry"
                        + ", exception: " + e.what()
                        + ", customerKey: " + to_string(customer->_customerKey)
                        + ", directoryEntry: " + directoryEntry
                    );
                }
            }

            FileIO:: closeDirectory (directory);
        }
        catch(...)
        {
            _logger->error(__FILEREF__ + "Error processing the Customer FTP"
                + ", customer->_name: " + customer->_name
                    );
        }
    }

    if (itCustomers == customers.end())
        _ulIngestionLastCustomerIndex	= 0;

}

void CMSEngineProcessor::handleIngestAssetEvent (shared_ptr<IngestAssetEvent> ingestAssetEvent)
{
    string relativePathToBeUsed;
    try
    {
        relativePathToBeUsed = _cmsEngineDBFacade->checkCustomerMaxIngestionNumber (
                ingestAssetEvent->getCustomer()->_customerKey);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "checkCustomerMaxIngestionNumber failed"
                + ", exception: " + e.what()
        );
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        string errorMessage = e.what();

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
                CMSEngineDBFacade::IngestionStatus::End_CustomerReachedHisMaxIngestionNumber,
                e.what());

        throw e;
    }
                    
    string      metadataFileContent;
    pair<CMSEngineDBFacade::IngestionType,CMSEngineDBFacade::ContentType> ingestionTypeAndContentType;
    Json::Value metadataRoot;
    try
    {
        {
            ifstream medatataFile(ingestAssetEvent->getFTPWorkingMetadataPathName());
            stringstream buffer;
            buffer << medatataFile.rdbuf();

            metadataFileContent = buffer.str();
        }

        ifstream ingestAssetJson(ingestAssetEvent->getFTPWorkingMetadataPathName(), std::ifstream::binary);
        try
        {
            ingestAssetJson >> metadataRoot;
        }
        catch(...)
        {
            throw runtime_error(string("wrong ingestion metadata json format")
                    + ", ingestAssetEvent->getFTPWorkingMetadataPathName: " + ingestAssetEvent->getFTPWorkingMetadataPathName()
                    );
        }

        ingestionTypeAndContentType = validateMetadata(metadataRoot);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "validateMetadata failed"
                + ", exception: " + e.what()
        );
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        string errorMessage = e.what();

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
            CMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what());

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "validateMetadata failed"
                + ", exception: " + e.what()
        );
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        string errorMessage = e.what();

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
            CMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what());

        throw e;
    }

    tuple<bool, string, string, string, int> mediaSourceDetails;
    try
    {
        mediaSourceDetails = getMediaSourceDetails(
                ingestionTypeAndContentType.first, metadataRoot);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                + ", exception: " + e.what()
        );
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        string errorMessage = e.what();

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
            CMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what());

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "getMediaSourceDetails failed"
                + ", exception: " + e.what()
        );
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        string errorMessage = e.what();

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
            CMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what());

        throw e;
    }

    try
    {
        string md5FileCheckSum = get<3>(mediaSourceDetails);
        int fileSizeInBytes = get<4>(mediaSourceDetails);

        validateMediaSourceFile(ingestAssetEvent->getFTPMediaSourcePathName(),
                md5FileCheckSum, fileSizeInBytes);
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "validateMediaSourceFile failed"
                + ", exception: " + e.what()
        );
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        string errorMessage = e.what();

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
            CMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what());

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "validateMediaSourceFile failed"
                + ", exception: " + e.what()
        );
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        string errorMessage = e.what();

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
            CMSEngineDBFacade::IngestionStatus::End_ValidationMetadataFailed,
            e.what());

        throw e;
    }

    unsigned long cmsPartitionIndexUsed;
    string cmsAssetPathName;
    try
    {                
        bool partitionIndexToBeCalculated   = true;
        bool deliveryRepositoriesToo        = true;
        cmsAssetPathName = _cmsStorage->moveAssetInCMSRepository(
            ingestAssetEvent->getFTPMediaSourcePathName(),
            ingestAssetEvent->getCustomer()->_directoryName,
            ingestAssetEvent->getMediaSourceFileName(),
            relativePathToBeUsed,
            partitionIndexToBeCalculated,
            &cmsPartitionIndexUsed,
            deliveryRepositoriesToo,
            ingestAssetEvent->getCustomer()->_territories
            );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_cmsStorage->moveAssetInCMSRepository failed");
        
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
                CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_cmsStorage->moveAssetInCMSRepository failed");
        
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
                CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
        
        throw e;
    }

    int64_t videoOrAudioDurationInMilliSeconds = 0;
    int64_t mediaItemKey;
    try
    {
        if (ingestionTypeAndContentType.second == CMSEngineDBFacade::ContentType::Video 
                || ingestionTypeAndContentType.second == CMSEngineDBFacade::ContentType::Audio)
        {
            videoOrAudioDurationInMilliSeconds = 
                EncoderVideoAudioProxy::getVideoOrAudioDurationInMilliSeconds(
                    cmsAssetPathName);
        }
        int sizeInBytes  = 10;
        int imageWidth = 10;
        int imageHeight = 10;

        _logger->info(__FILEREF__ + "_cmsEngineDBFacade->saveIngestedContentMetadata..."
        );

        pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey =
                _cmsEngineDBFacade->saveIngestedContentMetadata (
                ingestAssetEvent->getCustomer(),
                ingestAssetEvent->getIngestionJobKey(),
                metadataRoot,
                relativePathToBeUsed,
                ingestAssetEvent->getMediaSourceFileName(),
                cmsPartitionIndexUsed,
                sizeInBytes,
                videoOrAudioDurationInMilliSeconds,
                imageWidth,
                imageHeight
        );
        
        mediaItemKey = mediaItemKeyAndPhysicalPathKey.first;
        
        _logger->info(__FILEREF__ + "Added a new ingested content"
            + ", mediaItemKey: " + to_string(mediaItemKeyAndPhysicalPathKey.first)
            + ", physicalPathKey: " + to_string(mediaItemKeyAndPhysicalPathKey.second)
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_cmsStorage->moveAssetInCMSRepository failed");

        _logger->info(__FILEREF__ + "Remove file"
            + ", cmsAssetPathName: " + cmsAssetPathName
        );
        FileIO::remove(cmsAssetPathName);
        
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
                CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_cmsStorage->moveAssetInCMSRepository failed");

        _logger->info(__FILEREF__ + "Remove file"
            + ", cmsAssetPathName: " + cmsAssetPathName
        );
        FileIO::remove(cmsAssetPathName);
        
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
                CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
        
        throw e;
    }
    
    // ingest Screenshots if present
    if (ingestionTypeAndContentType.second == CMSEngineDBFacade::ContentType::Video)
    {        
        string field = "ContentIngestion";
        Json::Value contentIngestion = metadataRoot[field]; 

        field = "Screenshots";
        if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        {
            const Json::Value screenshots = contentIngestion[field];

            field = "Title";
            if (!_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            string videoTitle = contentIngestion.get(field, "XXX").asString();

            _logger->info(__FILEREF__ + "Processing the screenshots"
                + ", screenshots.size(): " + to_string(screenshots.size())
            );
            
            for(int screenshotIndex = 0; screenshotIndex < screenshots.size(); screenshotIndex++) 
            {
                try
                {
                    Json::Value screenshot = screenshots[screenshotIndex];

                    field = "TimePositionInSeconds";
                    if (!_cmsEngineDBFacade->isMetadataPresent(screenshot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    double timePositionInSeconds = screenshot.get(field, "XXX").asDouble();

                    field = "EncodingProfilesSet";
                    string encodingProfilesSet;
                    if (!_cmsEngineDBFacade->isMetadataPresent(screenshot, field))
                        encodingProfilesSet = "customerDefault";
                    else
                        encodingProfilesSet = screenshot.get(field, "XXX").asString();

                    field = "SourceImageWidth";
                    if (!_cmsEngineDBFacade->isMetadataPresent(screenshot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    int sourceImageWidth = screenshot.get(field, "XXX").asInt();

                    field = "SourceImageHeight";
                    if (!_cmsEngineDBFacade->isMetadataPresent(screenshot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", Field: " + field;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    int sourceImageHeight = screenshot.get(field, "XXX").asInt();

                    if (videoOrAudioDurationInMilliSeconds < timePositionInSeconds * 1000)
                    {
                        string errorMessage = __FILEREF__ + "Screenshot was not generated because timePositionInSeconds is bigger than the video duration"
                                + ", video mediaItemKey: " + to_string(mediaItemKey)
                                + ", timePositionInSeconds: " + to_string(timePositionInSeconds)
                                + ", videoOrAudioDurationInMilliSeconds: " + to_string(videoOrAudioDurationInMilliSeconds)
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                    
                    string imageFileName;
                    {
                        size_t fileExtensionIndex = ingestAssetEvent->getMediaSourceFileName().find_last_of(".");
                        if (fileExtensionIndex == string::npos)
                            imageFileName.append(ingestAssetEvent->getMediaSourceFileName());
                        else
                            imageFileName.append(ingestAssetEvent->getMediaSourceFileName().substr(0, fileExtensionIndex));
                        imageFileName
                                .append("_")
                                .append(to_string(screenshotIndex + 1))
                                .append(".jpg")
                        ;
                    }

                    {
                        shared_ptr<GenerateImageToIngestEvent>    generateImageToIngestEvent = _multiEventsSet->getEventsFactory()
                                ->getFreeEvent<GenerateImageToIngestEvent>(CMSENGINE_EVENTTYPEIDENTIFIER_GENERATEIMAGETOINGESTEVENT);

                        generateImageToIngestEvent->setSource(CMSENGINEPROCESSORNAME);
                        generateImageToIngestEvent->setDestination(CMSENGINEPROCESSORNAME);
                        generateImageToIngestEvent->setExpirationTimePoint(chrono::system_clock::now());

                        generateImageToIngestEvent->setCmsVideoPathName(cmsAssetPathName);
                        generateImageToIngestEvent->setCustomerFTPRepository(_cmsStorage->getCustomerFTPRepository(ingestAssetEvent->getCustomer()));
                        generateImageToIngestEvent->setImageFileName(imageFileName);
                        generateImageToIngestEvent->setImageTitle(videoTitle + " image #" + to_string(screenshotIndex + 1));

                        generateImageToIngestEvent->setTimePositionInSeconds(timePositionInSeconds);
                        generateImageToIngestEvent->setEncodingProfilesSet(encodingProfilesSet);
                        generateImageToIngestEvent->setSourceImageWidth(sourceImageWidth);
                        generateImageToIngestEvent->setSourceImageHeight(sourceImageHeight);

                        shared_ptr<Event>    event = dynamic_pointer_cast<Event>(generateImageToIngestEvent);
                        _multiEventsSet->addEvent(event);

                        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (GENERATEIMAGETOINGESTEVENT) to generate the screenshot"
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
}

void CMSEngineProcessor::handleGenerateImageToIngestEvent (
    shared_ptr<GenerateImageToIngestEvent> generateImageToIngestEvent)
{
    string imagePathName = generateImageToIngestEvent->getCustomerFTPRepository()
            + "/"
            + generateImageToIngestEvent->getImageFileName()
    ;

    size_t extensionIndex = generateImageToIngestEvent->getImageFileName().find_last_of(".");
    if (extensionIndex == string::npos)
    {
        string errorMessage = __FILEREF__ + "No extension find in the image file name"
                + ", generateImageToIngestEvent->getImageFileName(): " + generateImageToIngestEvent->getImageFileName();
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    string metadataImagePathName = generateImageToIngestEvent->getCustomerFTPRepository()
            + "/"
            + generateImageToIngestEvent->getImageFileName().substr(0, extensionIndex)
            + ".json"
    ;

    EncoderVideoAudioProxy::generateScreenshotToIngest(
            imagePathName,
            generateImageToIngestEvent->getTimePositionInSeconds(),
            generateImageToIngestEvent->getSourceImageWidth(),
            generateImageToIngestEvent->getSourceImageHeight(),
            generateImageToIngestEvent->getCmsVideoPathName()
    );
    
    _logger->info(__FILEREF__ + "Generated Screenshot to ingest"
        + ", imagePathName: " + imagePathName
    );

    generateImageMetadataToIngest(
            metadataImagePathName,
            generateImageToIngestEvent->getImageTitle(),
            generateImageToIngestEvent->getImageFileName(),
            generateImageToIngestEvent->getEncodingProfilesSet()
    );

    _logger->info(__FILEREF__ + "Generated Image metadata to ingest"
        + ", metadataImagePathName: " + metadataImagePathName
    );
}

void CMSEngineProcessor::generateImageMetadataToIngest(
        string metadataImagePathName,
        string title,
        string sourceImageFileName,
        string encodingProfilesSet
)
{
    string imageMetadata = string("")
        + "{"
            + "\"Type\": \"ContentIngestion\","
            + "\"Version\": \"1.0\","
            + "\"ContentIngestion\": {"
                + "\"Title\": \"" + title + "\","
                + "\"Ingester\": \"CMSEngine\","
                + "\"SourceReference\": \"" + sourceImageFileName + "\","
                + "\"ContentType\": \"image\","
                + "\"EncodingProfilesSet\": \"" + encodingProfilesSet + "\""
            + "}"
        + "}"
    ;

    ofstream metadataFileStream(metadataImagePathName, ofstream::trunc);
    metadataFileStream << imageMetadata;
}

void CMSEngineProcessor::handleCheckEncodingEvent ()
{
    vector<shared_ptr<CMSEngineDBFacade::EncodingItem>> encodingItems;
    
    bool resetToBeDone = _firstGetEncodingJob ? true : false;
    
    _cmsEngineDBFacade->getEncodingJobs(resetToBeDone,
        _processorCMS, encodingItems);

    _firstGetEncodingJob = false;

    _pActiveEncodingsManager->addEncodingItems(encodingItems);
}

pair<CMSEngineDBFacade::IngestionType,CMSEngineDBFacade::ContentType> CMSEngineProcessor::validateMetadata(
    Json::Value metadataRoot)
{
    pair<CMSEngineDBFacade::IngestionType,CMSEngineDBFacade::ContentType> ingestionTypeAndContentType;
    
    CMSEngineDBFacade::IngestionType    ingestionType;
    CMSEngineDBFacade::ContentType      contentType;

    string field = "Type";
    if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string type = metadataRoot.get("Type", "XXX").asString();
    if (type == "ContentIngestion")
        ingestionType = CMSEngineDBFacade::IngestionType::ContentIngestion;
    /*
    else if (type == "ContentUpdate")
        ingestionType = CMSEngineDBFacade::IngestionType::ContentUpdate;
    else if (type == "ContentRemove")
        ingestionType = CMSEngineDBFacade::IngestionType::ContentRemove;
    */
    else
    {
        string errorMessage = __FILEREF__ + "Field 'Type' is wrong"
                + ", Type: " + type;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    field = "Version";
    if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
    {
        string errorMessage = __FILEREF__ + "Field is not present or it is wrong"
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    // CMSEngineDBFacade::ContentType contentType = CMSEngineDBFacade::ContentType::Video;
    if (ingestionType == CMSEngineDBFacade::IngestionType::ContentIngestion)
    {
        string field = "ContentIngestion";
        if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value contentIngestion = metadataRoot[field]; 

        contentType = validateContentIngestionMetadata(contentIngestion);
    }
    
    ingestionTypeAndContentType.first = ingestionType;
    ingestionTypeAndContentType.second = contentType;

    
    return ingestionTypeAndContentType;
}

CMSEngineDBFacade::ContentType CMSEngineProcessor::validateContentIngestionMetadata(
    Json::Value contentIngestion)
{
    // see sample in directory samples
    
    CMSEngineDBFacade::ContentType         contentType;
    
    vector<string> contentIngestionMandatoryFields = {
        "Title",
        "SourceReference",
        "ContentType",
        "EncodingProfilesSet"
    };
    for (string contentIngestionField: contentIngestionMandatoryFields)
    {
        if (!_cmsEngineDBFacade->isMetadataPresent(contentIngestion, contentIngestionField))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + contentIngestionField;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string sContentType = contentIngestion.get("ContentType", "XXX").asString();
    if (sContentType != "video" && sContentType != "audio" && sContentType != "image")
    {
        string errorMessage = __FILEREF__ + "Field 'ContentType' is wrong"
                + ", sContentType: " + sContentType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    if (sContentType == "video")
        contentType = CMSEngineDBFacade::ContentType::Video;
    else if (sContentType == "audio")
        contentType = CMSEngineDBFacade::ContentType::Audio;
    else // if (sContentType == "image")
        contentType = CMSEngineDBFacade::ContentType::Image;

    string field;
    if (contentType == CMSEngineDBFacade::ContentType::Video 
            || contentType == CMSEngineDBFacade::ContentType::Audio)
    {
        field = "EncodingPriority";
        if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        {
            string encodingPriority = contentIngestion.get(field, "XXX").asString();
            if (encodingPriority != "low" && encodingPriority != "default" && encodingPriority != "high")
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
    
    // Screenshots
    if (contentType == CMSEngineDBFacade::ContentType::Video)
    {
        field = "Screenshots";
        if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        {
            const Json::Value screenshots = contentIngestion[field];
            
            for(int screenshotIndex = 0; screenshotIndex < screenshots.size(); screenshotIndex++) 
            {
                Json::Value screenshot = screenshots[screenshotIndex];
                
                vector<string> screenshotMandatoryFields = {
                    "TimePositionInSeconds",
                    // "EncodingProfilesSet",
                    "SourceImageWidth",
                    "SourceImageHeight"
                };
                for (string screenshotField: screenshotMandatoryFields)
                {
                    if (!_cmsEngineDBFacade->isMetadataPresent(screenshot, screenshotField))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", Field: " + screenshotField;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
            }
        }
    }
    
    field = "Delivery";
    if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field) && contentIngestion.get(field, "XXX").asString() == "FTP")
    {
        field = "FTP";
        if (!_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value ftp = contentIngestion[field]; 

        vector<string> mandatoryFields = {
            "Hostname",
            "User",
            "Password"
        };
        for (string field: mandatoryFields)
        {
            if (!_cmsEngineDBFacade->isMetadataPresent(ftp, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }

    field = "Notification";
    if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field) && contentIngestion.get(field, "XXX").asString() == "EMail")
    {
        field = "EMail";
        if (!_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value email = contentIngestion["EMail"]; 

        vector<string> mandatoryFields = {
            "Address"
        };
        for (string field: mandatoryFields)
        {
            if (!_cmsEngineDBFacade->isMetadataPresent(email, field))
            {
                string errorMessage = __FILEREF__ + "Field is not present or it is null"
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
    
    return contentType;
}

tuple<bool, string, string, string, int> CMSEngineProcessor::getMediaSourceDetails(
        CMSEngineDBFacade::IngestionType ingestionType,
        Json::Value root)        
{
    tuple<bool, string, string, string, int> mediaSourceDetails;
    
    string mediaSourceFileName;
    
    string field = "ContentIngestion";
    Json::Value contentIngestion = root[field]; 

    if (ingestionType == CMSEngineDBFacade::IngestionType::ContentIngestion)
    {
        string mediaSourceReference;    // URL or local file name
        bool mediaSourceToBeDownload;

        field = "SourceReference";
        mediaSourceReference = contentIngestion.get(field, "XXX").asString();
        
        string httpPrefix ("http");
        string ftpPrefix ("ftp");
        if (!mediaSourceReference.compare(0, httpPrefix.size(), httpPrefix)
                || !mediaSourceReference.compare(0, ftpPrefix.size(), ftpPrefix))
        {
            mediaSourceToBeDownload = true;
            
            // mediaSourceFileName
            {
                smatch m;
                
                regex e (R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)", std::regex::extended);
                
                if (!regex_search(mediaSourceReference, m, e))
                {
                    string errorMessage = __FILEREF__ + "mediaSourceReference URL format is wrong"
                            + ", mediaSourceReference: " + mediaSourceReference;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                
                string path = m[5].str();
                if (path.back() == '/')
                    path.pop_back();
                
                size_t fileNameIndex = path.find_last_of("/");
                if (fileNameIndex == string::npos)
                {
                    string errorMessage = __FILEREF__ + "No fileName find in the mediaSourceReference"
                            + ", mediaSourceReference: " + mediaSourceReference;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }

                mediaSourceFileName = path.substr(fileNameIndex + 1);
            }
        }
        else
        {
            mediaSourceFileName = mediaSourceReference;
            mediaSourceToBeDownload = false;
        }

        get<0>(mediaSourceDetails) = mediaSourceToBeDownload;
        get<1>(mediaSourceDetails) = mediaSourceReference;
    }   
    else
    {
        string errorMessage = __FILEREF__ + "ingestionType is wrong"
                + ", ingestionType: " + to_string(static_cast<int>(ingestionType));
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    get<2>(mediaSourceDetails) = mediaSourceFileName;

    field = "MD5FileCheckSum";
    if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
    {
        MD5         md5;
        char        md5RealDigest [32 + 1];

        string md5FileCheckSum = contentIngestion.get(field, "XXX").asString();
        
        get<3>(mediaSourceDetails) = md5FileCheckSum;
    }
    else
        get<3>(mediaSourceDetails) = string("");

    field = "FileSizeInBytes";
    if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
    {
        int fileSizeInBytes = contentIngestion.get(field, 3).asInt();

        get<4>(mediaSourceDetails) = fileSizeInBytes;
    }
    else
        get<4>(mediaSourceDetails) = -1;

    _logger->info(__FILEREF__ + "media source file to be processed"
        + ", mediaSourceToBeDownload: " + to_string(get<0>(mediaSourceDetails))
        + ", mediaSourceReference: " + get<1>(mediaSourceDetails)
        + ", mediaSourceFileName: " + get<2>(mediaSourceDetails)
        + ", md5FileCheckSum: " + get<3>(mediaSourceDetails)
        + ", fileSizeInBytes: " + to_string(get<4>(mediaSourceDetails))
    );

    
    return mediaSourceDetails;
}

void CMSEngineProcessor::validateMediaSourceFile (string ftpDirectoryMediaSourceFileName,
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

void CMSEngineProcessor::downloadMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Customer> customer,
        string metadataFileName, string ftpDirectoryWorkingMetadataPathName,
        string customerFTPDirectory, string mediaSourceFileName)
{
    bool downloadSuccessful = false;

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
 
        
    for (int attemptIndex = 0; attemptIndex < _maxDownloadAttemptNumber && !dowloadSuccessful; attemptIndex++)
    {
        try 
        {
            string ftpMediaSourcePathName =
                    customerFTPDirectory
                    + "/"
                    + mediaSourceFileName;

            ofstream of(ftpMediaSourcePathName);

            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            // Set the writer callback to enable cURL 
            // to write result in a memory area
            request.setOpt(new curlpp::options::WriteStream(&of));

            // Setting the URL to retrive.
            request.setOpt(new curlpp::options::Url(sourceReferenceURL));

            curlpp::options::ProgressFunction
                progressBar(bind(&CMSEngineProcessor::progressCallback, this, ingestionJobKey, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4));
            request.setOpt(progressBar);

            _logger->info(__FILEREF__ + "Downloading media file"
                + ", sourceReferenceURL: " + sourceReferenceURL
            );
            request.perform();
            
            downloadSuccessful = true;
        }
        catch ( curlpp::LogicError & e ) 
        {
            _logger->error(__FILEREF__ + "Download failed"
                + ", ingestionJobKey: " + ingestionJobKey 
                + ", sourceReferenceURL: " + sourceReferenceURL 
                + ", exception: " + e.what()
            );

            string ftpDirectoryErrorEntryPathName =
                _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, metadataFileName);

            _cmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                    CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());

            downloadSuccessful = false;
        }
        catch ( curlpp::RuntimeError & e ) 
        {
            _logger->error(__FILEREF__ + "Download failed"
                + ", ingestionJobKey: " + ingestionJobKey 
                + ", sourceReferenceURL: " + sourceReferenceURL 
                + ", exception: " + e.what()
            );

            string ftpDirectoryErrorEntryPathName =
                _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, metadataFileName);

            _cmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                    CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());

            downloadSuccessful = false;
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Download failed"
                + ", ingestionJobKey: " + ingestionJobKey 
                + ", sourceReferenceURL: " + sourceReferenceURL 
                + ", exception: " + e.what()
            );

            string ftpDirectoryErrorEntryPathName =
                _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, metadataFileName);

            _cmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                    CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());

            downloadSuccessful = false;
        }
    }

    try 
    {
        shared_ptr<IngestAssetEvent>    ingestAssetEvent = _multiEventsSet->getEventsFactory()
                ->getFreeEvent<IngestAssetEvent>(CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT);

        ingestAssetEvent->setSource(CMSENGINEPROCESSORNAME);
        ingestAssetEvent->setDestination(CMSENGINEPROCESSORNAME);
        ingestAssetEvent->setExpirationTimePoint(chrono::system_clock::now());

        ingestAssetEvent->setIngestionJobKey(ingestionJobKey);
        ingestAssetEvent->setCustomer(customer);

        ingestAssetEvent->setMetadataFileName(metadataFileName);
        ingestAssetEvent->setFTPWorkingMetadataPathName(ftpDirectoryWorkingMetadataPathName);
        ingestAssetEvent->setFTPMediaSourcePathName(ftpMediaSourcePathName);
        ingestAssetEvent->setMediaSourceFileName(mediaSourceFileName);

        shared_ptr<Event>    event = dynamic_pointer_cast<Event>(ingestAssetEvent);
        _multiEventsSet->addEvent(event);

        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
            + ", mediaSourceFileName: " + mediaSourceFileName
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second));
    }
    catch (runtime_error & e) 
    {
        _logger->error(__FILEREF__ + "sending INGESTASSETEVENT failed"
            + ", ingestionJobKey: " + ingestionJobKey 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, metadataFileName);

        _cmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "sending INGESTASSETEVENT failed"
            + ", ingestionJobKey: " + ingestionJobKey 
            + ", sourceReferenceURL: " + sourceReferenceURL 
            + ", exception: " + e.what()
        );

        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, metadataFileName);

        _cmsEngineDBFacade->updateIngestionJob (ingestionJobKey, 
                CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
    }
}

double CMSEngineProcessor::progressCallback(int64_t ingestionJobKey,
    double dltotal, double dlnow,
    double ultotal, double ulnow)
{

    _logger->info(__FILEREF__ + "Download still running"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", dltotal: " + to_string(dltotal)
        + ", dlnow: " + to_string(dlnow)
        + ", ultotal: " + to_string(ultotal)
        + ", ulnow: " + to_string(ulnow)
    );

    double progress = (dlnow / dltotal) * 100;
    float percent = floorf(progress * 100) / 100;

    _logger->info(__FILEREF__ + "Download still running"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", percent: " + to_string(percent)
        + ", dltotal: " + to_string(dltotal)
        + ", dlnow: " + to_string(dlnow)
        + ", ultotal: " + to_string(ultotal)
        + ", ulnow: " + to_string(ulnow)
    );
}