
#include <fstream>
#include <sstream>
#include "CMSEngineProcessor.h"
#include "CheckIngestionTimes.h"
#include "catralibraries/md5.h"

CMSEngineProcessor::CMSEngineProcessor(
        shared_ptr<spdlog::logger> logger, 
        shared_ptr<MultiEventsSet> multiEventsSet,
        shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
        shared_ptr<CMSStorage> cmsStorage
)
{
    _logger             = logger;
    _multiEventsSet     = multiEventsSet;
    _cmsEngineDBFacade  = cmsEngineDBFacade;
    _cmsStorage      = cmsStorage;
    
    _ulIngestionLastCustomerIndex   = 0;
    
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

    // SPDLOG_DEBUG(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    // SPDLOG_TRACE(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1, 3.23);
    _logger->info("CMSEngineProcessor thread started");

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
            case CMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION:	// 1
            {
                _logger->info("Received CMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION");

                try
                {
        		handleCheckIngestionEvent ();
                }
                catch(exception e)
                {
                    _logger->error(string("handleCheckIngestionEvent failed")
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<Event>(event);

            }
            break;
            case CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT:	// 2
            {
                _logger->info("Received CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT");

                shared_ptr<IngestAssetEvent>    ingestAssetEvent = dynamic_pointer_cast<IngestAssetEvent>(event);

                try
                {
                    handleIngestAssetEvent (ingestAssetEvent);
                }
                catch(exception e)
                {
                    _logger->error(string("handleIngestAssetEvent failed")
                        + ", exception: " + e.what()
                    );
                }

                _multiEventsSet->getEventsFactory()->releaseEvent<IngestAssetEvent>(ingestAssetEvent);
            }
            break;
            default:
                throw runtime_error(string("Event type identifier not managed")
                        + to_string(event->getEventKey().first));
        }
    }

    _logger->info("CMSEngineProcessor thread terminated");
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
            _logger->info(string("Looking for ingestions")
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
                    _logger->error("FileIO::readDirectory failed");

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
                        _logger->info(string("Remove obsolete FTP file")
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

                    _logger->info(string("json to be processed")
                        + ", directoryEntry: " + directoryEntry
                    );

                    string ftpDirectoryWorkingMetadataPathName =
                            _cmsStorage->moveFTPRepositoryEntryToWorkingArea(customer, directoryEntry);

                    string      metadataFileContent;
                    CMSEngineDBFacade::IngestionType ingestionType;
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

                            ingestionType = validateMetadata(metadataRoot);
                        }
                        catch(runtime_error e)
                        {
                            _logger->error(string("validateMetadata failed")
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
                            pair<string, string> mediaSource = validateMediaSourceFile(customerFTPDirectory, ingestionType, metadataRoot);
                            mediaSourceFileName = mediaSource.first;
                            ftpDirectoryMediaSourceFileName = mediaSource.second;
                        }
                        catch(runtime_error e)
                        {
                            _logger->error(string("validateMediaSourceFile failed")
                                    + ", exception: " + e.what()
                            );
                            string ftpDirectoryErrorEntryPathName =
                                _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);

                            string errorMessage = e.what();

                            _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                    directoryEntry, metadataFileContent, ingestionType, 
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
                        _logger->error(string("checkCustomerMaxIngestionNumber failed")
                                + ", exception: " + e.what()
                        );
                        string ftpDirectoryErrorEntryPathName =
                            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);
                    
                        string errorMessage = e.what();

                        _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                directoryEntry, metadataFileContent, ingestionType, 
                                CMSEngineDBFacade::IngestionStatus::End_CustomerReachedHisMaxIngestionNumber, 
                                errorMessage);

                        throw e;
                    }
                    
                    int64_t ingestionJobKey;
                    try
                    {
                        string errorMessage = "";
                        ingestionJobKey = _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, 
                                directoryEntry, metadataFileContent, ingestionType, 
                                CMSEngineDBFacade::IngestionStatus::DataReceivedAndValidated, errorMessage);
                    }
                    catch(exception e)
                    {
                        _logger->error(string("_cmsEngineDBFacade->addIngestionJob failed")
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

                            ingestAssetEvent->setFTPDirectoryMediaSourceFileName(ftpDirectoryMediaSourceFileName);
                            ingestAssetEvent->setMediaSourceFileName(mediaSourceFileName);

                            ingestAssetEvent->setIngestionJobKey(ingestionJobKey);
                            ingestAssetEvent->setCustomer(customer);
                            ingestAssetEvent->setRelativePath(relativePathToBeUsed);
                            ingestAssetEvent->setMetadataRoot(metadataRoot);

                            shared_ptr<Event>    event = dynamic_pointer_cast<Event>(ingestAssetEvent);
                            _multiEventsSet->addEvent(event);

                            _logger->info("addEvent: EVENT_TYPE ({}, {})", event->getEventKey().first, event->getEventKey().second);
                        }
                    }
                    catch(exception e)
                    {
                        _logger->error(string("add IngestAssetEvent failed")
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
                    _logger->error(string("Exception managing the FTP entry")
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
            _logger->error(string("Error processing the Customer FTP")
                + ", customer->_name: " + customer->_name
                    );
        }
    }

    if (itCustomers == customers.end())
        _ulIngestionLastCustomerIndex	= 0;

}

