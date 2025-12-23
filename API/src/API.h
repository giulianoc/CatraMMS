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

#include "BandwidthStats.h"
#include "Datetime.h"
#include "FastCGIAPI.h"
#include "MMSDeliveryAuthorization.h"
#include "MMSStorage.h"
#include "PostgresConnection.h"

class API final : public FastCGIAPI
{
  public:
	class APIAuthorizationDetails final : public FCGIRequestData::AuthorizationDetails
	{
	public:
		int64_t userKey{};
		shared_ptr<Workspace> workspace;
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
	};

	struct FileUploadProgressData
	{
		struct RequestData
		{
			int64_t _ingestionJobKey;
			string _progressId;
			string _binaryVirtualHostName;
			string _binaryListenHost;
			double _lastPercentageUpdated;
			int _callFailures;
			bool _contentRangePresent;
			uint64_t _contentRangeStart;
			uint64_t _contentRangeEnd;
			uint64_t _contentRangeSize;
		};

		mutex _mutex;
		vector<RequestData> _filesUploadProgressToBeMonitored;
	};

	API(bool noFileSystemAccess, const json& configurationRoot, const shared_ptr<MMSEngineDBFacade>& mmsEngineDBFacade, const shared_ptr<MMSStorage>& mmsStorage,
		const shared_ptr<MMSDeliveryAuthorization>& mmsDeliveryAuthorization, mutex *fcgiAcceptMutex, FileUploadProgressData *fileUploadProgressData,
		const shared_ptr<atomic<uint64_t>>& avgBandwidthUsage);

	~API() override;

	void manageRequestAndResponse(const string_view& sThreadId, /* int64_t requestIdentifier, */ FCGX_Request &request,
		const FCGIRequestData& requestData) override;

	shared_ptr<FCGIRequestData::AuthorizationDetails> checkAuthorization(const string_view& sThreadId,
		const FCGIRequestData& requestData, const string_view& userName, const string_view& password) override;

	bool basicAuthenticationRequired(const FCGIRequestData& requestData) override;

	void sendError(FCGX_Request &request, int htmlResponseCode, const string_view& errorMessage) override;

	void fileUploadProgressCheckThread();
	void stopUploadFileProgressThread();

	void bandwidthUsageThread();
	void stopBandwidthUsageThread();

  private:
	json _configurationRoot;

	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	bool _noFileSystemAccess;
	shared_ptr<MMSStorage> _mmsStorage;
	shared_ptr<MMSDeliveryAuthorization> _mmsDeliveryAuthorization;

	MMSEngineDBFacade::EncodingPriority _encodingPriorityWorkspaceDefaultValue;
	MMSEngineDBFacade::EncodingPeriod _encodingPeriodWorkspaceDefaultValue;
	int _maxIngestionsNumberWorkspaceDefaultValue{};
	int _maxStorageInMBWorkspaceDefaultValue{};
	int _expirationInDaysWorkspaceDefaultValue{};

	string _sharedEncodersPoolLabel;
	json _sharedEncodersLabel;
	int _defaultSharedHLSChannelsNumber{};

	// unsigned long       _binaryBufferLength;
	unsigned long _progressUpdatePeriodInSeconds{};
	int _webServerPort{};
	bool _fileUploadProgressThreadShutdown{};
	int _maxProgressCallFailures{};
	string _progressURI;

	bool _bandwidthUsageThreadShutdown{};
	unsigned long _bandwidthUsagePeriodInSeconds{};
	shared_ptr<std::atomic<uint64_t>> _avgBandwidthUsage;
	BandwidthStats _bandwidthStats;

	int _maxPageSize{};

	string _apiProtocol;
	string _apiHostname;
	int _apiPort{};
	string _apiVersion;

	// string				_ffmpegEncoderProtocol;
	// int					_ffmpegEncoderPort;
	string _ffmpegEncoderUser;
	string _ffmpegEncoderPassword;
	int _ffmpegEncoderTimeoutInSeconds{};
	string _ffmpegEncoderKillEncodingURI;
	string _ffmpegEncoderChangeLiveProxyPlaylistURI;
	string _ffmpegEncoderChangeLiveProxyOverlayTextURI;

