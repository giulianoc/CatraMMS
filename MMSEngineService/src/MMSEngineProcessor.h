
#ifndef MMSEngineProcessor_h
#define MMSEngineProcessor_h

#include <string>
#include <vector>
#include <deque>
// #define SPDLOG_DEBUG_ON
// #define SPDLOG_TRACE_ON
#include "spdlog/spdlog.h"
#include "catralibraries/MultiEventsSet.h"
#include "catralibraries/GetCpuUsage.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "ThreadsStatistic.h"
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
        string      destBinaryPathName;
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
			shared_ptr<ThreadsStatistic> mmsThreadsStatistic,
            ActiveEncodingsManager* pActiveEncodingsManager,
			mutex* cpuUsageMutex,
			deque<int>* cpuUsage,
            Json::Value configuration);
    
    ~MMSEngineProcessor();
    
    void operator()();

	void cpuUsageThread();
	void stopCPUUsageThread();

    
private:
    int                                 _processorIdentifier;
    int                                 _processorThreads;
    int									_cpuUsageThreshold;
    shared_ptr<spdlog::logger>          _logger;
    Json::Value                         _configuration;
    shared_ptr<MultiEventsSet>          _multiEventsSet;
    shared_ptr<MMSEngineDBFacade>       _mmsEngineDBFacade;
    shared_ptr<MMSStorage>              _mmsStorage;
    shared_ptr<long>                    _processorsThreadsNumber;
    ActiveEncodingsManager*             _pActiveEncodingsManager;

	shared_ptr<ThreadsStatistic>		_mmsThreadsStatistic;

	GetCpuUsage_t				_getCpuUsage;
	mutex*						_cpuUsageMutex;
	deque<int>*					_cpuUsage;
	bool						_cpuUsageThreadShutdown;

    string                  _processorMMS;

    int                     _maxDownloadAttemptNumber;
    int                     _progressUpdatePeriodInSeconds;
    int                     _secondsWaitingAmongDownloadingAttempt;
    
	int						_maxSecondsToWaitCheckIngestionLock;
	int						_maxSecondsToWaitCheckEncodingJobLock;
	// int						_maxSecondsToWaitMainAndBackupLiveChunkLock;

    // int                     _stagingRetentionInDays;

    int                     _maxIngestionJobsPerEvent;
    int                     _maxEncodingJobsPerEvent;
    int						_maxEventManagementTimeInSeconds;
    int                     _dependencyExpirationInHours;
    size_t                  _downloadChunkSizeInMegaBytes;
	int						_timeBeforeToPrepareResourcesInMinutes;
    
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
	string					_youTubeDataAPILiveBroadcastURI;
	string					_youTubeDataAPILiveStreamURI;
	string					_youTubeDataAPILiveBroadcastBindURI;
    long                    _youTubeDataAPITimeoutInSeconds;
    string                  _youTubeDataAPIClientId;
    string                  _youTubeDataAPIClientSecret;

	string					_deliveryProtocol;
	string					_deliveryHost;

    bool                    _localCopyTaskEnabled;

	string					_mmsAPIProtocol;
	string					_mmsAPIHostname;
	int						_mmsAPIPort;
	string					_mmsAPIIngestionURI;
	string					_mmsAPIVersion;
    int						_mmsAPITimeoutInSeconds;

	int						_waitingNFSSync_maxMillisecondsToWait;
	int						_waitingNFSSync_milliSecondsWaitingBetweenChecks;


	Json::Value getReviewedOutputsRoot(
		Json::Value outputsRoot, shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey, bool encodingProfileMandatory);

	int getMaxAdditionalProcessorThreads();

	bool isMaintenanceMode();
	bool isProcessorShutdown();

    void handleCheckIngestionEvent();

    void handleLocalAssetIngestionEventThread (
		shared_ptr<long> processorsThreadsNumber,
        LocalAssetIngestionEvent localAssetIngestionEvent);

    void handleMultiLocalAssetIngestionEventThread (
		shared_ptr<long> processorsThreadsNumber,
        MultiLocalAssetIngestionEvent multiLocalAssetIngestionEvent);

    void handleCheckEncodingEvent ();

    void handleContentRetentionEventThread (shared_ptr<long> processorsThreadsNumber);

    void handleDBDataRetentionEventThread ();

	void handleCheckRefreshPartitionFreeSizeEventThread();

	// void handleMainAndBackupOfRunnungLiveRecordingHA (shared_ptr<long> processorsThreadsNumber);

    void removeContentThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);
    
    void ftpDeliveryContentThread(
        shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    void postOnFacebookThread(
        shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    void postOnYouTubeThread(
        shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    void httpCallbackThread(
		shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    void userHttpCallback(
        int64_t ingestionJobKey, string httpProtocol, string httpHostName,
        int httpPort, string httpURI, string httpURLParameters,
        string httpMethod, long callbackTimeoutInSeconds,
        Json::Value userHeadersRoot, 
        Json::Value callbackMedatada, int maxRetries);

    void localCopyContentThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    void copyContent(
        int64_t ingestionJobKey, string mmsAssetPathName, 
        string localPath, string localFileName);

	void manageFaceRecognitionMediaTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

	void manageFaceIdentificationMediaTask(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

	void manageLiveRecorder(
        int64_t ingestionJobKey, string ingestionJobLabel,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot);

	void manageLiveProxy(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot);

	void manageVODProxy(
		int64_t ingestionJobKey,
		MMSEngineDBFacade::IngestionStatus ingestionStatus,
		shared_ptr<Workspace> workspace,
		Json::Value parametersRoot,
		vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies
	);

	void manageCountdown(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
		string ingestionDate,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
		vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

	void manageLiveGrid(
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionStatus ingestionStatus,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot);

	void liveCutThread_streamSegmenter(
		shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot);
	void liveCutThread_hlsSegmenter(
		shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey,
		string ingestionJobLabel,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot);

	void youTubeLiveBroadcastThread(
		shared_ptr<long> processorsThreadsNumber,
		int64_t ingestionJobKey,
		string ingestionJobLabel,
		shared_ptr<Workspace> workspace,
		Json::Value parametersRoot);

    void extractTracksContentThread(
        shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    void changeFileFormatThread(
        shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    string generateMediaMetadataToIngest(
        int64_t ingestionJobKey,
        string fileFormat,
        string title,
		int64_t imageOfVideoMediaItemKey,
		int64_t cutOfVideoMediaItemKey, int64_t cutOfAudioMediaItemKey, double startTimeInSeconds, double endTimeInSeconds,
        Json::Value parametersRoot);

    void manageEncodeTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

	void manageGroupOfTasks(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot);

	void manageVideoSpeedTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

	void managePictureInPictureTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

	void manageIntroOutroOverlayTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

    void manageOverlayImageOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);
    
    void manageOverlayTextOnVideoTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

    void generateAndIngestFramesThread(
        shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    void manageGenerateFramesTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

    void fillGenerateFramesParameters(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        MMSEngineDBFacade::IngestionType ingestionType,
        Json::Value parametersRoot,
		int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey,

        int& periodInSeconds, double& startTimeInSeconds,
        int& maxFramesNumber, string& videoFilter,
        bool& mjpeg, int& imageWidth, int& imageHeight,
        int64_t& durationInMilliSeconds);

    void manageSlideShowTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

    /*
    void generateAndIngestSlideshow(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<pair<int64_t,Validator::DependencyType>>& dependencies);
    */
    void generateAndIngestConcatenationThread(
        shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    void generateAndIngestCutMediaThread(
        shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

    void emailNotificationThread(
		shared_ptr<long> processorsThreadsNumber,
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>
			dependencies);

	void checkStreamingThread(
		shared_ptr<long> processorsThreadsNumber,
		int64_t ingestionJobKey,
		shared_ptr<Workspace> workspace,
		Json::Value parametersRoot);

    void manageMediaCrossReferenceTask(
        int64_t ingestionJobKey,
        shared_ptr<Workspace> workspace,
        Json::Value parametersRoot,
        vector<tuple<int64_t,MMSEngineDBFacade::ContentType,Validator::DependencyType, bool>>&
			dependencies);

	tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int, bool>
		getMediaSourceDetails(
			int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
			MMSEngineDBFacade::IngestionType ingestionType, Json::Value parametersRoot);

    void validateMediaSourceFile (int64_t ingestionJobKey,
        string mediaSourcePathName, string mediaFileFormat,
        string md5FileCheckSum, int fileSizeInBytes);

    void downloadMediaSourceFileThread(
        shared_ptr<long> processorsThreadsNumber,
        string sourceReferenceURL, bool regenerateTimestamps, int m3u8TarGzOrM3u8Streaming,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace);
    void moveMediaSourceFileThread(shared_ptr<long> processorsThreadsNumber,
        string sourceReferenceURL, int m3u8TarGzOrM3u8Streaming,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace);
    void copyMediaSourceFileThread(shared_ptr<long> processorsThreadsNumber,
        string sourceReferenceURL, int m3u8TarGzOrM3u8Streaming,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace);

	void manageTarFileInCaseOfIngestionOfSegments(
		int64_t ingestionJobKey,
		string tarBinaryPathName, string workspaceIngestionRepository,
		string sourcePathName
		);

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

    void ftpUploadMediaSource(
        string mmsAssetPathName, string fileName, int64_t sizeInBytes,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
		int64_t mediaItemKey, int64_t physicalPathKey,
        string ftpServer, int ftpPort, string ftpUserName, string ftpPassword, 
        string ftpRemoteDirectory, string ftpRemoteFileName);

    void postVideoOnFacebook(
        string mmsAssetPathName, int64_t sizeInBytes,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        string facebookNodeId, string facebookConfigurationLabel
        );

    void postVideoOnYouTube(
        string mmsAssetPathName, int64_t sizeInBytes,
        int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
        string youTubeConfigurationLabel, string youTubeTitle,
        string youTubeDescription, Json::Value youTubeTags,
        int youTubeCategoryId, string youTubePrivacy);

	string getYouTubeAccessTokenByConfigurationLabel(
		int64_t ingestionJobKey,
		shared_ptr<Workspace> workspace, string youTubeConfigurationLabel);
} ;

#endif

