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

#include "MMSStorage.h"
#include "APICommon.h"


class API: public APICommon {
public:
    struct FileUploadProgressData {
        struct RequestData {
            int64_t     _ingestionJobKey;
            string      _progressId;
            string      _binaryVirtualHostName;
            string      _binaryListenHost;
            double      _lastPercentageUpdated;
            int         _callFailures;
            bool        _contentRangePresent;
            long long   _contentRangeStart;
            long long   _contentRangeEnd;
            long long   _contentRangeSize;
        };
        
        mutex                       _mutex;
        vector<RequestData>   _filesUploadProgressToBeMonitored;
    };
    
    API(Json::Value configuration, 
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
		shared_ptr<MMSStorage> mmsStorage,
		string fastcgiHostName,
		int fastcgiPort,
		int fastcgiListenQueueDepth,
		mutex* fcgiAcceptMutex,
		FileUploadProgressData* fileUploadProgressData,
		shared_ptr<spdlog::logger> logger);
    
    ~API();
    
    /*
    virtual void getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool>& userKeyWorkspaceAndFlags,
        unsigned long contentLength);
    */

    virtual void manageRequestAndResponse(
            FCGX_Request& request,
            string requestURI,
            string requestMethod,
            unordered_map<string, string> queryParameters,
            bool basicAuthenticationPresent,
            tuple<int64_t,shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>&
				userKeyWorkspaceAndFlags,
			string apiKey,
            unsigned long contentLength,
            string requestBody,
            unordered_map<string, string>& requestDetails
    );
    
    void fileUploadProgressCheck();
    void stopUploadFileProgressThread();

private:
	shared_ptr<MMSStorage>				_mmsStorage;
    MMSEngineDBFacade::EncodingPriority _encodingPriorityWorkspaceDefaultValue;
    MMSEngineDBFacade::EncodingPeriod _encodingPeriodWorkspaceDefaultValue;
    int _maxIngestionsNumberWorkspaceDefaultValue;
    int _maxStorageInMBWorkspaceDefaultValue;
    // unsigned long       _binaryBufferLength;
    unsigned long       _progressUpdatePeriodInSeconds;
    int                 _webServerPort;
    bool                _fileUploadProgressThreadShutdown;
    int                 _maxProgressCallFailures;
    string              _progressURI;

	bool				_savingGEOUserInfo;
	string				_geoServiceURL;
	int					_geoServiceTimeoutInSeconds;

    int                 _maxPageSize;
    
    string              _apiProtocol;
    string              _apiHostname;
    int                 _apiPort;
    
    // string				_ffmpegEncoderProtocol;
    // int					_ffmpegEncoderPort;
    string				_ffmpegEncoderUser;
    string				_ffmpegEncoderPassword;
	int					_ffmpegEncoderTimeoutInSeconds;
    string				_ffmpegEncoderKillEncodingURI;

	int					_maxSecondsToWaitAPIIngestionLock;

    int                 _defaultTTLInSeconds;
    int                 _defaultMaxRetries;
    bool                _defaultRedirect;
    string              _deliveryProtocol;
    string              _deliveryHost_authorizationThroughParameter;
    string              _deliveryHost_authorizationThroughPath;
    
	bool				_ldapEnabled;
    string				_ldapURL;
    string				_ldapCertificatePathName;
    string				_ldapManagerUserName;
    string				_ldapManagerPassword;
    string				_ldapBaseDn;
	string				_ldapDefaultWorkspaceKeys;


    FileUploadProgressData*     _fileUploadProgressData;
    

    void registerUser(
        FCGX_Request& request,
        string requestBody);
    
    void updateUser(
        FCGX_Request& request,
        int64_t userKey,
        string requestBody);

    void updateWorkspace(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
        string requestBody);

