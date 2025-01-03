
#ifndef MMSEngineProcessor_h
#define MMSEngineProcessor_h

#include "ActiveEncodingsManager.h"
#include "LocalAssetIngestionEvent.h"
#include "MMSDeliveryAuthorization.h"
#include "MultiLocalAssetIngestionEvent.h"
#include "ThreadsStatistic.h"
#include "Validator.h"
#include "catralibraries/GetCpuUsage.h"
#include "catralibraries/MultiEventsSet.h"
#include <fstream>

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

#define MMSENGINEPROCESSORNAME "MMSEngineProcessor"

class MMSEngineProcessor
{
  public:
	struct CurlDownloadData
	{
		int64_t ingestionJobKey;
		int currentChunkNumber;
		string destBinaryPathName;
		ofstream mediaSourceFileStream;
		size_t currentTotalSize;
		size_t maxChunkFileSize;
	};

	struct CurlUploadFacebookData
	{
		ifstream mediaSourceFileStream;

		bool bodyFirstPartSent;
		string bodyFirstPart;

		bool bodyLastPartSent;
		string bodyLastPart;

		int64_t startOffset;
		int64_t endOffset;

		int64_t currentOffset;
	};

	struct CurlUploadYouTubeData
	{
		ifstream mediaSourceFileStream;

		int64_t lastByteSent;
		int64_t fileSizeInBytes;
	};

	MMSEngineProcessor(
		int processorIdentifier, shared_ptr<spdlog::logger> logger, shared_ptr<MultiEventsSet> multiEventsSet,
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, shared_ptr<MMSStorage> mmsStorage, shared_ptr<long> processorsThreadsNumber,
		shared_ptr<ThreadsStatistic> mmsThreadsStatistic, shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization,
		ActiveEncodingsManager *pActiveEncodingsManager, mutex *cpuUsageMutex, deque<int> *cpuUsage, json configurationRoot
	);

	~MMSEngineProcessor();

	void operator()();

	void cpuUsageThread();
	void stopCPUUsageThread();

  private:
	int _processorIdentifier;
	int _processorThreads;
	int _cpuUsageThreshold;
	shared_ptr<spdlog::logger> _logger;
	json _configurationRoot;
	shared_ptr<MultiEventsSet> _multiEventsSet;
	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	shared_ptr<MMSStorage> _mmsStorage;
	shared_ptr<long> _processorsThreadsNumber;
	ActiveEncodingsManager *_pActiveEncodingsManager;

	shared_ptr<ThreadsStatistic> _mmsThreadsStatistic;

	shared_ptr<MMSDeliveryAuthorization> _mmsDeliveryAuthorization;

	GetCpuUsage_t _getCpuUsage;
	mutex *_cpuUsageMutex;
	deque<int> *_cpuUsage;
	bool _cpuUsageThreadShutdown;

	string _processorMMS;

	int _maxDownloadAttemptNumber;
	int _progressUpdatePeriodInSeconds;
	int _secondsWaitingAmongDownloadingAttempt;

	int _maxSecondsToWaitCheckIngestionLock;
	int _maxSecondsToWaitCheckEncodingJobLock;
	// int						_maxSecondsToWaitMainAndBackupLiveChunkLock;

	// int                     _stagingRetentionInDays;

	int _maxIngestionJobsPerEvent;
	int _maxEncodingJobsPerEvent;
	int _maxEventManagementTimeInSeconds;
	int _dependencyExpirationInHours;
	size_t _downloadChunkSizeInMegaBytes;
	int _timeBeforeToPrepareResourcesInMinutes;

	string _facebookGraphAPIProtocol;
	string _facebookGraphAPIHostName;
	string _facebookGraphAPIVideoHostName;
	int _facebookGraphAPIPort;
	string _facebookGraphAPIVersion;
	long _facebookGraphAPITimeoutInSeconds;
	string _facebookGraphAPIClientId;
	string _facebookGraphAPIClientSecret;
	string _facebookGraphAPIRedirectURL;
	string _facebookGraphAPIAccessTokenURI;
	string _facebookGraphAPILiveVideosURI;

