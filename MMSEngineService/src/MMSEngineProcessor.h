
#pragma once

#include "ActiveEncodingsManager.h"
#include "GetCpuUsage.h"
#include "LocalAssetIngestionEvent.h"
#include "MMSDeliveryAuthorization.h"
#include "MultiEventsSet.h"
#include "MultiLocalAssetIngestionEvent.h"
#include "ThreadsStatistic.h"
#include "Validator.h"
#include <fstream>

#define MMSENGINEPROCESSORNAME "MMSEngineProcessor"

class MMSEngineProcessor
{
  public:
	struct CurlDownloadData
	{
		int64_t ingestionJobKey;
		int currentChunkNumber;
		std::string destBinaryPathName;
		std::ofstream mediaSourceFileStream;
		size_t currentTotalSize;
		size_t maxChunkFileSize;
	};

	struct CurlUploadFacebookData
	{
		std::ifstream mediaSourceFileStream;

		bool bodyFirstPartSent;
		std::string bodyFirstPart;

		bool bodyLastPartSent;
		std::string bodyLastPart;

		int64_t startOffset;
		int64_t endOffset;

		int64_t currentOffset;
	};

	struct CurlUploadYouTubeData
	{
		std::ifstream mediaSourceFileStream;

		int64_t lastByteSent;
		int64_t fileSizeInBytes;
	};

	MMSEngineProcessor(
		int processorIdentifier, std::shared_ptr<spdlog::logger> logger, std::shared_ptr<MultiEventsSet> multiEventsSet,
		std::shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, std::shared_ptr<MMSStorage> mmsStorage, std::shared_ptr<long> processorsThreadsNumber,
		std::shared_ptr<ThreadsStatistic> mmsThreadsStatistic, std::shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization,
		ActiveEncodingsManager *pActiveEncodingsManager, std::mutex *cpuUsageMutex, std::deque<int> *cpuUsage, nlohmann::json configurationRoot
	);

	~MMSEngineProcessor();

	void operator()();

	void cpuUsageThread();
	void stopCPUUsageThread();

  private:
	int _processorIdentifier;
	int _processorThreads;
	int _cpuUsageThreshold;
	std::shared_ptr<spdlog::logger> _logger;
	nlohmann::json _configurationRoot;
	std::shared_ptr<MultiEventsSet> _multiEventsSet;
	std::shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	std::shared_ptr<MMSStorage> _mmsStorage;
	std::shared_ptr<long> _processorsThreadsNumber;
	ActiveEncodingsManager *_pActiveEncodingsManager;

	std::shared_ptr<ThreadsStatistic> _mmsThreadsStatistic;

	std::shared_ptr<MMSDeliveryAuthorization> _mmsDeliveryAuthorization;

	GetCpuUsage _getCpuUsage;
	std::mutex *_cpuUsageMutex;
	std::deque<int> *_cpuUsage;
	bool _cpuUsageThreadShutdown;

	std::string _processorMMS;

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

	std::string _facebookGraphAPIProtocol;
	std::string _facebookGraphAPIHostName;
	std::string _facebookGraphAPIVideoHostName;
	int _facebookGraphAPIPort;
	std::string _facebookGraphAPIVersion;
	long _facebookGraphAPITimeoutInSeconds;
	std::string _facebookGraphAPIClientId;
	std::string _facebookGraphAPIClientSecret;
	std::string _facebookGraphAPIRedirectURL;
	std::string _facebookGraphAPIAccessTokenURI;
	std::string _facebookGraphAPILiveVideosURI;

	std::string _youTubeDataAPIProtocol;
	std::string _youTubeDataAPIHostName;
	int _youTubeDataAPIPort;
	std::string _youTubeDataAPIRefreshTokenURI;
	std::string _youTubeDataAPIUploadVideoURI;
	std::string _youTubeDataAPILiveBroadcastURI;
	std::string _youTubeDataAPILiveStreamURI;
	std::string _youTubeDataAPILiveBroadcastBindURI;
	long _youTubeDataAPITimeoutInSeconds;
	long _youTubeDataAPITimeoutInSecondsForUploadVideo;
	std::string _youTubeDataAPIClientId;
	std::string _youTubeDataAPIClientSecret;

	std::string _deliveryProtocol;
	std::string _deliveryHost;

	bool _localCopyTaskEnabled;

	std::string _mmsWorkflowIngestionURL;
	int _mmsAPITimeoutInSeconds;

