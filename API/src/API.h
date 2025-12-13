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
		const string_view& requestBody, bool responseBodyCompressed, unsigned long contentLength,
		const unordered_map<string, string> &requestDetails, const unordered_map<string, string>& queryParameters
	) override;

	shared_ptr<AuthorizationDetails> checkAuthorization(const string_view& sThreadId, const string_view& userName, const string_view& password) override;

	bool basicAuthenticationRequired(const string &requestURI, const unordered_map<string, string> &queryParameters) override;

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
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void emailFormatCheck(string email);

	void updateUser(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void createTokenToResetPassword(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void resetPassword(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void updateWorkspace(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void setWorkspaceAsDefault(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void createWorkspace(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void deleteWorkspace(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void unshareWorkspace(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void
	workspaceUsage(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void shareWorkspace_(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void workspaceList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void confirmRegistration(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void login(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void addInvoice(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void invoiceList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void mmsSupport(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void status(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void avgBandwidthUsage(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void binaryAuthorization(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void deliveryAuthorizationThroughParameter(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void deliveryAuthorizationThroughPath(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void manageHTTPStreamingManifest_authorizationThroughParameter(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void ingestionRootsStatus(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void ingestionRootMetaDataContent(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void ingestionJobsStatus(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void cancelIngestionJob(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void updateIngestionJob(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void ingestionJobSwitchToEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void encodingJobsStatus(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void encodingJobPriority(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void killOrCancelEncodingJob(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void changeLiveProxyPlaylist(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void changeLiveProxyOverlayText(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void killEncodingJob(int64_t encoderKey, int64_t ingestionJobKey, int64_t encodingJobKey, string killType);

	void mediaItemsList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void updateMediaItem(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void updatePhysicalPath(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void tagsList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void encodingProfilesSetsList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void encodingProfilesList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void ingestion(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
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
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	// void manageTarFileInCaseOfIngestionOfSegments(
	// 	int64_t ingestionJobKey,
	// 	string tarBinaryPathName, string workspaceIngestionRepository,
	// 	string sourcePathName);

	void addUpdateEncodingProfilesSet(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addEncodingProfile(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeEncodingProfile(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeEncodingProfilesSet(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void workflowsAsLibraryList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void workflowAsLibraryContent(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void saveWorkflowAsLibrary(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeWorkflowAsLibrary(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void createDeliveryAuthorization(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void createBulkOfDeliveryAuthorization(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addYouTubeConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyYouTubeConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeYouTubeConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void youTubeConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addFacebookConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyFacebookConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeFacebookConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void facebookConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addTwitchConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyTwitchConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeTwitchConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void twitchConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
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
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void streamList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void streamFreePushEncoderPort(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addSourceTVStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifySourceTVStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeSourceTVStream(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void sourceTVStreamList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addAWSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyAWSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeAWSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void awsChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addCDN77ChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyCDN77ChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeCDN77ChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void cdn77ChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addRTMPChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyRTMPChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeRTMPChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void rtmpChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addSRTChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifySRTChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeSRTChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void srtChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addHLSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyHLSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeHLSChannelConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void hlsChannelConfList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addFTPConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyFTPConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeFTPConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void ftpConfList(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void addEMailConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyEMailConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeEMailConf(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void emailConfList(const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void addRequestStatistic(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void requestStatisticList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void requestStatisticPerContentList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void requestStatisticPerUserList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void requestStatisticPerMonthList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void requestStatisticPerDayList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void requestStatisticPerHourList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void requestStatisticPerCountryList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void loginStatisticList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void encoderList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void encodersPoolList(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addEncodersPool(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void modifyEncodersPool(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeEncodersPool(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void addAssociationWorkspaceEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	void removeAssociationWorkspaceEncoder(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody,
		bool responseBodyCompressed, const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters
	);

	json getReviewedFiltersRoot(json filtersRoot, const shared_ptr<Workspace>& workspace, int64_t ingestionJobKey);
};