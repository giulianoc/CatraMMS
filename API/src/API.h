/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   API.h
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#pragma once

#include "BandwidthUsageThread.h"
#include "Datetime.h"
#include "FastCGIAPI.h"
#include "MMSDeliveryAuthorization.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "PostgresConnection.h"

class API final : public FastCGIAPI
{
  public:
	class APIAuthorizationDetails final : public FCGIRequestData::AuthorizationDetails
	{
	public:
		int64_t userKey{};
		std::shared_ptr<Workspace> workspace;
		bool admin{};
		bool canCreateRemoveWorkspace{};
		bool canIngestWorkflow{};
		bool canCreateProfiles{};
		bool canDeliveryAuthorization{};
		bool canShareWorkspace{};
		bool canEditMedia{};
		bool canEditConfiguration{};
		bool canKillEncoding{};
		bool canCancelIngestionJob{};
		bool canEditEncodersPool{};
		bool canApplicationRecorder{};
		bool canCreateRemoveLiveChannel{};
		bool canUpdateEncoderAndDeliveryStats{};

		std::string toString()
		{
			return std::format("userKey: {}"
				", admin: {}"
				", canCreateRemoveWorkspace: {}"
				", canIngestWorkflow: {}"
				", canCreateProfiles: {}"
				", canDeliveryAuthorization: {}"
				", canShareWorkspace: {}"
				", canEditMedia: {}"
				", canEditConfiguration: {}"
				", canKillEncoding: {}"
				", canCancelIngestionJob: {}"
				", canEditEncodersPool: {}"
				", canApplicationRecorder: {}"
				", canCreateRemoveLiveChannel: {}"
				", canUpdateEncoderAndDeliveryStats: {}",
				userKey, admin, canCreateRemoveWorkspace, canIngestWorkflow, canCreateProfiles, canDeliveryAuthorization,
				canShareWorkspace, canEditMedia, canEditConfiguration, canKillEncoding, canCancelIngestionJob, canEditEncodersPool,
				canApplicationRecorder, canCreateRemoveLiveChannel, canUpdateEncoderAndDeliveryStats
				);
		}
	};

	struct FileUploadProgressData
	{
		struct RequestData
		{
			int64_t _ingestionJobKey;
			std::string _progressId;
			std::string _binaryVirtualHostName;
			std::string _binaryListenHost;
			double _lastPercentageUpdated;
			int _callFailures;
			bool _contentRangePresent;
			uint64_t _contentRangeStart;
			uint64_t _contentRangeEnd;
			uint64_t _contentRangeSize;
		};

		std::mutex _mutex;
		std::vector<RequestData> _filesUploadProgressToBeMonitored;
	};

	API(bool noFileSystemAccess, const nlohmann::json& configurationRoot, const std::shared_ptr<MMSEngineDBFacade>& mmsEngineDBFacade, const std::shared_ptr<MMSStorage>& mmsStorage,
		const std::shared_ptr<MMSDeliveryAuthorization>& mmsDeliveryAuthorization, std::mutex *fcgiAcceptMutex, FileUploadProgressData *fileUploadProgressData,
		const std::shared_ptr<BandwidthUsageThread>& bandwidthUsageThread);

	~API() override;

	void manageRequestAndResponse(const std::string_view& sThreadId, /* int64_t requestIdentifier, */ FCGX_Request &request,
		const FCGIRequestData& requestData) override;

	std::shared_ptr<FCGIRequestData::AuthorizationDetails> checkAuthorization(const std::string_view& sThreadId,
		const FCGIRequestData& requestData, const std::string_view& userName, const std::string_view& password) override;

	bool basicAuthenticationRequired(const FCGIRequestData& requestData) override;

	void sendError(FCGX_Request &request, int htmlResponseCode, const std::string_view& errorMessage) override;

	void fileUploadProgressCheckThread();
	void stopUploadFileProgressThread();

  private:
	nlohmann::json _configurationRoot;

	std::shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	bool _noFileSystemAccess;
	std::shared_ptr<MMSStorage> _mmsStorage;
	std::shared_ptr<MMSDeliveryAuthorization> _mmsDeliveryAuthorization;

	MMSEngineDBFacade::EncodingPriority _encodingPriorityWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPriority::Low;
	MMSEngineDBFacade::EncodingPeriod _encodingPeriodWorkspaceDefaultValue = MMSEngineDBFacade::EncodingPeriod::Daily;
	int _maxIngestionsNumberWorkspaceDefaultValue{};
	int _maxStorageInMBWorkspaceDefaultValue{};
	int _expirationInDaysWorkspaceDefaultValue{};

	std::string _sharedEncodersPoolLabel;
	nlohmann::json _sharedEncodersLabel;
	int _defaultSharedHLSChannelsNumber{};

	// unsigned long       _binaryBufferLength;
	unsigned long _progressUpdatePeriodInSeconds{};
	int _webServerPort{};
	bool _fileUploadProgressThreadShutdown{};
	int _maxProgressCallFailures{};
	std::string _progressURI;