void CMSEngineProcessor::handleIngestAssetEvent (shared_ptr<IngestAssetEvent> ingestAssetEvent)
{

    /*
    if (getContentDurationInMilliSeconds (
        _cCustomer. _bName. str (), (const char *) bFTPAssetPathName,
        (const char *) (pmitMediaItemInfo -> _bContentType),
        pContentDuration, _ptSystemTracer) != errNoError)
    */

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
        _logger->error(string("_cmsStorage->moveAssetInCMSRepository failed"));
        
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
                CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
    }

    try
    {
        int sizeInBytes  = 10;
        int64_t videoOrAudioDurationInMilliSeconds = 10;
        int imageWidth = 10;
        int imageHeight = 10;

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
    }
    catch(runtime_error e)
    {
        _logger->error(string("_cmsStorage->moveAssetInCMSRepository failed"));

        _logger->info(string("Remove file")
            + ", cmsAssetPathName: " + cmsAssetPathName
        );
        FileIO::remove(cmsAssetPathName);
        
        string ftpDirectoryErrorEntryPathName =
            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(
                ingestAssetEvent->getCustomer(), ingestAssetEvent->getMetadataFileName());

        _cmsEngineDBFacade->updateIngestionJob (ingestAssetEvent->getIngestionJobKey(),
                CMSEngineDBFacade::IngestionStatus::End_IngestionFailure, e.what());
    }
    
        /*
        se screenshot del video:
        generate image e ingest

        move metadata file name (o remove?)
         */

}

