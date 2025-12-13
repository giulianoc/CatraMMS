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
	class APIAuthorizationDetails final : public FastCGIAPI::AuthorizationDetails
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

	void manageRequestAndResponse(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI, const string_view& requestMethod,
		const string_view& requestBody, bool responseBodyCompressed, unsigned long contentLength
	) override;

	shared_ptr<AuthorizationDetails> checkAuthorization(const string_view& sThreadId, const string_view& userName, const string_view& password) override;

	bool basicAuthenticationRequired(const string &requestURI) override;

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

	void registerUser(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed);

	void emailFormatCheck(string email);

	void updateUser(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void createTokenToResetPassword(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed);

	void resetPassword(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed);

	void updateWorkspace(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void setWorkspaceAsDefault(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void createWorkspace(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void deleteWorkspace(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void unshareWorkspace(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void
	workspaceUsage(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed);

	void shareWorkspace_(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void workspaceList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void confirmRegistration(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void login(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed);

	void addInvoice(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void invoiceList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void mmsSupport(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void status(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void avgBandwidthUsage(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void binaryAuthorization(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void deliveryAuthorizationThroughParameter(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void deliveryAuthorizationThroughPath(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void manageHTTPStreamingManifest_authorizationThroughParameter(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void ingestionRootsStatus(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void ingestionRootMetaDataContent(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void ingestionJobsStatus(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void cancelIngestionJob(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void updateIngestionJob(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void ingestionJobSwitchToEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void encodingJobsStatus(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void encodingJobPriority(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void killOrCancelEncodingJob(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void changeLiveProxyPlaylist(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void changeLiveProxyOverlayText(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void killEncodingJob(int64_t encoderKey, int64_t ingestionJobKey, int64_t encodingJobKey, string killType);

	void mediaItemsList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void updateMediaItem(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void updatePhysicalPath(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void tagsList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void encodingProfilesSetsList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void encodingProfilesList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void ingestion(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
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
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	// void manageTarFileInCaseOfIngestionOfSegments(
	// 	int64_t ingestionJobKey,
	// 	string tarBinaryPathName, string workspaceIngestionRepository,
	// 	string sourcePathName);

	void addUpdateEncodingProfilesSet(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addEncodingProfile(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeEncodingProfile(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeEncodingProfilesSet(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void workflowsAsLibraryList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void workflowAsLibraryContent(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void saveWorkflowAsLibrary(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeWorkflowAsLibrary(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void createDeliveryAuthorization(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void createBulkOfDeliveryAuthorization(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addYouTubeConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyYouTubeConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeYouTubeConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void youTubeConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addFacebookConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyFacebookConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeFacebookConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void facebookConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addTwitchConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyTwitchConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeTwitchConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void twitchConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addTiktokConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyTiktokConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeTiktokConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void tiktokConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void streamList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void streamFreePushEncoderPort(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addSourceTVStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifySourceTVStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeSourceTVStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void sourceTVStreamList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addAWSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyAWSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeAWSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void awsChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addCDN77ChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyCDN77ChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeCDN77ChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void cdn77ChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addRTMPChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyRTMPChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeRTMPChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void rtmpChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addSRTChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifySRTChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeSRTChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void srtChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addHLSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyHLSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeHLSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void hlsChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addFTPConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyFTPConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeFTPConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void ftpConfList(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed);

	void addEMailConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyEMailConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeEMailConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void emailConfList(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed);

	void addRequestStatistic(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void requestStatisticList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void requestStatisticPerContentList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void requestStatisticPerUserList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void requestStatisticPerMonthList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void requestStatisticPerDayList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void requestStatisticPerHourList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void requestStatisticPerCountryList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void loginStatisticList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void encoderList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void encodersPoolList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addEncodersPool(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void modifyEncodersPool(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeEncodersPool(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void addAssociationWorkspaceEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	void removeAssociationWorkspaceEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed
	);

	json getReviewedFiltersRoot(json filtersRoot, const shared_ptr<Workspace>& workspace, int64_t ingestionJobKey);
};