	string _youTubeDataAPIProtocol;
	string _youTubeDataAPIHostName;
	int _youTubeDataAPIPort;
	string _youTubeDataAPIRefreshTokenURI;
	string _youTubeDataAPIUploadVideoURI;
	string _youTubeDataAPILiveBroadcastURI;
	string _youTubeDataAPILiveStreamURI;
	string _youTubeDataAPILiveBroadcastBindURI;
	long _youTubeDataAPITimeoutInSeconds;
	long _youTubeDataAPITimeoutInSecondsForUploadVideo;
	string _youTubeDataAPIClientId;
	string _youTubeDataAPIClientSecret;

	string _deliveryProtocol;
	string _deliveryHost;

	bool _localCopyTaskEnabled;

	string _mmsWorkflowIngestionURL;
	string _mmsIngestionURL;
	string _mmsBinaryIngestionURL;
	int _mmsAPITimeoutInSeconds;
	string _mmsAPIVODDeliveryURI;

	int _waitingNFSSync_maxMillisecondsToWait;
	int _waitingNFSSync_milliSecondsWaitingBetweenChecks;

	string _liveRecorderVirtualVODImageLabel;

	string _emailProviderURL;
	string _emailUserName;
	string _emailPassword;
	string _emailCcsCommaSeparated;

	json getReviewedOutputsRoot(json outputsRoot, shared_ptr<Workspace> workspace, int64_t ingestionJobKey, bool encodingProfileMandatory);

	json getReviewedFiltersRoot(json filtersRoot, shared_ptr<Workspace> workspace, int64_t ingestionJobKey);

	int getMaxAdditionalProcessorThreads();

	bool isMaintenanceMode();
	bool isProcessorShutdown();

	void handleCheckIngestionEvent();

	void handleLocalAssetIngestionEventThread(shared_ptr<long> processorsThreadsNumber, LocalAssetIngestionEvent localAssetIngestionEvent);
	void handleLocalAssetIngestionEvent(shared_ptr<long> processorsThreadsNumber, LocalAssetIngestionEvent localAssetIngestionEvent);

	void
	handleMultiLocalAssetIngestionEventThread(shared_ptr<long> processorsThreadsNumber, MultiLocalAssetIngestionEvent multiLocalAssetIngestionEvent);

	void handleCheckEncodingEvent();

	void handleContentRetentionEventThread(shared_ptr<long> processorsThreadsNumber);

	void handleDBDataRetentionEventThread();

	void handleGEOInfoEventThread();

	void handleCheckRefreshPartitionFreeSizeEventThread();

	// void handleMainAndBackupOfRunnungLiveRecordingHA (shared_ptr<long> processorsThreadsNumber);

	void removeContentThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void ftpDeliveryContentThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void postOnFacebookThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void postOnYouTubeThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void httpCallbackThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void userHttpCallback(
		int64_t ingestionJobKey, string httpProtocol, string httpHostName, int httpPort, string httpURI, string httpURLParameters, bool formData,
		string httpMethod, long callbackTimeoutInSeconds, json userHeadersRoot, string &data, string userName, string password, int maxRetries
	);

	void localCopyContentThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void copyContent(int64_t ingestionJobKey, string mmsAssetPathName, string localPath, string localFileName);

