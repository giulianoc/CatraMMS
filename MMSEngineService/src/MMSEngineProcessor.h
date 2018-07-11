
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
#include "MultiLocalAssetIngestionEvent.h"
#include "Validator.h"
#include "json/json.h"

#define MMSENGINEPROCESSORNAME    "MMSEngineProcessor"


class MMSEngineProcessor
{
public:
    struct CurlDownloadData {
        int         currentChunkNumber;
        string      workspaceIngestionBinaryPathName;
        ofstream    mediaSourceFileStream;
        size_t      currentTotalSize;
        size_t      maxChunkFileSize;
    };
    
    MMSEngineProcessor(
            int processorIdentifier,
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            ActiveEncodingsManager* pActiveEncodingsManager,
            Json::Value configuration);
    
    ~MMSEngineProcessor();
    
    void operator()();
    
private:
    int                                 _processorIdentifier;
    shared_ptr<spdlog::logger>          _logger;
    Json::Value                         _configuration;
    shared_ptr<MultiEventsSet>          _multiEventsSet;
    shared_ptr<MMSEngineDBFacade>       _mmsEngineDBFacade;
    shared_ptr<MMSStorage>              _mmsStorage;
    ActiveEncodingsManager*             _pActiveEncodingsManager;
    
    string                  _processorMMS;

    int                     _maxDownloadAttemptNumber;
    int                     _progressUpdatePeriodInSeconds;
    int                     _secondsWaitingAmongDownloadingAttempt;
    
    int                     _stagingRetentionInDays;

    int                     _maxIngestionJobsPerEvent;
    int                     _dependencyExpirationInHours;
    size_t                  _downloadChunkSizeInMegaBytes;
    
    string                  _emailProtocol;
    string                  _emailServer;
    int                     _emailPort;
    string                  _emailUserName;
    string                  _emailPassword;
    string                  _emailFrom;
    
    
    // void sendEmail(string to, string subject, vector<string>& emailBody);

    void handleCheckIngestionEvent();

    void handleLocalAssetIngestionEvent (
        shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent);

    void handleMultiLocalAssetIngestionEvent (
        shared_ptr<MultiLocalAssetIngestionEvent> multiLocalAssetIngestionEvent);

    void handleCheckEncodingEvent ();

    void handleContentRetentionEvent ();

    void removeContent(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);
    
    /*
    string generateImageMetadataToIngest(
        int64_t ingestionJobKey,
        bool mjpeg,
        string fileFormat,
        Json::Value parametersRoot
    );
    */

    string generateMediaMetadataToIngest(
        int64_t ingestionJobKey,
        // bool video,
        string fileFormat,
        Json::Value parametersRoot);

    void manageEncodeTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);

    void manageOverlayImageOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);
    
    void manageOverlayTextOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);

    void generateAndIngestFrames(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);

    void manageGenerateFramesTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);

    void fillGenerateFramesParameters(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies,
        
        int& periodInSeconds, double& startTimeInSeconds,
        int& maxFramesNumber, string& videoFilter,
        bool& mjpeg, int& imageWidth, int& imageHeight,
        string& imageFileName,
        int64_t& sourcePhysicalPathKey, string& sourcePhysicalPath);

    void generateAndIngestSlideshow(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);

    void generateAndIngestConcatenation(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);

    void generateAndIngestCutMedia(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);

    void manageEmailNotification(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);

    tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> getMediaSourceDetails(
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value root);

    void validateMediaSourceFile (int64_t ingestionJobKey,
        string ftpDirectoryMediaSourceFileName,
        string md5FileCheckSum, int fileSizeInBytes);

    bool isMetadataPresent(Json::Value root, string field);

    void downloadMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace);
    void moveMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace);
    void copyMediaSourceFile(string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace);

    int progressCallback(
        int64_t ingestionJobKey,
        chrono::system_clock::time_point& lastTimeProgressUpdate, 
        double& lastPercentageUpdated, bool& downloadingStoppedByUser,
        double dltotal, double dlnow,
        double ultotal, double ulnow);

    // static size_t emailPayloadFeed(void *ptr, size_t size, size_t nmemb, void *userp);
} ;

#endif

