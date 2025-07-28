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

#ifndef API_h
#define API_h

#include "FastCGIAPI.h"
#include "MMSDeliveryAuthorization.h"
#include "MMSStorage.h"
#include "PostgresConnection.h"

class API : public FastCGIAPI
{
  public:
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
			long long _contentRangeStart;
			long long _contentRangeEnd;
			long long _contentRangeSize;
		};

		mutex _mutex;
		vector<RequestData> _filesUploadProgressToBeMonitored;
	};

	API(bool noFileSystemAccess, json configurationRoot, shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, shared_ptr<MMSStorage> mmsStorage,
		shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization, mutex *fcgiAcceptMutex, FileUploadProgressData *fileUploadProgressData);

	~API();

	virtual void manageRequestAndResponse(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestURI, string requestMethod,
		unordered_map<string, string> queryParameters, bool authorizationPresent, string userName, string password, unsigned long contentLength,
		string requestBody, unordered_map<string, string> &requestDetails
	);

	virtual void checkAuthorization(string sThreadId, string userName, string password);

	virtual bool basicAuthenticationRequired(string requestURI, unordered_map<string, string> queryParameters);

	virtual void sendError(FCGX_Request &request, int htmlResponseCode, string errorMessage);

	void fileUploadProgressCheck();
	void stopUploadFileProgressThread();

	void bandwidthUsageThread();
	void stopBandwidthUsageThread();

  private:
	json _configurationRoot;

	tuple<int64_t, shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool> _userKeyWorkspaceAndFlags;
	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	bool _noFileSystemAccess;
	shared_ptr<MMSStorage> _mmsStorage;
	shared_ptr<MMSDeliveryAuthorization> _mmsDeliveryAuthorization;

	MMSEngineDBFacade::EncodingPriority _encodingPriorityWorkspaceDefaultValue;
	MMSEngineDBFacade::EncodingPeriod _encodingPeriodWorkspaceDefaultValue;
	int _maxIngestionsNumberWorkspaceDefaultValue;
	int _maxStorageInMBWorkspaceDefaultValue;
	int _expirationInDaysWorkspaceDefaultValue;

	string _sharedEncodersPoolLabel;
	json _sharedEncodersLabel;
	int _defaultSharedHLSChannelsNumber;

	// unsigned long       _binaryBufferLength;
	unsigned long _progressUpdatePeriodInSeconds;
	int _webServerPort;
	bool _fileUploadProgressThreadShutdown;
	int _maxProgressCallFailures;
	string _progressURI;

	bool _bandwidthUsageThreadShutdown;
	unsigned long _bandwidthUsagePeriodInSeconds;
	atomic<uint64_t> _bandwidthUsage{0};

	int _maxPageSize;

	string _apiProtocol;
	string _apiHostname;
	int _apiPort;
	string _apiVersion;

	// string				_ffmpegEncoderProtocol;
	// int					_ffmpegEncoderPort;
	string _ffmpegEncoderUser;
	string _ffmpegEncoderPassword;
	int _ffmpegEncoderTimeoutInSeconds;
	string _ffmpegEncoderKillEncodingURI;
	string _ffmpegEncoderChangeLiveProxyPlaylistURI;
	string _ffmpegEncoderChangeLiveProxyOverlayTextURI;

	int _intervalInSecondsToCheckEncodingFinished;

	int _maxSecondsToWaitAPIIngestionLock;

	int _defaultTTLInSeconds;
	int _defaultMaxRetries;
	bool _defaultRedirect;
	string _deliveryProtocol;
	string _deliveryHost_authorizationThroughParameter;
	string _deliveryHost_authorizationThroughPath;

	bool _ldapEnabled;
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

	bool _registerUserEnabled;

	string _guiProtocol;
	string _guiHostname;
	int _guiPort;

	int _waitingNFSSync_maxMillisecondsToWait;
	int _waitingNFSSync_milliSecondsWaitingBetweenChecks;

	FileUploadProgressData *_fileUploadProgressData;

	void registerUser(string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestBody);

	void emailFormatCheck(string email);

	void updateUser(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey, string requestBody,
		bool admin
	);

	void createTokenToResetPassword(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, unordered_map<string, string> queryParameters
	);

	void resetPassword(string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestBody);

	void updateWorkspace(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		int64_t userKey, string requestBody
	);

	void setWorkspaceAsDefault(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		int64_t userKey, unordered_map<string, string> queryParameters, string requestBody
	);

	void createWorkspace(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
		unordered_map<string, string> queryParameters, string requestBody, bool admin
	);

	void deleteWorkspace(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
		shared_ptr<Workspace> workspace
	);

	void unshareWorkspace(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
		shared_ptr<Workspace> workspace
	);

	void
	workspaceUsage(string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace);

	void shareWorkspace_(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void workspaceList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
		shared_ptr<Workspace> workspace, unordered_map<string, string> queryParameters, bool admin
	);

	void confirmRegistration(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, unordered_map<string, string> queryParameters
	);

	void login(string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestBody);

	void addInvoice(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void invoiceList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
		unordered_map<string, string> queryParameters, bool admin
	);

	void mmsSupport(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey, string apiKey,
		shared_ptr<Workspace> workspace, unordered_map<string, string> queryParameters, string requestBody
	);

	void ingestionRootsStatus(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void ingestionRootMetaDataContent(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void ingestionJobsStatus(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void cancelIngestionJob(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void updateIngestionJob(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		int64_t userKey, unordered_map<string, string> queryParameters, string requestBody, bool admin
	);

	void ingestionJobSwitchToEncoder(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		int64_t userKey, unordered_map<string, string> queryParameters, bool admin
	);

	void encodingJobsStatus(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void encodingJobPriority(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void killOrCancelEncodingJob(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void changeLiveProxyPlaylist(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void changeLiveProxyOverlayText(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void killEncodingJob(int64_t encoderKey, int64_t ingestionJobKey, int64_t encodingJobKey, string killType);

	void mediaItemsList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody, bool admin
	);

	void updateMediaItem(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		int64_t userKey, unordered_map<string, string> queryParameters, string requestBody, bool admin
	);

	void updatePhysicalPath(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		int64_t userKey, unordered_map<string, string> queryParameters, string requestBody, bool admin
	);

	void tagsList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void encodingProfilesSetsList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void encodingProfilesList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void ingestion(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey, string apiKey,
		shared_ptr<Workspace> workspace, unordered_map<string, string> queryParameters, string requestBody
	);

	json manageWorkflowVariables(string requestBody, json variablesValuesToBeUsedRoot);

	void manageReferencesInput(
		int64_t ingestionRootKey, string taskOrGroupOfTasksLabel, string ingestionType, json &taskOrGroupOfTasksRoot, bool parametersSectionPresent,
		json &parametersRoot, vector<int64_t> &dependOnIngestionJobKeysForStarting, vector<int64_t> &dependOnIngestionJobKeysOverallInput,
		unordered_map<string, vector<int64_t>> &mapLabelAndIngestionJobKey
	);

#ifdef __POSTGRES__
	vector<int64_t> ingestionSingleTask(
		PostgresConnTrans &trans, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey, json &taskRoot,
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
		PostgresConnTrans &trans, int64_t userKey, string apiKey, shared_ptr<Workspace> workspace, int64_t ingestionRootKey, json &groupOfTasksRoot,
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
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestMethod,
		unordered_map<string, string> queryParameters, shared_ptr<Workspace> workspace,
		// unsigned long contentLength,
		unordered_map<string, string> &requestDetails
	);

	// void manageTarFileInCaseOfIngestionOfSegments(
	// 	int64_t ingestionJobKey,
	// 	string tarBinaryPathName, string workspaceIngestionRepository,
	// 	string sourcePathName);

	void addUpdateEncodingProfilesSet(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void addEncodingProfile(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeEncodingProfile(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void removeEncodingProfilesSet(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void workflowsAsLibraryList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void workflowAsLibraryContent(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void saveWorkflowAsLibrary(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
		shared_ptr<Workspace> workspace, unordered_map<string, string> queryParameters, string requestBody, bool admin
	);

	void removeWorkflowAsLibrary(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
		shared_ptr<Workspace> workspace, unordered_map<string, string> queryParameters, bool admin
	);

	void createDeliveryAuthorization(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
		shared_ptr<Workspace> requestWorkspace, string clientIPAddress, unordered_map<string, string> queryParameters
	);

	/*
	pair<string, string> createDeliveryAuthorization(
		int64_t userKey,
		shared_ptr<Workspace> requestWorkspace,
		string clientIPAddress,

		int64_t mediaItemKey,
		string uniqueName,
		int64_t encodingProfileKey,
		string encodingProfileLabel,

		int64_t physicalPathKey,

		int64_t ingestionJobKey,
		int64_t deliveryCode,

		int ttlInSeconds,
		int maxRetries,
		bool save,
		string deliveryType,

		bool warningIfMissingMediaItemKey,
		bool filteredByStatistic
	);
	*/

	void createBulkOfDeliveryAuthorization(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, int64_t userKey,
		shared_ptr<Workspace> requestWorkspace, string clientIPAddress, unordered_map<string, string> queryParameters, string requestBody
	);

	// int64_t checkDeliveryAuthorizationThroughParameter(string contentURI, string tokenParameter);

	// int64_t checkDeliveryAuthorizationThroughPath(string contentURI);

	// string getSignedPath(string contentURI, time_t expirationTime);

	void addYouTubeConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyYouTubeConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeYouTubeConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void youTubeConfList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addFacebookConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyFacebookConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeFacebookConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void facebookConfList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addTwitchConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyTwitchConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeTwitchConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void twitchConfList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addTiktokConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyTiktokConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeTiktokConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void tiktokConfList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addStream(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyStream(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeStream(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void streamList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void streamFreePushEncoderPort(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addSourceTVStream(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifySourceTVStream(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeSourceTVStream(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void sourceTVStreamList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addAWSChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyAWSChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeAWSChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void awsChannelConfList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addCDN77ChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyCDN77ChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeCDN77ChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void cdn77ChannelConfList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addRTMPChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyRTMPChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeRTMPChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void rtmpChannelConfList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addSRTChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifySRTChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeSRTChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void srtChannelConfList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addHLSChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyHLSChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeHLSChannelConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void hlsChannelConfList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addFTPConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyFTPConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeFTPConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void
	ftpConfList(string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace);

	void addEMailConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void modifyEMailConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeEMailConf(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void
	emailConfList(string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace);

	void addRequestStatistic(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void requestStatisticList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void requestStatisticPerContentList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void requestStatisticPerUserList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void requestStatisticPerMonthList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void requestStatisticPerDayList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void requestStatisticPerHourList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void requestStatisticPerCountryList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void loginStatisticList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void parseContentRange(string contentRange, long long &contentRangeStart, long long &contentRangeEnd, long long &contentRangeSize);

	void addEncoder(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		string requestBody
	);

	void modifyEncoder(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeEncoder(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void encoderList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace, bool admin,
		unordered_map<string, string> queryParameters
	);

	void encodersPoolList(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace, bool admin,
		unordered_map<string, string> queryParameters
	);

	void addEncodersPool(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		string requestBody
	);

	void modifyEncodersPool(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters, string requestBody
	);

	void removeEncodersPool(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void addAssociationWorkspaceEncoder(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	void removeAssociationWorkspaceEncoder(
		string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters
	);

	json getReviewedFiltersRoot(json filtersRoot, shared_ptr<Workspace> workspace, int64_t ingestionJobKey);
};

#endif /* POSTCUSTOMER_H */
