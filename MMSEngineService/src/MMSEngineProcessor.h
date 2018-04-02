
#ifndef MMSEngineProcessor_h
#define MMSEngineProcessor_h

#include <string>
#include <vector>
// #define SPDLOG_DEBUG_ON
// #define SPDLOG_TRACE_ON
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "ActiveEncodingsManager.h"
#include "LocalAssetIngestionEvent.h"
#include "GenerateImageToIngestEvent.h"
#include "json/json.h"

#define MMSENGINEPROCESSORNAME    "MMSEngineProcessor"


class MMSEngineProcessor
{
public:
    MMSEngineProcessor(
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            ActiveEncodingsManager* pActiveEncodingsManager,
            Json::Value configuration);
    
    ~MMSEngineProcessor();
    
    void operator()();
    
private:
    shared_ptr<spdlog::logger>          _logger;
    Json::Value                         _configuration;
    shared_ptr<MultiEventsSet>          _multiEventsSet;
    shared_ptr<MMSEngineDBFacade>       _mmsEngineDBFacade;
    shared_ptr<MMSStorage>              _mmsStorage;
    ActiveEncodingsManager*             _pActiveEncodingsManager;
    
    bool                    _firstGetEncodingJob;
    string                  _processorMMS;

    int                     _maxDownloadAttemptNumber;
    int                     _progressUpdatePeriodInSeconds;
    int                     _secondsWaitingAmongDownloadingAttempt;

    int                     _maxIngestionJobsPerEvent;
    // int                     _maxIngestionJobsWithDependencyToCheckPerEvent;
    int                     _dependencyExpirationInHours;
    
    void handleCheckIngestionEvent();

    void handleLocalAssetIngestionEvent (
        shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent);

    void handleCheckEncodingEvent ();

    string generateImageMetadataToIngest(
        int64_t ingestionJobKey,
        bool mjpeg,
        string sourceFileName,
        Json::Value screenshotRoot
    );

    void generateAndIngestFrame(
        int64_t ingestionJobKey,
        shared_ptr<Customer> customer,
        Json::Value metadataRoot,
        vector<int64_t>& dependencies
    );

    tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> getMediaSourceDetails(
        int64_t ingestionJobKey, shared_ptr<Customer> customer,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value root);

    void validateMediaSourceFile (int64_t ingestionJobKey,
        string ftpDirectoryMediaSourceFileName,
        string md5FileCheckSum, int fileSizeInBytes);

    bool isMetadataPresent(Json::Value root, string field);

    void downloadMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Customer> customer);
    void moveMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Customer> customer);
    void copyMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Customer> customer);

    int progressCallback(
        int64_t ingestionJobKey,
        chrono::system_clock::time_point& lastTimeProgressUpdate, 
        double& lastPercentageUpdated, bool& downloadingStoppedByUser,
        double dltotal, double dlnow,
        double ultotal, double ulnow);

} ;

#endif

