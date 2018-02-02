
#include <fstream>
#include "CMSEngineProcessor.h"
#include "CheckIngestionTimes.h"

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
    
    _ulMaxIngestionsNumberPerCustomerEachIngestionPeriod       = 2;
    _ulJsonToBeProcessedAfterSeconds                           = 10;
    _ulRetentionPeriodInDays                                   = 10;
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

		handleCheckIngestionEvent ();

                _multiEventsSet->getEventsFactory()->releaseEvent<Event>(event);

            }
            break;
            case CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT:	// 2
            {
                _logger->info("Received CMSENGINE_EVENTTYPEIDENTIFIER_INGESTASSETEVENT");

                shared_ptr<IngestAssetEvent>    ingestAssetEvent = dynamic_pointer_cast<IngestAssetEvent>(event);

		handleIngestAssetEvent (ingestAssetEvent);

                _multiEventsSet->getEventsFactory()->releaseEvent<IngestAssetEvent>(ingestAssetEvent);
            }
            break;
            default:
                throw invalid_argument(string("Event type identifier not managed")
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
                    // check if the file has ".json" as extension.
                    // We do not accept also the ".json" file (without name)
                    string jsonExtension(".json");
                    if (directoryEntry.length() < 6 ||
                            !equal(jsonExtension.rbegin(), jsonExtension.rend(), directoryEntry.rbegin()))
                            continue;

                    string srcPathName(customerFTPDirectory);
                    srcPathName.append("/").append(directoryEntry);

                    chrono::system_clock::time_point lastModificationTime = 
                        FileIO:: getFileTime (srcPathName);

                    if (chrono::duration_cast<chrono::hours>(chrono::system_clock::now() - lastModificationTime) 
                            >= chrono::hours(_ulRetentionPeriodInDays * 24))
                    {
                        _logger->info(string("Remove obsolete FTP file")
                            + ", srcPathName: " + srcPathName
                                );

                        FileIO::remove (srcPathName);

                        continue;
                    }
                    else if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastModificationTime) 
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

                    string ftpDirectoryWorkingEntryPathName =
                            _cmsStorage->moveFTPRepositoryEntryToWorkingArea(customer, directoryEntry);

                    CMSEngineDBFacade::IngestionType ingestionType;
                    try
                    {
                        ingestionType = validateMetadata(ftpDirectoryWorkingEntryPathName);
                    }
                    catch(exception e)
                    {
                        _logger->error(string("validateMetadata failed")
                                + ", exception: " + e.what()
                        );
                        string ftpDirectoryErrorEntryPathName =
                            _cmsStorage->moveFTPRepositoryWorkingEntryToErrorArea(customer, directoryEntry);
                    
                        throw e;
                    }
                    
                    int64_t ingestionJobKey;
                    try
                    {
                        ingestionJobKey = _cmsEngineDBFacade->addIngestionJob (customer->_customerKey, directoryEntry, ingestionType);
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

                            ingestAssetEvent->setFTPDirectoryWorkingEntryPathName(ftpDirectoryWorkingEntryPathName);
                            ingestAssetEvent->setIngestionJobKey(ingestionJobKey);
                            ingestAssetEvent->setCustomer(customer);
                            ingestAssetEvent->setFileName(directoryEntry);

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

void CMSEngineProcessor::handleIngestAssetEvent(shared_ptr<IngestAssetEvent> ingestAssetEvent)

{
}

CMSEngineDBFacade::IngestionType CMSEngineProcessor::validateMetadata(string ftpDirectoryWorkingEntryPathName)
{
    /*
    {
        "Type": "Encoding",         // mandatory
        "Version": "1.0",           // mandatory
        "Encoding": {
            "SourceFileName": "aa.mp4", // mandatory
            "ContentType": "video",     // mandatory: "video" or "audio" or "image"
            "MD5FileCheckSum": "....",  // optional
            "FileSizeInBytes": "...",   // optional

            "EncodingProfilesSet": "",  // optional: "default" or "defaultCustomer" or <custom name>

            "Delivery": "FTP",      // optional: "FTP"
            "FTP": {                // mandatory only if "Delivery" is "FTP"
                "Hostname": "aaa",  // mandatory only if "Delivery" is "FTP": hostname or IP address
                "Port": "21",       // optional
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
    
    CMSEngineDBFacade::IngestionType ingestionType;
    Json::Value root;
    
    try
    {                
        std::ifstream ingestAssetJson(ftpDirectoryWorkingEntryPathName, std::ifstream::binary);
        ingestAssetJson >> root;
        
        string type = root.get("Type", "XXX").asString();
        if (type == "IngestionType")
            ingestionType = CMSEngineDBFacade::IngestionType::Encoding;
        else if (type == "ContentIngestion")
            ingestionType = CMSEngineDBFacade::IngestionType::ContentIngestion;
        else if (type == "ContentUpdate")
            ingestionType = CMSEngineDBFacade::IngestionType::ContentUpdate;
        else if (type == "ContentRemove")
            ingestionType = CMSEngineDBFacade::IngestionType::ContentRemove;
        else
        {
            string errorMessage = string("Field 'Type' is wrong");
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        
        string field = "Version";
        if (!isMetadataPresent(root, field))
        {
            string errorMessage = string("Field is not present")
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }


        // servlet saveMetaData
        //  Type/Version    Encoding/1.0
        // switch(Type) e call relativo metodo
        // IngestAssetThread::manageEncoding1_0
        //      controllo MD5
        //      controllo fileSize
    }
    catch(exception e)
    {
        _logger->error(string("validateMetadata failed")
                + ", exception: " + e.what()
        );
        
        throw e.what();
    }
    
    return ingestionType;
}

bool CMSEngineProcessor::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field))
        return true;
    else
        return false;
}