	std::shared_ptr<BandwidthUsageThread> _bandwidthUsageThread;

	int _maxPageSize{};

	std::string _apiProtocol;
	std::string _apiHostname;
	int _apiPort{};
	std::string _apiVersion;

	// std::string				_ffmpegEncoderProtocol;
	// int					_ffmpegEncoderPort;
	std::string _ffmpegEncoderUser;
	std::string _ffmpegEncoderPassword;
	int _ffmpegEncoderTimeoutInSeconds{};
	std::string _ffmpegEncoderKillEncodingURI;
	std::string _ffmpegEncoderChangeLiveProxyPlaylistURI;
	std::string _ffmpegEncoderChangeLiveProxyOverlayTextURI;

	int _intervalInSecondsToCheckEncodingFinished{};

	int _maxSecondsToWaitAPIIngestionLock{};

	int _defaultTTLInSeconds{};
	int _defaultMaxRetries{};
	bool _defaultRedirect{};
	std::string _deliveryProtocol;
	std::string _deliveryHost_authorizationThroughParameter;
	std::string _deliveryHost_authorizationThroughPath;

	bool _ldapEnabled{};
	std::string _ldapURL;
	std::string _ldapCertificatePathName;
	std::string _ldapManagerUserName;
	std::string _ldapManagerPassword;
	std::string _ldapBaseDn;
	std::string _ldapDefaultWorkspaceKeys;

	std::string _mmsVersion;

	std::string _keyPairId;
	std::string _privateKeyPEMPathName;
	// json _vodCloudFrontHostNamesRoot;

	std::string _emailProviderURL;
	std::string _emailUserName;
	std::string _emailPassword;
	std::string _emailCcsCommaSeparated;

	bool _registerUserEnabled{};

	std::string _guiProtocol;
	std::string _guiHostname;
	int _guiPort{};

	int _waitingNFSSync_maxMillisecondsToWait{};
	int _waitingNFSSync_milliSecondsWaitingBetweenChecks{};

	FileUploadProgressData *_fileUploadProgressData{};

	void loadConfiguration(const nlohmann::json &configurationRoot, FileUploadProgressData *fileUploadProgressData);

