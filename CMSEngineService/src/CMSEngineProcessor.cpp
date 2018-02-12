
#include <fstream>
#include <sstream>
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
                    string relativePathToBeUsed;
                    string mediaSourceFileName;
                    string ftpDirectoryMediaSourceFileName;
                    {    
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

                        try
                        {
                            pair<string, string> mediaSource = validateMediaSourceFile(
                                    customerFTPDirectory, ingestionTypeAndContentType.first, metadataRoot);
                            mediaSourceFileName = mediaSource.first;
                            ftpDirectoryMediaSourceFileName = mediaSource.second;
                        }
                        catch(runtime_error e)
                        {
                            _logger->error(__FILEREF__ + "validateMediaSourceFile failed"
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
                            _logger->error(__FILEREF__ + "validateMediaSourceFile failed"
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
                    }
                    
                    try
                    {
                        relativePathToBeUsed = _cmsEngineDBFacade->checkCustomerMaxIngestionNumber (customer->_customerKey);
                    }
                    catch(exception e)
                    {
                        _logger->error(__FILEREF__ + "checkCustomerMaxIngestionNumber failed"
                                + ", exception: " + e.what()
                        );
                        string ftpDirectoryErrorEntryPathName =
                            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);
                    
                        string errorMessage = e.what();

                        _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                directoryEntry, metadataFileContent, ingestionTypeAndContentType.first, 
                                CMSEngineDBFacade::IngestionStatus::End_CustomerReachedHisMaxIngestionNumber, 
                                errorMessage);

                        throw e;
                    }
                    
                    int64_t ingestionJobKey;
                    try
                    {
                        string errorMessage = "";
                        ingestionJobKey = _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                directoryEntry, metadataFileContent, ingestionTypeAndContentType.first, 
                                CMSEngineDBFacade::IngestionStatus::DataReceivedAndValidated, errorMessage);
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
                        {
                            shared_ptr<IngestAssetEvent>    ingestAssetEvent = _multiEventsSet->getEventsFactory()
                                    ->getFreeEvent<IngestAssetEvent>(CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT);

                            ingestAssetEvent->setSource(CMSENGINEPROCESSORNAME);
                            ingestAssetEvent->setDestination(CMSENGINEPROCESSORNAME);
                            ingestAssetEvent->setExpirationTimePoint(chrono::system_clock::now());

                            ingestAssetEvent->setFTPDirectoryWorkingMetadataPathName(ftpDirectoryWorkingMetadataPathName);
                            ingestAssetEvent->setMetadataFileName(directoryEntry);

                            ingestAssetEvent->setContentType(ingestionTypeAndContentType.second);
                            ingestAssetEvent->setFTPDirectoryMediaSourceFileName(ftpDirectoryMediaSourceFileName);
                            ingestAssetEvent->setMediaSourceFileName(mediaSourceFileName);

                            ingestAssetEvent->setIngestionJobKey(ingestionJobKey);
                            ingestAssetEvent->setCustomer(customer);
                            ingestAssetEvent->setRelativePath(relativePathToBeUsed);
                            ingestAssetEvent->setMetadataRoot(metadataRoot);

                            shared_ptr<Event>    event = dynamic_pointer_cast<Event>(ingestAssetEvent);
                            _multiEventsSet->addEvent(event);

                            _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE"
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
    unsigned long cmsPartitionIndexUsed;
    string cmsAssetPathName;
    try
    {                
        bool partitionIndexToBeCalculated   = true;
        bool deliveryRepositoriesToo        = true;
        cmsAssetPathName = _cmsStorage->moveAssetInCMSRepository(
            ingestAssetEvent->getFTPDirectoryMediaSourceFileName(),
            ingestAssetEvent->getCustomer()->_directoryName,
            ingestAssetEvent->getMediaSourceFileName(),
            ingestAssetEvent->getRelativePath(),
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
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_cmsStorage->moveAssetInCMSRepository failed");
        
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
                CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
    }

    int64_t videoOrAudioDurationInMilliSeconds = 0;
    int64_t mediaItemKey;
    try
    {
        if (ingestAssetEvent->getContentType() == CMSEngineDBFacade::ContentType::Video 
                || ingestAssetEvent->getContentType() == CMSEngineDBFacade::ContentType::Audio)
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
                ingestAssetEvent->getMetadataRoot(),
                ingestAssetEvent->getRelativePath(),
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
    }
    
    // ingest Screenshots if present
    if (ingestAssetEvent->getContentType() == CMSEngineDBFacade::ContentType::Video)
    {
        Json::Value metadataRoot = ingestAssetEvent->getMetadataRoot();
        
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

                        _logger->debug(__FILEREF__ + "addEvent: EVENT_TYPE"
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

    generateImageMetadataToIngest(
            metadataImagePathName,
            generateImageToIngestEvent->getImageTitle(),
            generateImageToIngestEvent->getImageFileName(),
            generateImageToIngestEvent->getEncodingProfilesSet()
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
                + "\"SourceFileName\": \"" + sourceImageFileName + "\","
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
    
    vector<string> mandatoryFields = {
        "Title",
        "SourceFileName",
        "ContentType",
        "EncodingProfilesSet"
    };
    for (string field: mandatoryFields)
    {
        if (!_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
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
                for (string field: mandatoryFields)
                {
                    if (!_cmsEngineDBFacade->isMetadataPresent(screenshot, field))
                    {
                        string errorMessage = __FILEREF__ + "Field is not present or it is null"
                                + ", Field: " + field;
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

pair<string, string> CMSEngineProcessor::validateMediaSourceFile(
        string customerFTPDirectory,
        CMSEngineDBFacade::IngestionType ingestionType,
        Json::Value root)        
{
    pair<string, string> mediaSource;
    
 
    string mediaSourceFileName;
    
    Json::Value contentIngestion = root["ContentIngestion"]; 

    if (ingestionType == CMSEngineDBFacade::IngestionType::ContentIngestion)
    {
        mediaSourceFileName = contentIngestion.get("SourceFileName", "XXX").asString();
        
        mediaSource.first = mediaSourceFileName;
    }   
    else
    {
        string errorMessage = __FILEREF__ + "ingestionType is wrong"
                + ", ingestionType: " + to_string(static_cast<int>(ingestionType));
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    string ftpDirectoryMediaSourceFileName (customerFTPDirectory);
    ftpDirectoryMediaSourceFileName
        .append("/")
        .append(mediaSourceFileName);
    
    mediaSource.second = ftpDirectoryMediaSourceFileName;

    _logger->info(__FILEREF__ + "media source file to be processed"
        + ", ftpDirectoryMediaSourceFileName: " + ftpDirectoryMediaSourceFileName
    );

    if (!FileIO::fileExisting(ftpDirectoryMediaSourceFileName))
    {
        string errorMessage = __FILEREF__ + "Media Source file does not exist (it was not uploaded yet)"
                + ", mediaSourceFileName: " + mediaSourceFileName;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string field = "MD5FileCheckSum";
    if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
    {
        MD5         md5;
        char        md5RealDigest [32 + 1];

        string md5FileCheckSum = contentIngestion.get(field, "XXX").asString();

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
    
    field = "FileSizeInBytes";
    if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
    {
        int fileSizeInBytes = contentIngestion.get(field, 3).asInt();

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
    
    return mediaSource;
}
