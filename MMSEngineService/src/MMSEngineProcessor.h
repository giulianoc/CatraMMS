
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
    
    struct CurlUploadFacebookData {
        ifstream    mediaSourceFileStream;
        
        bool        bodyFirstPartSent;
        string      bodyFirstPart;

        bool        bodyLastPartSent;
        string      bodyLastPart;
        
        int64_t     startOffset;
        int64_t     endOffset;
        
        int64_t     currentOffset;
    };

    struct CurlUploadYouTubeData {
        ifstream    mediaSourceFileStream;
        
        int64_t     lastByteSent;
        int64_t     fileSizeInBytes;        
    };

    MMSEngineProcessor(
            int processorIdentifier,
            shared_ptr<spdlog::logger> logger, 
            shared_ptr<MultiEventsSet> multiEventsSet,
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            shared_ptr<long> processorsThreadsNumber,
            ActiveEncodingsManager* pActiveEncodingsManager,
            Json::Value configuration);
    
    ~MMSEngineProcessor();
    
    void operator()();
    
private:
    int                                 _processorIdentifier;
    int                                 _processorThreads;
    int                                 _maxAdditionalProcessorThreads;
    shared_ptr<spdlog::logger>          _logger;
    Json::Value                         _configuration;
    shared_ptr<MultiEventsSet>          _multiEventsSet;
    shared_ptr<MMSEngineDBFacade>       _mmsEngineDBFacade;
    shared_ptr<MMSStorage>              _mmsStorage;
    shared_ptr<long>                    _processorsThreadsNumber;
    ActiveEncodingsManager*             _pActiveEncodingsManager;
    
    string                  _processorMMS;

    int                     _maxDownloadAttemptNumber;
    int                     _progressUpdatePeriodInSeconds;
    int                     _secondsWaitingAmongDownloadingAttempt;
    
	int						_maxSecondsToWaitCheckIngestionLock;
	int						_maxSecondsToWaitCheckEncodingJobLock;
	int						_maxSecondsToWaitMainAndBackupLiveChunkLock;

    // int                     _stagingRetentionInDays;

    int                     _maxIngestionJobsPerEvent;
    int                     _maxEncodingJobsPerEvent;
    int                     _dependencyExpirationInHours;
    size_t                  _downloadChunkSizeInMegaBytes;
    
    string                  _emailProtocol;
    string                  _emailServer;
    int                     _emailPort;
    string                  _emailUserName;
    string                  _emailPassword;
    string                  _emailFrom;
    
    string                  _facebookGraphAPIProtocol;
    string                  _facebookGraphAPIHostName;
    int                     _facebookGraphAPIPort;
    string                  _facebookGraphAPIVersion;
    long                    _facebookGraphAPITimeoutInSeconds;

    string                  _youTubeDataAPIProtocol;
    string                  _youTubeDataAPIHostName;
    int                     _youTubeDataAPIPort;
    string                  _youTubeDataAPIRefreshTokenURI;
    string                  _youTubeDataAPIUploadVideoURI;
    long                    _youTubeDataAPITimeoutInSeconds;
    string                  _youTubeDataAPIClientId;
    string                  _youTubeDataAPIClientSecret;

    bool                    _localCopyTaskEnabled;
    
    // void sendEmail(string to, string subject, vector<string>& emailBody);

    void handleCheckIngestionEvent();

    void handleLocalAssetIngestionEvent (
        shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent);

    void handleMultiLocalAssetIngestionEvent (
        shared_ptr<MultiLocalAssetIngestionEvent> multiLocalAssetIngestionEvent);

    void handleCheckEncodingEvent ();

    void handleContentRetentionEventThread (shared_ptr<long> processorsThreadsNumber);

	void handleMainAndBackupOfRunnungLiveRecordingHA (shared_ptr<long> processorsThreadsNumber);

    void removeContentTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);
    
    void ftpDeliveryContentTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void postOnFacebookTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void postOnYouTubeTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void httpCallbackTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void userHttpCallbackThread(
        shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey, string httpProtocol, string httpHostName,
        int httpPort, string httpURI, string httpURLParameters,
        string httpMethod, long callbackTimeoutInSeconds,
        Json::Value userHeadersRoot, 
        Json::Value callbackMedatada);

    void localCopyContentTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void copyContentThread(
        shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey, string mmsAssetPathName, 
        string localPath, string localFileName);

	void manageFaceRecognitionMediaTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

	void manageFaceIdentificationMediaTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

	void manageLiveRecorder(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot);

    void extractTracksContentThread(
        shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>> dependencies);

    void changeFileFormatThread(
        shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>> dependencies);

    string generateMediaMetadataToIngest(
        int64_t ingestionJobKey,
        string fileFormat,
        string title,
		int64_t imageOfVideoMediaItemKey,
        Json::Value parametersRoot);

    void manageEncodeTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

	void manageVideoSpeedTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>&
			dependencies);

    void manageOverlayImageOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);
    
    void manageOverlayTextOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void generateAndIngestFramesTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void manageGenerateFramesTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    int64_t fillGenerateFramesParameters(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies,
        
        int& periodInSeconds, double& startTimeInSeconds,
        int& maxFramesNumber, string& videoFilter,
        bool& mjpeg, int& imageWidth, int& imageHeight,
        int64_t& sourcePhysicalPathKey, string& sourcePhysicalPath,
        int64_t& durationInMilliSeconds);

    void manageSlideShowTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    /*
    void generateAndIngestSlideshow(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);
    */
    void generateAndIngestConcatenationTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void generateAndIngestCutMediaTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void manageEmailNotificationTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    void manageMediaCrossReferenceTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType>>& dependencies);

    tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int> getMediaSourceDetails(
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value root);

    void validateMediaSourceFile (int64_t ingestionJobKey,
        string mediaSourcePathName,
        string md5FileCheckSum, int fileSizeInBytes);

    bool isMetadataPresent(Json::Value root, string field);

    void downloadMediaSourceFileThread(
        shared_ptr<long> processorsThreadsNumber,
        string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace);
    void moveMediaSourceFileThread(shared_ptr<long> processorsThreadsNumber,
        string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace);
    void copyMediaSourceFileThread(shared_ptr<long> processorsThreadsNumber,
        string sourceReferenceURL,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace);

    int progressDownloadCallback(
        int64_t ingestionJobKey,
        chrono::system_clock::time_point& lastTimeProgressUpdate, 
        double& lastPercentageUpdated, bool& downloadingStoppedByUser,
        double dltotal, double dlnow,
        double ultotal, double ulnow);

    int progressUploadCallback(
        int64_t ingestionJobKey,
        chrono::system_clock::time_point& lastTimeProgressUpdate, 
        double& lastPercentageUpdated, bool& uploadingStoppedByUser,
        double dltotal, double dlnow,
        double ultotal, double ulnow);

    void ftpUploadMediaSourceThread(
        shared_ptr<long> processorsThreadsNumber,
        string mmsAssetPathName, string fileName, int64_t sizeInBytes,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        string ftpServer, int ftpPort, string ftpUserName, string ftpPassword, 
        string ftpRemoteDirectory, string ftpRemoteFileName);

    void postVideoOnFacebookThread(
        shared_ptr<long> processorsThreadsNumber,
        string mmsAssetPathName, int64_t sizeInBytes,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        string facebookNodeId, string facebookConfigurationLabel
        );

    void postVideoOnYouTubeThread(
        shared_ptr<long> processorsThreadsNumber,
        string mmsAssetPathName, int64_t sizeInBytes,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        string youTubeConfigurationLabel, string youTubeTitle,
        string youTubeDescription, Json::Value youTubeTags,
        int youTubeCategoryId, string youTubePrivacy);

	string getYouTubeAccessTokenByConfigurationLabel(
		shared_ptr<Workspace> workspace, string youTubeConfigurationLabel);
} ;

#endif

