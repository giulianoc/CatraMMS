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

#include "APICommon.h"

class API: public APICommon {
public:
    struct FileUploadProgressData {
        struct RequestData {
            int64_t     _ingestionJobKey;
            string      _progressId;
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
            tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool,bool,bool,bool>& userKeyWorkspaceAndFlags,
            unsigned long contentLength,
            string requestBody,
            unordered_map<string, string>& requestDetails
    );
    
    void fileUploadProgressCheck();
    void stopUploadFileProgressThread();

private:
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
    
    string              _apiProtocol;
    string              _apiHostname;
    int                 _apiPort;
    
    int                 _defaultTTLInSeconds;
    int                 _defaultMaxRetries;
    bool                _defaultRedirect;
    string              _deliveryProtocol;
    string              _deliveryHost;
    
    FileUploadProgressData*     _fileUploadProgressData;
    

    void registerUser(
        FCGX_Request& request,
        string requestBody);
    
    void updateUser(
        FCGX_Request& request,
        int64_t userKey,
        string requestBody);

    void createWorkspace(
        FCGX_Request& request,
        int64_t userKey,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void shareWorkspace_(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void confirmRegistration(
        FCGX_Request& request,
        unordered_map<string, string> queryParameters);

    void login(
        FCGX_Request& request,
        string requestBody);

    void ingestionRootsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    void ingestionJobsStatus(
        FCGX_Request& request,
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

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

    void mediaItemsList(
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
        shared_ptr<Workspace> workspace,
        unordered_map<string, string> queryParameters,
        string requestBody);

    vector<int64_t> ingestionSingleTask(shared_ptr<MySQLConnection> conn,
            shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
            Json::Value taskRoot, 
            vector<int64_t> dependOnIngestionJobKeysExecution, int dependOnSuccess,
            vector<int64_t> dependOnIngestionJobKeysReferences,
            unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
            string& responseBody);
        
    vector<int64_t> ingestionGroupOfTasks(shared_ptr<MySQLConnection> conn,
            shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
            Json::Value groupOfTasksRoot, 
            vector <int64_t> dependOnIngestionJobKeysExecution, int dependOnSuccess,
            vector<int64_t> dependOnIngestionJobKeysReferences,
            unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
            string& responseBody);

    void ingestionEvents(shared_ptr<MySQLConnection> conn,
            shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
            Json::Value taskOrGroupOfTasksRoot, 
            vector<int64_t> dependOnIngestionJobKeysExecution, 
            vector<int64_t> dependOnIngestionJobKeysReferences,
            unordered_map<string, vector<int64_t>>& mapLabelAndIngestionJobKey,
            string& responseBody);

    void uploadedBinary(
        FCGX_Request& request,
        string requestMethod,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool,bool,bool,bool> userKeyWorkspaceAndFlags,
        // unsigned long contentLength,
            unordered_map<string, string>& requestDetails
    );
    
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

    void createDeliveryAuthorization(
        FCGX_Request& request,
        int64_t userKey,
        shared_ptr<Workspace> requestWorkspace,
        string clientIPAddress,
        unordered_map<string, string> queryParameters);

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

    void parseContentRange(string contentRange,
        long long& contentRangeStart,
        long long& contentRangeEnd,
        long long& contentRangeSize);
    
};

#endif /* POSTCUSTOMER_H */