	int _intervalInSecondsToCheckEncodingFinished{};

	int _maxSecondsToWaitAPIIngestionLock{};

	int _defaultTTLInSeconds{};
	int _defaultMaxRetries{};
	bool _defaultRedirect{};
	string _deliveryProtocol;
	string _deliveryHost_authorizationThroughParameter;
	string _deliveryHost_authorizationThroughPath;

	string _deliveryExternalNetworkInterface;

	bool _ldapEnabled{};
	string _ldapURL;
	string _ldapCertificatePathName;
	string _ldapManagerUserName;
	string _ldapManagerPassword;
	string _ldapBaseDn;
	string _ldapDefaultWorkspaceKeys;

	string _mmsVersion;

	string _keyPairId;
	string _privateKeyPEMPathName;
	// json _vodCloudFrontHostNamesRoot;

	string _emailProviderURL;
	string _emailUserName;
	string _emailPassword;
	string _emailCcsCommaSeparated;

	bool _registerUserEnabled{};

	string _guiProtocol;
	string _guiHostname;
	int _guiPort{};

	int _waitingNFSSync_maxMillisecondsToWait{};
	int _waitingNFSSync_milliSecondsWaitingBetweenChecks{};

	FileUploadProgressData *_fileUploadProgressData{};

	void loadConfiguration(json configurationRoot, FileUploadProgressData *fileUploadProgressData);