	void registerUser(const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void emailFormatCheck(std::string email);

	void updateUser(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void createTokenToResetPassword(const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void resetPassword(const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void updateWorkspace(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void setWorkspaceAsDefault(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void createWorkspace(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void deleteWorkspace(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void unshareWorkspace(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void
	workspaceUsage(const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void shareWorkspace_(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void workspaceList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void confirmRegistration(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void login(const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void addInvoice(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void invoiceList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void mmsSupport(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void status(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void avgBandwidthUsage(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void binaryAuthorization(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void deliveryAuthorizationThroughParameter(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void deliveryAuthorizationThroughPath(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void manageHTTPStreamingManifest_authorizationThroughParameter(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestionRootsStatus(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestionRootMetaDataContent(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestionJobsStatus(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void cancelIngestionJob(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updateIngestionJob(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestionJobSwitchToEncoder(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodingJobsStatus(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodingJobPriority(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void killOrCancelEncodingJob(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void changeLiveProxyPlaylist(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void changeLiveProxyOverlayText(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void killEncodingJob(int64_t encoderKey, int64_t ingestionJobKey, int64_t encodingJobKey, std::string killType);

	void mediaItemsList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updateMediaItem(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updatePhysicalPath(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void tagsList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodingProfilesSetsList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodingProfilesList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestion(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	static nlohmann::json manageWorkflowVariables(const std::string_view& requestBody, nlohmann::json variablesValuesToBeUsedRoot);

	static void manageReferencesInput(
		int64_t ingestionRootKey, std::string taskOrGroupOfTasksLabel, std::string ingestionType, nlohmann::json &taskOrGroupOfTasksRoot, bool parametersSectionPresent,
		nlohmann::json &parametersRoot, std::vector<int64_t> &dependOnIngestionJobKeysForStarting, std::vector<int64_t> &dependOnIngestionJobKeysOverallInput,
		std::unordered_map<std::string, std::vector<int64_t>> &mapLabelAndIngestionJobKey
	);

#ifdef __POSTGRES__
	std::vector<int64_t> ingestionSingleTask(
		PostgresConnTrans &trans, int64_t userKey, const std::string& apiKey, std::shared_ptr<Workspace> workspace, int64_t ingestionRootKey, nlohmann::json &taskRoot,
		std::vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess, std::vector<int64_t> dependOnIngestionJobKeysOverallInput,
		std::unordered_map<std::string, std::vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* std::string& responseBody, */ nlohmann::json &responseBodyTasksRoot
	);
#else
	vector<int64_t> ingestionSingleTask(
		std::shared_ptr<MySQLConnection> conn, int64_t userKey, std::string apiKey, std::shared_ptr<Workspace> workspace, int64_t ingestionRootKey, json &taskRoot,
		vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess, vector<int64_t> dependOnIngestionJobKeysOverallInput,
		unordered_map<std::string, vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* std::string& responseBody, */ json &responseBodyTasksRoot
	);
#endif

#ifdef __POSTGRES__
	std::vector<int64_t> ingestionGroupOfTasks(
		PostgresConnTrans &trans, int64_t userKey, std::string apiKey, const std::shared_ptr<Workspace>& workspace, int64_t ingestionRootKey, nlohmann::json &groupOfTasksRoot,
		std::vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess, std::vector<int64_t> dependOnIngestionJobKeysOverallInput,
		std::unordered_map<std::string, std::vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* std::string& responseBody, */ nlohmann::json &responseBodyTasksRoot
	);
#else
	vector<int64_t> ingestionGroupOfTasks(
		std::shared_ptr<MySQLConnection> conn, int64_t userKey, std::string apiKey, std::shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
		json &groupOfTasksRoot, vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,
		vector<int64_t> dependOnIngestionJobKeysOverallInput, unordered_map<std::string, vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* std::string& responseBody, */ json &responseBodyTasksRoot
	);
#endif

#ifdef __POSTGRES__
	void ingestionEvents(
		PostgresConnTrans &trans, int64_t userKey, std::string apiKey, std::shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
		nlohmann::json &taskOrGroupOfTasksRoot, std::vector<int64_t> dependOnIngestionJobKeysForStarting, std::vector<int64_t> dependOnIngestionJobKeysOverallInput,
		std::vector<int64_t> dependOnIngestionJobKeysOverallInputOnError, std::vector<int64_t> &referencesOutputIngestionJobKeys,
		std::unordered_map<std::string, std::vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* std::string& responseBody, */ nlohmann::json &responseBodyTasksRoot
	);
#else
	void ingestionEvents(
		std::shared_ptr<MySQLConnection> conn, int64_t userKey, std::string apiKey, std::shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
		json &taskOrGroupOfTasksRoot, vector<int64_t> dependOnIngestionJobKeysForStarting, vector<int64_t> dependOnIngestionJobKeysOverallInput,
		vector<int64_t> dependOnIngestionJobKeysOverallInputOnError, vector<int64_t> &referencesOutputIngestionJobKeys,
		unordered_map<std::string, vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* std::string& responseBody, */ json &responseBodyTasksRoot
	);
#endif

	void uploadedBinary(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	// void manageTarFileInCaseOfIngestionOfSegments(
	// 	int64_t ingestionJobKey,
	// 	std::string tarBinaryPathName, std::string workspaceIngestionRepository,
	// 	std::string sourcePathName);

	void addUpdateEncodingProfilesSet(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addEncodingProfile(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEncodingProfile(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEncodingProfilesSet(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void workflowsAsLibraryList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void workflowAsLibraryContent(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void saveWorkflowAsLibrary(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeWorkflowAsLibrary(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void createDeliveryAuthorization(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void createBulkOfDeliveryAuthorization(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addYouTubeConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyYouTubeConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeYouTubeConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void youTubeConfList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addFacebookConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyFacebookConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeFacebookConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void facebookConfList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addTwitchConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyTwitchConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeTwitchConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void twitchConfList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addTiktokConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyTiktokConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeTiktokConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void tiktokConfList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addStream(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyStream(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeStream(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void streamList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void streamFreePushEncoderPort(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addSourceTVStream(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifySourceTVStream(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeSourceTVStream(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void sourceTVStreamList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addRTMPChannelConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyRTMPChannelConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeRTMPChannelConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void rtmpChannelConfList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addSRTChannelConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifySRTChannelConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeSRTChannelConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void srtChannelConfList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addHLSChannelConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyHLSChannelConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeHLSChannelConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void hlsChannelConfList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addFTPConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyFTPConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeFTPConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ftpConfList(const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void addEMailConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyEMailConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEMailConf(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void emailConfList(const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void addRequestStatistic(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerContentList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerUserList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerMonthList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerDayList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerHourList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerCountryList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void loginStatisticList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addEncoder(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyEncoder(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updateEncoderBandwidthStats(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updateEncoderCPUUsageStats(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEncoder(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encoderList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodersPoolList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addEncodersPool(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyEncodersPool(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEncodersPool(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addAssociationWorkspaceEncoder(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeAssociationWorkspaceEncoder(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void deliveryServerList(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addDeliveryServer(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyDeliveryServer(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updateDeliveryServerBandwidthStats(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updateDeliveryServerCPUUsageStats(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeDeliveryServer(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	nlohmann::json getReviewedFiltersRoot(nlohmann::json filtersRoot, const std::shared_ptr<Workspace>& workspace, int64_t ingestionJobKey);
};