CMSEngineDBFacade::IngestionType CMSEngineProcessor::validateMetadata(
    Json::Value metadataRoot)
{
    
    CMSEngineDBFacade::IngestionType ingestionType;

    string field = "Type";
    if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
    {
        string errorMessage = string("Field is not present or it is null")
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
        string errorMessage = string("Field 'Type' is wrong")
                + ", Type: " + type;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    field = "Version";
    if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
    {
        string errorMessage = string("Field is not present or it is wrong")
                + ", Field: " + field;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    if (ingestionType == CMSEngineDBFacade::IngestionType::ContentIngestion)
    {
        string field = "ContentIngestion";
        if (!_cmsEngineDBFacade->isMetadataPresent(metadataRoot, field))
        {
            string errorMessage = string("Field is not present or it is null")
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        Json::Value contentIngestion = metadataRoot[field]; 

        validateContentIngestionMetadata(contentIngestion);
    }
    // else ...        
    
    return ingestionType;
}

void CMSEngineProcessor::validateContentIngestionMetadata(Json::Value contentIngestion)
{
    /*
    {
        "Type": "ContentIngestion",         // mandatory
        "Version": "1.0",           // mandatory
        "ContentIngestion": {
            "Title": "aaaa",            // mandatory
            "SubTitle": "aaaa",            // optional
            "Ingester": "aaaa",         // optional
            "Keywords": "aaa",          // optional
            "Description": "aaa",       // optional

            "SourceFileName": "aa.mp4", // mandatory
            "ContentType": "video",     // mandatory: "video" or "audio" or "image"
            "LogicalType": "Advertising",     // optional
            "MD5FileCheckSum": null,  // optional
            "FileSizeInBytes": null,   // optional

            "EncodingProfilesSet": "systemDefault",  // mandatory: "systemDefault" or "customerDefault" or <custom name>
            "EncodingPriority": "low",               // optional: "low", "default", "high"

            "ContentProviderName": "default",    // optional
            
            "Territories": {
                "default": {
                    "startPublishing": "NOW",
                    "endPublishing": "FOREVER"
                }
            },

            "Delivery": "FTP",      // optional: "FTP"
            "FTP": {                // mandatory only if "Delivery" is "FTP"
                "Hostname": "aaa",  // mandatory only if "Delivery" is "FTP": hostname or IP address
                "Port": null,       // optional
                "User": "aaa",      // mandatory only if "Delivery" is "FTP"
                "Password": "bbb"   // mandatory only if "Delivery" is "FTP"
            },

            "Notification": "EMail",      // optional: "EMail
            "EMail": {              // mandatory only if "Notification" is "EMail"
                "Address": "giulanoc@catrasoftware.it"  // mandatory only if "Notification" is "EMail"
            }
        }
    }
    */
    
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
            string errorMessage = string("Field is not present or it is null")
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }

    string contentType = contentIngestion.get("ContentType", "XXX").asString();
    if (contentType != "video" && contentType != "audio" && contentType != "image")
    {
        string errorMessage = string("Field 'ContentType' is wrong")
                + ", ContentType: " + contentType;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }

    string field = "EncodingPriority";
    if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
    {
        string encodingPriority = contentIngestion.get(field, "XXX").asString();
        if (encodingPriority != "low" && encodingPriority != "default" && encodingPriority != "high")
        {
            string errorMessage = string("Field 'EncodingPriority' is wrong")
                    + ", EncodingPriority: " + encodingPriority;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
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
    
    field = "Delivery";
    if (_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field) && contentIngestion.get(field, "XXX").asString() == "FTP")
    {
        field = "FTP";
        if (!_cmsEngineDBFacade->isMetadataPresent(contentIngestion, field))
        {
            string errorMessage = string("Field is not present or it is null")
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
                string errorMessage = string("Field is not present or it is null")
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
            string errorMessage = string("Field is not present or it is null")
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
                string errorMessage = string("Field is not present or it is null")
                        + ", Field: " + field;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
    }
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
        string errorMessage = string("ingestionType is wrong")
                + ", ingestionType: " + to_string(static_cast<int>(ingestionType));
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    
    string ftpDirectoryMediaSourceFileName (customerFTPDirectory);
    ftpDirectoryMediaSourceFileName
        .append("/")
        .append(mediaSourceFileName);
    
    mediaSource.second = ftpDirectoryMediaSourceFileName;

    _logger->info(string("media source file to be processed")
        + ", ftpDirectoryMediaSourceFileName: " + ftpDirectoryMediaSourceFileName
    );

    if (!FileIO::fileExisting(ftpDirectoryMediaSourceFileName))
    {
        string errorMessage = string("Media Source file does not exist (it was not uploaded yet)")
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
            string errorMessage = string("MD5 check failed")
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
            string errorMessage = string("FileSize check failed")
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