    void setWorkspaceAsDefault(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void createWorkspace(
        FCGX_Request& request,
        int64_t userKey,
        unordered_map<string, string> queryParameters,
        string requestBody, bool admin);

    void deleteWorkspace(
        FCGX_Request& request,
		int64_t userKey,
        shared_ptr<Workspace> workspace);

	void workspaceUsage (
			FCGX_Request& request,
			shared_ptr<Workspace> workspace);

    void shareWorkspace_(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void workspaceList(
        FCGX_Request& request,
		int64_t userKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
		bool admin);

    void confirmRegistration(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters);

    void login(
        FCGX_Request& request,
        string requestBody);

	void mmsSupport(
        FCGX_Request& request,
		int64_t userKey, string apiKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void ingestionRootsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

	void ingestionRootMetaDataContent(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void ingestionJobsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

	void cancelIngestionJob(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void updateIngestionJob(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
        unordered_map<string, string> queryParameters,
        string requestBody, bool admin);

    void encodingJobsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void encodingJobPriority(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void killOrCancelEncodingJob(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

	void killEncodingJob(int64_t encoderKey, int64_t encodingJobKey);

    void mediaItemsList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody, bool admin);

    void updateMediaItem(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
        unordered_map<string, string> queryParameters,
        string requestBody, bool admin);

    void updatePhysicalPath(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        int64_t userKey,
        unordered_map<string, string> queryParameters,
        string requestBody, bool admin);

    void tagsList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void encodingProfilesSetsList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void encodingProfilesList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void ingestion(
        FCGX_Request& request,
		int64_t userKey, string apiKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

	Json::Value manageWorkflowVariables(string requestBody, Json::Value variablesValuesToBeUsedRoot);

	void manageReferencesInput(int64_t ingestionRootKey,
			string taskOrGroupOfTasksLabel, string ingestionType, Json::Value& taskOrGroupOfTasksRoot,
			bool parametersSectionPresent, Json::Value& parametersRoot,
			vector<int64_t>& dependOnIngestionJobKeysForStarting,
			vector<int64_t>& dependOnIngestionJobKeysOverallInput,
			unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey);

    vector<int64_t> ingestionSingleTask(shared_ptr<MySQLConnection> conn,
			int64_t userKey, string apiKey,
            shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
            Json::Value& taskRoot, 
            vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,
            vector<int64_t> dependOnIngestionJobKeysOverallInput,
            unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
            string& responseBody);
        
	vector<int64_t> ingestionGroupOfTasks(shared_ptr<MySQLConnection> conn,
		int64_t userKey, string apiKey,
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
        Json::Value& groupOfTasksRoot, 
        vector<int64_t> dependOnIngestionJobKeysForStarting, int dependOnSuccess,
        vector<int64_t> dependOnIngestionJobKeysOverallInput,
        unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
		string& responseBody);

    void ingestionEvents(shared_ptr<MySQLConnection> conn,
			int64_t userKey, string apiKey,
            shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
            Json::Value& taskOrGroupOfTasksRoot, 
            vector<int64_t> dependOnIngestionJobKeysForStarting, vector<int64_t> dependOnIngestionJobKeysOverallInput,
            // vector<int64_t> dependOnIngestionJobKeysOverallInputOnError,
            vector<int64_t>& referencesOutputIngestionJobKeys,
            unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
            string& responseBody);

    void uploadedBinary(
        FCGX_Request& request,
        string requestMethod,
        unordered_map<string, string> queryParameters,
		shared_ptr<Workspace> workspace,
        // unsigned long contentLength,
            unordered_map<string, string>& requestDetails
    );
    
	void manageTarFileInCaseOfIngestionOfSegments(
		int64_t ingestionJobKey,
		string tarBinaryPathName, string workspaceIngestionRepository,
		string sourcePathName);

    void addEncodingProfilesSet(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void addEncodingProfile(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void removeEncodingProfile(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void removeEncodingProfilesSet(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

	void workflowsAsLibraryList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

	void workflowAsLibraryContent(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

	void saveWorkflowAsLibrary(
        FCGX_Request& request,
		int64_t userKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody, bool admin);

	void removeWorkflowAsLibrary(
        FCGX_Request& request,
		int64_t userKey,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters, bool admin);

    void createDeliveryAuthorization(
        FCGX_Request& request,
        int64_t userKey,
        shared_ptr<Workspace> requestWorkspace,
        string clientIPAddress,
        unordered_map<string, string> queryParameters);

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
		bool authorizationThroughPath
	);

	void createBulkOfDeliveryAuthorization(
		FCGX_Request& request,
		int64_t userKey,
		shared_ptr<Workspace> requestWorkspace,
		string clientIPAddress,
		unordered_map<string, string> queryParameters,
		string requestBody);

	void createDeliveryCDN77Authorization(
		FCGX_Request& request,
		int64_t userKey,
		shared_ptr<Workspace> requestWorkspace,
		string clientIPAddress,
		unordered_map<string, string> queryParameters);

	int64_t checkDeliveryAuthorizationThroughParameter(
		string contentURI, string tokenParameter);

	int64_t checkDeliveryAuthorizationThroughPath(
		string contentURI);

	string getSignedPath(string contentURI, time_t expirationTime);

    void addYouTubeConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void modifyYouTubeConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void removeYouTubeConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void youTubeConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace);

    void addFacebookConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void modifyFacebookConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void removeFacebookConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void facebookConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace);

    void addIPChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void modifyIPChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void removeIPChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void ipChannelConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters);

    void addSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void modifySATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void removeSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void satChannelConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters);

    void addSourceSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void modifySourceSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void removeSourceSATChannelConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void sourceSatChannelConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters);

    void addFTPConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void modifyFTPConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void removeFTPConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void ftpConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace);

    void addEMailConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void modifyEMailConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void removeEMailConf(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters);

    void emailConfList(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace);

    void parseContentRange(string contentRange,
        long long& contentRangeStart,
        long long& contentRangeEnd,
        long long& contentRangeSize);
    
	void addEncoder(
		FCGX_Request& request,
		shared_ptr<Workspace> workspace,
		string requestBody);

	void modifyEncoder(
		FCGX_Request& request,
		shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters,
		string requestBody);

	void removeEncoder(
		FCGX_Request& request,
		shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters);

	void encoderList(
		FCGX_Request& request, 
		shared_ptr<Workspace> workspace, bool admin,
		unordered_map<string, string> queryParameters);

	void encodersPoolList(
		FCGX_Request& request, 
		shared_ptr<Workspace> workspace, bool admin,
		unordered_map<string, string> queryParameters);

	void addEncodersPool(
		FCGX_Request& request,
		shared_ptr<Workspace> workspace,
		string requestBody);

	void modifyEncodersPool(
		FCGX_Request& request,
		shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters,
		string requestBody);

	void removeEncodersPool(
		FCGX_Request& request,
		shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters);

	void addAssociationWorkspaceEncoder(
		FCGX_Request& request,
		shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters);

	void removeAssociationWorkspaceEncoder(
		FCGX_Request& request,
		shared_ptr<Workspace> workspace,
		unordered_map<string, string> queryParameters);

};

#endif /* POSTCUSTOMER_H */

