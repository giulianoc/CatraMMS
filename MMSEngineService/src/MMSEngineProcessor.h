
#ifndef MMSEngineProcessor_h
#define MMSEngineProcessor_h

#include <string>
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
private:
    shared_ptr<spdlog::logger>          _logger;
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
    
    void handleCheckIngestionEvent();

    void handleLocalAssetIngestionEvent (
        shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent);

    void handleCheckEncodingEvent ();

    void handleGenerateImageToIngestEvent (
        shared_ptr<GenerateImageToIngestEvent> generateImageToIngestEvent);

    void generateImageMetadataToIngest(
        string metadataImagePathName,
        string title,
        string sourceImageFileName,
        string encodingProfilesSet
    );

    pair<MMSEngineDBFacade::IngestionType,MMSEngineDBFacade::ContentType>  validateMetadata(Json::Value root);

    MMSEngineDBFacade::ContentType validateContentIngestionMetadata(Json::Value encoding);

    tuple<bool, bool, string, string, string, int> getMediaSourceDetails(
        shared_ptr<Customer> customer,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value root);

    void validateMediaSourceFile (string ftpDirectoryMediaSourceFileName,
        string md5FileCheckSum, int fileSizeInBytes);

    bool isMetadataPresent(Json::Value root, string field);

    void downloadMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Customer> customer,
        string metadataFileName, string mediaSourceFileName);

    int progressCallback(
        int64_t ingestionJobKey,
        chrono::system_clock::time_point& lastTimeProgressUpdate, 
        int& lastPercentageUpdated, bool& downloadingStoppedByUser,
        double dltotal, double dlnow,
        double ultotal, double ulnow);

public:
    MMSEngineProcessor(
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            ActiveEncodingsManager* pActiveEncodingsManager);
    
    ~MMSEngineProcessor();
    
    void operator()();
} ;

#endif