	int _waitingNFSSync_maxMillisecondsToWait;
	int _waitingNFSSync_milliSecondsWaitingBetweenChecks;

	std::string _liveRecorderVirtualVODImageLabel;

	std::string _emailProviderURL;
	std::string _emailUserName;
	std::string _emailPassword;
	std::string _emailCcsCommaSeparated;

	nlohmann::json getReviewedOutputsRoot(
		const nlohmann::json &outputsRoot, const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey, bool encodingProfileMandatory
	);

	nlohmann::json getReviewedFiltersRoot(nlohmann::json filtersRoot, const std::shared_ptr<Workspace> &workspace, int64_t ingestionJobKey) const;

	bool newThreadPermission(std::shared_ptr<long> processorsThreadsNumber);

	static bool isMaintenanceMode();
	static bool isProcessorShutdown();

	void handleCheckIngestionEvent();

	void handleLocalAssetIngestionEventThread(std::shared_ptr<long> processorsThreadsNumber, LocalAssetIngestionEvent localAssetIngestionEvent);
	void handleLocalAssetIngestionEvent(std::shared_ptr<long> processorsThreadsNumber, LocalAssetIngestionEvent localAssetIngestionEvent);

	void
	handleMultiLocalAssetIngestionEventThread(std::shared_ptr<long> processorsThreadsNumber, MultiLocalAssetIngestionEvent multiLocalAssetIngestionEvent);

	void handleCheckEncodingEvent();

	void handleContentRetentionEventThread(std::shared_ptr<long> processorsThreadsNumber);

	void handleDBDataRetentionEventThread();

	void handleGEOInfoEventThread();

	void handleCheckRefreshPartitionFreeSizeEventThread();

	// void handleMainAndBackupOfRunnungLiveRecordingHA (std::shared_ptr<long> processorsThreadsNumber);

	void removeContentThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void ftpDeliveryContentThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void postOnFacebookThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void postOnYouTubeThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void httpCallbackThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void userHttpCallback(
		int64_t ingestionJobKey, std::string httpProtocol, std::string httpHostName, int httpPort, std::string httpURI, std::string httpURLParameters, bool formData,
		std::string httpMethod, long callbackTimeoutInSeconds, nlohmann::json userHeadersRoot, std::string &data, std::string userName, std::string password, int maxRetries
	);

	void localCopyContentThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void copyContent(int64_t ingestionJobKey, std::string mmsAssetPathName, std::string localPath, std::string localFileName);

