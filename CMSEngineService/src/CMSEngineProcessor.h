
#ifndef CMSEngineProcessor_h
#define CMSEngineProcessor_h

#include <string>
// #define SPDLOG_DEBUG_ON
// #define SPDLOG_TRACE_ON
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "CMSEngineDBFacade.h"
#include "CMSStorage.h"
#include "ActiveEncodingsManager.h"
#include "IngestAssetEvent.h"
#include "GenerateImageToIngestEvent.h"
#include "json/json.h"

#define CMSENGINEPROCESSORNAME    "CMSEngineProcessor"


class CMSEngineProcessor
{
private:
    shared_ptr<spdlog::logger>          _logger;
    shared_ptr<MultiEventsSet>          _multiEventsSet;
    shared_ptr<CMSEngineDBFacade>       _cmsEngineDBFacade;
    shared_ptr<CMSStorage>              _cmsStorage;
    ActiveEncodingsManager*             _pActiveEncodingsManager;
    
    unsigned long           _ulIngestionLastCustomerIndex;
    bool                    _firstGetEncodingJob;
    string                  _processorCMS;
    int                     _maxDownloadAttemptNumber;
    
    unsigned long           _ulMaxIngestionsNumberPerCustomerEachIngestionPeriod;
    unsigned long           _ulJsonToBeProcessedAfterSeconds;
    unsigned long           _ulRetentionPeriodInDays;

    void handleCheckIngestionEvent();

    void handleIngestAssetEvent (shared_ptr<IngestAssetEvent> ingestAssetEvent);

    void handleCheckEncodingEvent ();

    void handleGenerateImageToIngestEvent (
        shared_ptr<GenerateImageToIngestEvent> generateImageToIngestEvent);

    void generateImageMetadataToIngest(
        string metadataImagePathName,
        string title,
        string sourceImageFileName,
        string encodingProfilesSet
    );

    pair<CMSEngineDBFacade::IngestionType,CMSEngineDBFacade::ContentType>  validateMetadata(Json::Value root);

    CMSEngineDBFacade::ContentType validateContentIngestionMetadata(Json::Value encoding);

    tuple<bool, string, string, string, int> getMediaSourceDetails(
        CMSEngineDBFacade::IngestionType ingestionType,
        Json::Value root);

    void validateMediaSourceFile (string ftpDirectoryMediaSourceFileName,
        string md5FileCheckSum, int fileSizeInBytes);

    bool isMetadataPresent(Json::Value root, string field);

    void downloadMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Customer> customer,
        string metadataFileName, string ftpDirectoryWorkingMetadataPathName,
        string customerFTPDirectory, string mediaSourceFileName);

    double progressCallback(int64_t ingestionJobKey,
        double dltotal, double dlnow,
        double ultotal, double ulnow);

public:
    CMSEngineProcessor(
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<CMSStorage> cmsStorage,
            ActiveEncodingsManager* pActiveEncodingsManager);
    
    ~CMSEngineProcessor();
    
    void operator()();
} ;

#endif