	void registerUser(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void emailFormatCheck(string email);

	void updateUser(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void createTokenToResetPassword(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void resetPassword(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void updateWorkspace(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void setWorkspaceAsDefault(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void createWorkspace(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void deleteWorkspace(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void unshareWorkspace(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void
	workspaceUsage(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void shareWorkspace_(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void workspaceList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void confirmRegistration(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void login(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void addInvoice(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void invoiceList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void mmsSupport(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void status(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void avgBandwidthUsage_(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void binaryAuthorization(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void deliveryAuthorizationThroughParameter(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void deliveryAuthorizationThroughPath(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void manageHTTPStreamingManifest_authorizationThroughParameter(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestionRootsStatus(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestionRootMetaDataContent(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestionJobsStatus(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void cancelIngestionJob(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updateIngestionJob(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestionJobSwitchToEncoder(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodingJobsStatus(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodingJobPriority(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void killOrCancelEncodingJob(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void changeLiveProxyPlaylist(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void changeLiveProxyOverlayText(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void killEncodingJob(int64_t encoderKey, int64_t ingestionJobKey, int64_t encodingJobKey, string killType);

	void mediaItemsList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updateMediaItem(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void updatePhysicalPath(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void tagsList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodingProfilesSetsList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodingProfilesList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ingestion(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	static json manageWorkflowVariables(const string_view& requestBody, json variablesValuesToBeUsedRoot);

	static void manageReferencesInput(
		int64_t ingestionRootKey, string taskOrGroupOfTasksLabel, string ingestionType, json &taskOrGroupOfTasksRoot, bool parametersSectionPresent,
		json &parametersRoot, vector<int64_t> &dependOnIngestionJobKeysForStarting, vector<int64_t> &dependOnIngestionJobKeysOverallInput,
		unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey
	);

#ifdef __POSTGRES__
	vector<int64_t> ingestionSingleTask(
		PostgresConnTrans &trans, int64_t userKey, const string& apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey, json &taskRoot,
		vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess, vector<int64_t> dependOnIngestionJobKeysOverallInput,
		unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* string& responseBody, */ json &responseBodyTasksRoot
	);
#else
	vector<int64_t> ingestionSingleTask(
		shared_ptr<MySQLConnection> conn, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey, json &taskRoot,
		vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess, vector<int64_t> dependOnIngestionJobKeysOverallInput,
		unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* string& responseBody, */ json &responseBodyTasksRoot
	);
#endif

#ifdef __POSTGRES__
	vector<int64_t> ingestionGroupOfTasks(
		PostgresConnTrans &trans, int64_t userKey, string apiKey, const shared_ptr<Workspace>& workspace, int64_t ingestionRootKey, json &groupOfTasksRoot,
		vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess, vector<int64_t> dependOnIngestionJobKeysOverallInput,
		unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* string& responseBody, */ json &responseBodyTasksRoot
	);
#else
	vector<int64_t> ingestionGroupOfTasks(
		shared_ptr<MySQLConnection> conn, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
		json &groupOfTasksRoot, vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,
		vector<int64_t> dependOnIngestionJobKeysOverallInput, unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* string& responseBody, */ json &responseBodyTasksRoot
	);
#endif

#ifdef __POSTGRES__
	void ingestionEvents(
		PostgresConnTrans &trans, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
		json &taskOrGroupOfTasksRoot, vector<int64_t> dependOnIngestionJobKeysForStarting, vector<int64_t> dependOnIngestionJobKeysOverallInput,
		vector<int64_t> dependOnIngestionJobKeysOverallInputOnError, vector<int64_t> &referencesOutputIngestionJobKeys,
		unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* string& responseBody, */ json &responseBodyTasksRoot
	);
#else
	void ingestionEvents(
		shared_ptr<MySQLConnection> conn, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
		json &taskOrGroupOfTasksRoot, vector<int64_t> dependOnIngestionJobKeysForStarting, vector<int64_t> dependOnIngestionJobKeysOverallInput,
		vector<int64_t> dependOnIngestionJobKeysOverallInputOnError, vector<int64_t> &referencesOutputIngestionJobKeys,
		unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey,
		/* string& responseBody, */ json &responseBodyTasksRoot
	);
#endif

	void uploadedBinary(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	// void manageTarFileInCaseOfIngestionOfSegments(
	// 	int64_t ingestionJobKey,
	// 	string tarBinaryPathName, string workspaceIngestionRepository,
	// 	string sourcePathName);

	void addUpdateEncodingProfilesSet(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addEncodingProfile(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEncodingProfile(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEncodingProfilesSet(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void workflowsAsLibraryList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void workflowAsLibraryContent(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void saveWorkflowAsLibrary(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeWorkflowAsLibrary(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void createDeliveryAuthorization(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void createBulkOfDeliveryAuthorization(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addYouTubeConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyYouTubeConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeYouTubeConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void youTubeConfList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addFacebookConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyFacebookConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeFacebookConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void facebookConfList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addTwitchConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyTwitchConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeTwitchConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void twitchConfList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addTiktokConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyTiktokConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeTiktokConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void tiktokConfList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addStream(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyStream(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeStream(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void streamList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void streamFreePushEncoderPort(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addSourceTVStream(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifySourceTVStream(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeSourceTVStream(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void sourceTVStreamList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addAWSChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyAWSChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeAWSChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void awsChannelConfList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addCDN77ChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyCDN77ChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeCDN77ChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void cdn77ChannelConfList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addRTMPChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyRTMPChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeRTMPChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void rtmpChannelConfList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addSRTChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifySRTChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeSRTChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void srtChannelConfList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addHLSChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyHLSChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeHLSChannelConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void hlsChannelConfList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addFTPConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyFTPConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeFTPConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void ftpConfList(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void addEMailConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyEMailConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEMailConf(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void emailConfList(const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void addRequestStatistic(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerContentList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerUserList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerMonthList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerDayList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerHourList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void requestStatisticPerCountryList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void loginStatisticList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addEncoder(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyEncoder(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEncoder(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encoderList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void encodersPoolList(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addEncodersPool(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void modifyEncodersPool(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeEncodersPool(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void addAssociationWorkspaceEncoder(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void removeAssociationWorkspaceEncoder(
		const string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	json getReviewedFiltersRoot(json filtersRoot, const shared_ptr<Workspace>& workspace, int64_t ingestionJobKey);
};