	void manageFaceRecognitionMediaTask(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageFaceIdentificationMediaTask(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageLiveRecorder(
		int64_t ingestionJobKey, const std::string &ingestionJobLabel, MMSEngineDBFacade::IngestionStatus ingestionStatus,
		const std::shared_ptr<Workspace> &workspace, nlohmann::json parametersRoot
	);

	void manageLiveProxy(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot
	);

	void manageVODProxy(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageCountdown(
		int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, /* std::string ingestionDate, */ std::shared_ptr<Workspace> workspace,
		nlohmann::json parametersRoot, std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void
	manageLiveGrid(int64_t ingestionJobKey, MMSEngineDBFacade::IngestionStatus ingestionStatus, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot);

	void manageLiveCutThread_streamSegmenter(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot
	);
	void manageLiveCutThread_hlsSegmenter(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::string ingestionJobLabel, std::shared_ptr<Workspace> workspace,
		nlohmann::json parametersRoot
	);

	void youTubeLiveBroadcastThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::string ingestionJobLabel, std::shared_ptr<Workspace> workspace,
		nlohmann::json parametersRoot
	);

	void facebookLiveBroadcastThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::string ingestionJobLabel, std::shared_ptr<Workspace> workspace,
		nlohmann::json parametersRoot
	);

	void extractTracksContentThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void changeFileFormatThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	[[nodiscard]] std::string generateMediaMetadataToIngest(
		int64_t ingestionJobKey, std::string fileFormat, std::string title, int64_t imageOfVideoMediaItemKey, int64_t cutOfVideoMediaItemKey,
		int64_t cutOfAudioMediaItemKey, double startTimeInSeconds, double endTimeInSeconds, nlohmann::json parametersRoot
	) const;

	void manageEncodeTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageGroupOfTasks(int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot);

	void manageVideoSpeedTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageAddSilentAudioTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void managePictureInPictureTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageIntroOutroOverlayTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageOverlayImageOnVideoTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void manageOverlayTextOnVideoTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void generateAndIngestFrameThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace,
		MMSEngineDBFacade::IngestionType ingestionType, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void manageGenerateFramesTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, MMSEngineDBFacade::IngestionType ingestionType, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	void fillGenerateFramesParameters(
		std::shared_ptr<Workspace> workspace, int64_t ingestionJobKey, MMSEngineDBFacade::IngestionType ingestionType, nlohmann::json parametersRoot,
		int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey,

		int &periodInSeconds, double &startTimeInSeconds, int &maxFramesNumber, std::string &videoFilter, bool &mjpeg, int &imageWidth, int &imageHeight,
		int64_t &durationInMilliSeconds
	);

	void manageSlideShowTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	/*
	void generateAndIngestSlideshow(
		int64_t ingestionJobKey,
		std::shared_ptr<Workspace> workspace,
		json parametersRoot,
		std::vector<std::pair<int64_t,Validator::DependencyType>>& dependencies);
	*/
	void manageConcatThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void manageCutMediaThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void emailNotificationThread(
		std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> dependencies
	);

	void
	checkStreamingThread(std::shared_ptr<long> processorsThreadsNumber, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot);

	void manageMediaCrossReferenceTask(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, nlohmann::json parametersRoot,
		std::vector<std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool>> &dependencies
	);

	std::tuple<MMSEngineDBFacade::IngestionStatus, std::string, std::string, int64_t, std::string, int, bool> getMediaSourceDetails(
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, MMSEngineDBFacade::IngestionType ingestionType, nlohmann::json parametersRoot
	);

	void
	validateMediaSourceFile(int64_t ingestionJobKey, std::string mediaSourcePathName, std::string mediaFileFormat, std::string md5FileCheckSum, int fileSizeInBytes);

	void downloadMediaSourceFileThread(
		std::shared_ptr<long> processorsThreadsNumber, std::string sourceReferenceURL, bool regenerateTimestamps, int m3u8TarGzOrStreaming,
		int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace
	);
	void moveMediaSourceFileThread(
		std::shared_ptr<long> processorsThreadsNumber, std::string sourceReferenceURL, int m3u8TarGzOrStreaming, int64_t ingestionJobKey,
		std::shared_ptr<Workspace> workspace
	);
	void copyMediaSourceFileThread(
		std::shared_ptr<long> processorsThreadsNumber, std::string sourceReferenceURL, int m3u8TarGzOrStreaming, int64_t ingestionJobKey,
		std::shared_ptr<Workspace> workspace
	);

	// void manageTarFileInCaseOfIngestionOfSegments(
	// 	int64_t ingestionJobKey,
	// 	std::string tarBinaryPathName, std::string workspaceIngestionRepository,
	// 	std::string sourcePathName
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
		std::string mmsAssetPathName, std::string fileName, int64_t sizeInBytes, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, int64_t mediaItemKey,
		int64_t physicalPathKey, std::string ftpServer, int ftpPort, std::string ftpUserName, std::string ftpPassword, std::string ftpRemoteDirectory,
		std::string ftpRemoteFileName
	);

	void postVideoOnFacebook(
		std::string mmsAssetPathName, int64_t sizeInBytes, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, std::string facebookConfigurationLabel,
		std::string facebookDestination, std::string facebookNodeId
	);

	void postVideoOnYouTube(
		std::string mmsAssetPathName, int64_t sizeInBytes, int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, std::string youTubeConfigurationLabel,
		std::string youTubeTitle, std::string youTubeDescription, nlohmann::json youTubeTags, int youTubeCategoryId, std::string youTubePrivacy, bool youTubeMadeForKids
	);
	std::pair<int64_t, int64_t>
	youTubeDetailsToResumePostVideo(int64_t ingestionJobKey, std::string youTubeUploadURL, std::string youTubeAccessToken, int64_t sizeInBytes);

	std::string getYouTubeAccessTokenByConfigurationLabel(int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, std::string youTubeConfigurationLabel);

	std::string getFacebookPageToken(int64_t ingestionJobKey, std::shared_ptr<Workspace> workspace, std::string facebookConfigurationLabel, std::string facebookPageId);

	std::tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, std::string, std::string, std::string, std::string, int64_t, std::string, std::string, bool> processDependencyInfo(
		std::shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
		std::tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> keyAndDependencyType
	);

	std::string getEncodedFileExtensionByEncodingProfile(nlohmann::json encodingProfileDetailsRoot);
};