	void manageFaceRecognitionMediaTask(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageFaceIdentificationMediaTask(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageLiveRecorder(
		int64_t ingestionJobKey, string ingestionJobLabel, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace,
		json parametersRoot
	);

	void manageLiveProxy(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot
	);

	void manageVODProxy(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageCountdown(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, /* string ingestionDate, */ shared_ptr<Workspace> workspace,
		json parametersRoot, vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void
	manageLiveGrid(int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, shared_ptr<Workspace> workspace, json parametersRoot);

	void manageLiveCutThread_streamSegmenter(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot
	);
	void manageLiveCutThread_hlsSegmenter(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string ingestionJobLabel, shared_ptr<Workspace> workspace,
		json parametersRoot
	);

	void youTubeLiveBroadcastThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string ingestionJobLabel, shared_ptr<Workspace> workspace,
		json parametersRoot
	);

	void facebookLiveBroadcastThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, string ingestionJobLabel, shared_ptr<Workspace> workspace,
		json parametersRoot
	);

	void extractTracksContentThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void changeFileFormatThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	string generateMediaMetadataToIngest(
		int64_t ingestionJobKey, string fileFormat, string title, int64_t imageOfVideoMediaItemKey, int64_t cutOfVideoMediaItemKey,
		int64_t cutOfAudioMediaItemKey, double startTimeInSeconds, double endTimeInSeconds, json parametersRoot
	);

	void manageEncodeTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageGroupOfTasks(int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot);

	void manageVideoSpeedTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageAddSilentAudioTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void managePictureInPictureTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageIntroOutroOverlayTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageOverlayImageOnVideoTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageOverlayTextOnVideoTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void generateAndIngestFrameThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace,
		MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void manageGenerateFramesTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void fillGenerateFramesParameters(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey, MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot,
		int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey,

		int &periodInSeconds, double &startTimeInSeconds, int &maxFramesNumber, string &videoFilter, bool &mjpeg, int &imageWidth, int &imageHeight,
		int64_t &durationInMilliSeconds
	);

	void manageSlideShowTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	/*
	void generateAndIngestSlideshow(
		int64_t ingestionJobKey,
		shared_ptr<Workspace> workspace,
		json parametersRoot,
		vector<pair<int64_t,Validator::DependencyType>>& dependencies);
	*/
	void manageConcatThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void manageCutMediaThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void emailNotificationThread(
		shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void
	checkStreamingThread(shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot);

	void manageMediaCrossReferenceTask(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, json parametersRoot,
		vector<tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	tuple<MMSEngineDBFacade::IngestionStatus, string, string, string, int, bool> getMediaSourceDetails(
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace, MMSEngineDBFacade::IngestionType ingestionType, json parametersRoot
	);

	void
	validateMediaSourceFile(int64_t ingestionJobKey, string mediaSourcePathName, string mediaFileFormat, string md5FileCheckSum, int fileSizeInBytes);

	void downloadMediaSourceFileThread(
		shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL, bool regenerateTimestamps, int m3u8TarGzOrStreaming,
		int64_t ingestionJobKey, shared_ptr<Workspace> workspace
	);
	void moveMediaSourceFileThread(
		shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL, int m3u8TarGzOrStreaming, int64_t ingestionJobKey,
		shared_ptr<Workspace> workspace
	);
	void copyMediaSourceFileThread(
		shared_ptr<long> processorsThreadsNumber, string sourceReferenceURL, int m3u8TarGzOrStreaming, int64_t ingestionJobKey,
		shared_ptr<Workspace> workspace
	);

	// void manageTarFileInCaseOfIngestionOfSegments(
	// 	int64_t ingestionJobKey,
	// 	string tarBinaryPathName, string workspaceIngestionRepository,
	// 	string sourcePathName
	// 	);

	/*
	int progressDownloadCallback(
		int64_t ingestionJobKey, chrono::system_clock::time_point &lastTimeProgressUpdate, double &lastPercentageUpdated,
		bool &downloadingStoppedByUser, double dltotal, double dlnow, double ultotal, double ulnow
	);
	*/

	/*
	int progressUploadCallback(
		int64_t ingestionJobKey, chrono::system_clock::time_point &lastTimeProgressUpdate, double &lastPercentageUpdated,
		bool &uploadingStoppedByUser, double dltotal, double dlnow, double ultotal, double ulnow
	);
	*/

	void ftpUploadMediaSource(
		string mmsAssetPathName, string fileName, int64_t sizeInBytes, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, int64_t mediaItemKey,
		int64_t physicalPathKey, string ftpServer, int ftpPort, string ftpUserName, string ftpPassword, string ftpRemoteDirectory,
		string ftpRemoteFileName
	);

	void postVideoOnFacebook(
		string mmsAssetPathName, int64_t sizeInBytes, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string facebookConfigurationLabel,
		string facebookDestination, string facebookNodeId
	);

	void postVideoOnYouTube(
		string mmsAssetPathName, int64_t sizeInBytes, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string youTubeConfigurationLabel,
		string youTubeTitle, string youTubeDescription, json youTubeTags, int youTubeCategoryId, string youTubePrivacy, bool youTubeMadeForKids
	);

	string getYouTubeAccessTokenByConfigurationLabel(int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string youTubeConfigurationLabel);

	string getFacebookPageToken(int64_t ingestionJobKey, shared_ptr<Workspace> workspace, string facebookConfigurationLabel, string facebookPageId);

	tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool> processDependencyInfo(
		shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
		tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> keyAndDependencyType
	);

	string getEncodedFileExtensionByEncodingProfile(json encodingProfileDetailsRoot);
};

#endif
