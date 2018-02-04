
#ifndef CMSEngineProcessor_h
#define CMSEngineProcessor_h

#include <string>
// #define SPDLOG_DEBUG_ON
// #define SPDLOG_TRACE_ON
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "CMSEngineDBFacade.h"
#include "CMSStorage.h"
#include "IngestAssetEvent.h"
#include "json/json.h"

#define CMSENGINEPROCESSORNAME    "CMSEngineProcessor"


class CMSEngineProcessor
{
private:
    shared_ptr<spdlog::logger>          _logger;
    shared_ptr<MultiEventsSet>          _multiEventsSet;
    shared_ptr<CMSEngineDBFacade>       _cmsEngineDBFacade;
    shared_ptr<CMSStorage>              _cmsStorage;
    
    unsigned long           _ulIngestionLastCustomerIndex;
    unsigned long           _ulMaxIngestionsNumberPerCustomerEachIngestionPeriod;
    unsigned long           _ulJsonToBeProcessedAfterSeconds;
    unsigned long           _ulRetentionPeriodInDays;

    void handleCheckIngestionEvent();

    void handleIngestAssetEvent (shared_ptr<IngestAssetEvent> ingestAssetEvent);

    CMSEngineDBFacade::IngestionType validateMetadata(Json::Value root);

    void validateContentIngestionMetadata(Json::Value encoding);

    pair<string, string> validateMediaSourceFile(
        string customerFTPDirectory,
        CMSEngineDBFacade::IngestionType ingestionType,
        Json::Value root);

    bool isMetadataPresent(Json::Value root, string field);

public:
    CMSEngineProcessor(
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
            shared_ptr<CMSStorage> cmsStorage);
    
    ~CMSEngineProcessor();
    
    void operator()();
} ;

#endif

