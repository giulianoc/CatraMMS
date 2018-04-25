/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   MMSEngineDBFacade.h
 * Author: giuliano
 *
 * Created on January 27, 2018, 9:38 AM
 */

#ifndef MMSEngineDBFacade_h
#define MMSEngineDBFacade_h

#include <string>
#include <memory>
#include <vector>
#include "spdlog/spdlog.h"
#include "Workspace.h"
#include "json/json.h"
#include "MySQLConnection.h"



#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

using namespace std;

struct APIKeyNotFoundOrExpired: public exception {    
    char const* what() const throw() 
    {
        return "APIKey was not found or it is expired";
    }; 
};

struct MediaItemKeyNotFound: public exception { 
    
    string _errorMessage;
    
    MediaItemKeyNotFound(string errorMessage)
    {
        _errorMessage = errorMessage;
    }
    
    char const* what() const throw() 
    {
        return _errorMessage.c_str();
    }; 
};

class MMSEngineDBFacade {

public:
    enum class ContentType {
        Video		= 0,
	Audio		= 1,
	Image		= 2
//	Application	= 3,
//	Ringtone	= 4,
//	Playlist	= 5,
//	Live		= 6
    };
    static const char* toString(const ContentType& contentType)
    {
        switch (contentType)
        {
            case ContentType::Video:
                return "Video";
            case ContentType::Audio:
                return "Audio";
            case ContentType::Image:
                return "Image";
            default:
                throw runtime_error(string("Wrong ContentType"));
        }
    }
    static ContentType toContentType(const string& contentType)
    {
        string lowerCase;
        lowerCase.resize(contentType.size());
        transform(contentType.begin(), contentType.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "video")
            return ContentType::Video;
        else if (lowerCase == "audio")
            return ContentType::Audio;
        else if (lowerCase == "image")
            return ContentType::Image;
        else
            throw runtime_error(string("Wrong ContentType")
                    + ", contentType: " + contentType
                    );
    }

    enum class EncodingPriority {
        Low                 = 0,
        Medium              = 1,
        High                = 2
    };
    static const char* toString(const EncodingPriority& priority)
    {
        switch (priority)
        {
            case EncodingPriority::Low:
                return "Low";
            case EncodingPriority::Medium:
                return "Medium";
            case EncodingPriority::High:
                return "High";
            default:
            throw runtime_error(string("Wrong EncodingPriority"));
        }
    }
    static EncodingPriority toEncodingPriority(const string& priority)
    {
        string lowerCase;
        lowerCase.resize(priority.size());
        transform(priority.begin(), priority.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "low")
            return EncodingPriority::Low;
        else if (lowerCase == "medium")
            return EncodingPriority::Medium;
        else if (lowerCase == "high")
            return EncodingPriority::High;
        else
            throw runtime_error(string("Wrong EncodingPriority")
                    + ", priority: " + priority
                    );
    }

    enum class EncodingStatus {
        ToBeProcessed           = 0,
        Processing              = 1,
        End_ProcessedSuccessful = 2,
        End_Failed              = 3
    };
    static const char* toString(const EncodingStatus& encodingStatus)
    {
        switch (encodingStatus)
        {
            case EncodingStatus::ToBeProcessed:
                return "ToBeProcessed";
            case EncodingStatus::Processing:
                return "Processing";
            case EncodingStatus::End_ProcessedSuccessful:
                return "End_ProcessedSuccessful";
            case EncodingStatus::End_Failed:
                return "End_Failed";
            default:
            throw runtime_error(string("Wrong EncodingStatus"));
        }
    }
    static EncodingStatus toEncodingStatus(const string& encodingStatus)
    {
        string lowerCase;
        lowerCase.resize(encodingStatus.size());
        transform(encodingStatus.begin(), encodingStatus.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "tobeprocessed")
            return EncodingStatus::ToBeProcessed;
        else if (lowerCase == "processing")
            return EncodingStatus::Processing;
        else if (lowerCase == "end_processedsuccessful")
            return EncodingStatus::End_ProcessedSuccessful;
        else if (lowerCase == "end_failed")
            return EncodingStatus::End_Failed;
        else
            throw runtime_error(string("Wrong EncodingStatus")
                    + ", encodingStatus: " + encodingStatus
                    );
    }
    
    enum class EncodingError {
        NoError,
        PunctualError,
        MaxCapacityReached,
        ErrorBeforeEncoding
    };
    
    enum class EncodingTechnology {
        Image      = 0,    // (Download),
        MP4,                // (Streaming+Download),
        MPEG2_TS,           // (IPhone Streaming),
        WEBM,               // (VP8 and Vorbis)
        WindowsMedia,
        Adobe
    };
    
    enum class EncodingPeriod {
        Daily		= 0,
	Weekly		= 1,
	Monthly		= 2,
        Yearly          = 3
    };
    static const char* toString(const EncodingPeriod& encodingPeriod)
    {
        switch (encodingPeriod)
        {
            case EncodingPeriod::Daily:
                return "Daily";
            case EncodingPeriod::Weekly:
                return "Weekly";
            case EncodingPeriod::Monthly:
                return "Monthly";
            case EncodingPeriod::Yearly:
                return "Yearly";
            default:
            throw runtime_error(string("Wrong EncodingPeriod"));
        }
    }
    static EncodingPeriod toEncodingPeriod(const string& encodingPeriod)
    {
        string lowerCase;
        lowerCase.resize(encodingPeriod.size());
        transform(encodingPeriod.begin(), encodingPeriod.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "daily")
            return EncodingPeriod::Daily;
        else if (lowerCase == "weekly")
            return EncodingPeriod::Weekly;
        else if (lowerCase == "monthly")
            return EncodingPeriod::Monthly;
        else if (lowerCase == "yearly")
            return EncodingPeriod::Yearly;
        else
            throw runtime_error(string("Wrong EncodingPeriod")
                    + ", encodingPeriod: " + encodingPeriod
                    );
    }

    struct EncodingItem
    {
        long long                               _encodingJobKey;
        long long                               _ingestionJobKey;
        unsigned long                           _mmsPartitionNumber;
        string                                  _fileName;
        string                                  _relativePath;
        shared_ptr<Workspace>                   _workspace;
        long long                               _mediaItemKey;
        long long                               _physicalPathKey;
        int64_t                                 _durationInMilliSeconds;
        ContentType                             _contentType;
        EncodingPriority                        _encodingPriority;
        /*
        string                                  _ftpIPAddress;
        string                                  _ftpPort;
        string                                  _ftpUser;
        string                                  _ftpPassword;
         */
        long long                               _encodingProfileKey;
        MMSEngineDBFacade::EncodingTechnology   _encodingProfileTechnology;
        string                                  _jsonProfile;
    } ;

    enum class WorkspaceType {
        LiveSessionOnly         = 0,
        IngestionAndDelivery    = 1,
        EncodingOnly            = 2
    };

    enum class IngestionType {
        Unknown                 = 0,    // in case json was not able to be parsed
        ContentIngestion        = 1,
        Encode                  = 2,
        Frame                   = 3,
        PeriodicalFrames        = 4,
        IFrames                 = 5,
        MotionJPEGByPeriodicalFrames        = 6,
        MotionJPEGByIFrames     = 7,
        Slideshow               = 8,
        ConcatDemuxer           = 9,
        Cut                     = 10,
        EmailNotification       = 11,
        ContentUpdate           = 50,
        ContentRemove           = 60
    };
    static const char* toString(const IngestionType& ingestionType)
    {
        switch (ingestionType)
        {
            case IngestionType::Unknown:
                return "Unknown";
            case IngestionType::ContentIngestion:
                return "Content-Ingestion";
            case IngestionType::Encode:
                return "Encode";
            case IngestionType::Frame:
                return "Frame";
            case IngestionType::PeriodicalFrames:
                return "Periodical-Frames";
            case IngestionType::IFrames:
                return "I-Frames";
            case IngestionType::MotionJPEGByPeriodicalFrames:
                return "Motion-JPEG-by-Periodical-Frames";
            case IngestionType::MotionJPEGByIFrames:
                return "Motion-JPEG-by-I-Frames";
            case IngestionType::Slideshow:
                return "Slideshow";
            case IngestionType::ConcatDemuxer:
                return "Concat-Demuxer";
            case IngestionType::Cut:
                return "Cut";
            case IngestionType::EmailNotification:
                return "Email-Notification";
            case IngestionType::ContentUpdate:
                return "ContentUpdate";
            case IngestionType::ContentRemove:
                return "ContentRemove";
            default:
            throw runtime_error(string("Wrong IngestionType"));
        }
    }
    static IngestionType toIngestionType(const string& ingestionType)
    {
        string lowerCase;
        lowerCase.resize(ingestionType.size());
        transform(ingestionType.begin(), ingestionType.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "content-ingestion")
            return IngestionType::ContentIngestion;
        else if (lowerCase == "encode")
            return IngestionType::Encode;
        else if (lowerCase == "frame")
            return IngestionType::Frame;
        else if (lowerCase == "periodical-frames")
            return IngestionType::PeriodicalFrames;
        else if (lowerCase == "i-frames")
            return IngestionType::IFrames;
        else if (lowerCase == "motion-jpeg-by-periodical-frames")
            return IngestionType::MotionJPEGByPeriodicalFrames;
        else if (lowerCase == "motion-jpeg-by-i-frames")
            return IngestionType::MotionJPEGByIFrames;
        else if (lowerCase == "slideshow")
            return IngestionType::Slideshow;
        else if (lowerCase == "concat-demuxer")
            return IngestionType::ConcatDemuxer;
        else if (lowerCase == "cut")
            return IngestionType::Cut;
        else if (lowerCase == "email-notification")
            return IngestionType::EmailNotification;
        else if (lowerCase == "contentupdate")
            return IngestionType::ContentUpdate;
        else if (lowerCase == "contentremove")
            return IngestionType::ContentRemove;
        else
            throw runtime_error(string("Wrong IngestionType")
                    + ", ingestionType: " + ingestionType
                    );
    }

    enum class IngestionStatus {
        Start_TaskQueued,    
        
        SourceDownloadingInProgress,
        SourceMovingInProgress,
        SourceCopingInProgress,
        SourceUploadingInProgress,

        End_DownloadCancelledByUser,   

        End_ValidationMetadataFailed,   

        End_ValidationMediaSourceFailed,   

        End_WorkspaceReachedHisMaxIngestionNumber,
        
        End_IngestionFailure,                    // nothing done
        
        End_NotToBeExecuted,    // because of dependencies    
        
        End_TaskSuccess
    };
    static const char* toString(const IngestionStatus& ingestionStatus)
    {
        switch (ingestionStatus)
        {
            case IngestionStatus::Start_TaskQueued:
                return "Start_TaskQueued";
            case IngestionStatus::SourceDownloadingInProgress:
                return "SourceDownloadingInProgress";
            case IngestionStatus::SourceMovingInProgress:
                return "SourceMovingInProgress";
            case IngestionStatus::SourceCopingInProgress:
                return "SourceCopingInProgress";
            case IngestionStatus::SourceUploadingInProgress:
                return "SourceUploadingInProgress";
            case IngestionStatus::End_DownloadCancelledByUser:
                return "End_DownloadCancelledByUser";
            case IngestionStatus::End_ValidationMetadataFailed:
                return "End_ValidationMetadataFailed";
            case IngestionStatus::End_ValidationMediaSourceFailed:
                return "End_ValidationMediaSourceFailed";
            case IngestionStatus::End_WorkspaceReachedHisMaxIngestionNumber:
                return "End_WorkspaceReachedHisMaxIngestionNumber";
            case IngestionStatus::End_IngestionFailure:
                return "End_IngestionFailure";
            case IngestionStatus::End_NotToBeExecuted:
                return "End_NotToBeExecuted";
            case IngestionStatus::End_TaskSuccess:
                return "End_TaskSuccess";
            default:
            throw runtime_error(string("Wrong IngestionStatus"));
        }
    }
    static IngestionStatus toIngestionStatus(const string& ingestionStatus)
    {
        string lowerCase;
        lowerCase.resize(ingestionStatus.size());
        transform(ingestionStatus.begin(), ingestionStatus.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "start_taskqueued")
            return IngestionStatus::Start_TaskQueued;
        else if (lowerCase == "sourcedownloadinginprogress")
            return IngestionStatus::SourceDownloadingInProgress;
        else if (lowerCase == "sourcemovinginprogress")
            return IngestionStatus::SourceMovingInProgress;
        else if (lowerCase == "sourcecopinginprogress")
            return IngestionStatus::SourceCopingInProgress;
        else if (lowerCase == "sourceuploadinginprogress")
            return IngestionStatus::SourceUploadingInProgress;
        else if (lowerCase == "end_downloadcancelledbyuser")
            return IngestionStatus::End_DownloadCancelledByUser;
        else if (lowerCase == "end_validationmetadatafailed")
            return IngestionStatus::End_ValidationMetadataFailed;
        else if (lowerCase == "end_validationmediasourcefailed")
            return IngestionStatus::End_ValidationMediaSourceFailed;
        else if (lowerCase == "end_workspacereachedhismaxingestionnumber")
            return IngestionStatus::End_WorkspaceReachedHisMaxIngestionNumber;
        else if (lowerCase == "end_ingestionfailure")
            return IngestionStatus::End_IngestionFailure;
        else if (lowerCase == "end_nottobeexecuted")
            return IngestionStatus::End_NotToBeExecuted;
        else if (lowerCase == "end_tasksuccess")
            return IngestionStatus::End_TaskSuccess;
        else
            throw runtime_error(string("Wrong IngestionStatus")
                    + ", ingestionStatus: " + ingestionStatus
                    );
    }
    static bool isIngestionStatusFinalState(const IngestionStatus& ingestionStatus)
    {
        string prefix = "End";
        string sIngestionStatus = MMSEngineDBFacade::toString(ingestionStatus);
        
        return (sIngestionStatus.size() >= prefix.size() && 0 == sIngestionStatus.compare(0, prefix.size(), prefix));
    }
    static bool isIngestionStatusSuccess(const IngestionStatus& ingestionStatus)
    {
        return (ingestionStatus == IngestionStatus::End_TaskSuccess);
    }
    static bool isIngestionStatusFailed(const IngestionStatus& ingestionStatus)
    {
        return (isIngestionStatusFinalState(ingestionStatus) && ingestionStatus != IngestionStatus::End_TaskSuccess);
    }

public:
    MMSEngineDBFacade(
        Json::Value configuration,
        shared_ptr<spdlog::logger> logger
            );

    ~MMSEngineDBFacade();

    // vector<shared_ptr<Customer>> getCustomers();
    
    shared_ptr<Workspace> getWorkspace(int64_t workspaceKey);

    shared_ptr<Workspace> getWorkspace(string workspaceName);

    bool isMetadataPresent(Json::Value root, string field);

    tuple<int64_t,int64_t,string> registerUser(
        string userName,
        string userEmailAddress,
        string userPassword,
        string userCountry,
        string workspaceName,
        string workspaceDirectoryName,
        WorkspaceType workspaceType,
        string deliveryURL,
        EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
        long maxIngestionsNumber,
        long maxStorageInGB,
        string languageCode,
        chrono::system_clock::time_point userExpirationDate);
    
    string confirmUser(string confirmationCode);

    bool isLoginValid(
        string emailAddress,
        string password);

    string getPassword(string emailAddress);

    tuple<shared_ptr<Workspace>,bool,bool> checkAPIKey (string apiKey);

    int64_t addEncodingProfilesSet (
        shared_ptr<MySQLConnection> conn, int64_t workspaceKey,
        MMSEngineDBFacade::ContentType contentType, 
        string label);

    int64_t addEncodingProfile(
        shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string label,
        MMSEngineDBFacade::ContentType contentType, 
        EncodingTechnology encodingTechnology,
        string jsonProfile,
        int64_t encodingProfilesSetKey  // -1 if it is not associated to any Set
    );

    void getIngestionsToBeManaged(
        vector<tuple<int64_t,string,shared_ptr<Workspace>,string, IngestionType, IngestionStatus>>& ingestionsToBeManaged,
        string processorMMS,
        int maxIngestionJobs
        // int maxIngestionJobsWithDependencyToCheck
    );

    shared_ptr<MySQLConnection> beginIngestionJobs ();
    
    int64_t addIngestionRoot (
        shared_ptr<MySQLConnection> conn,
    	int64_t workspaceKey, string rootType, string rootLabel,
        bool rootLabelDuplication);

    int64_t addIngestionJob (shared_ptr<MySQLConnection> conn,
    	int64_t workspaceKey, int64_t ingestionRootKey, 
        string label, string metadataContent,
        MMSEngineDBFacade::IngestionType ingestionType, 
        vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess
    );

    void updateIngestionJobMetadataContent (
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        string metadataContent);

    shared_ptr<MySQLConnection> endIngestionJobs (
        shared_ptr<MySQLConnection> conn, bool commit);

    void updateIngestionJob (
        int64_t ingestionJobKey,
        string processorMMS);

    void updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS);

    void updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionType ingestionType,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS);

    bool updateIngestionJobSourceDownloadingInProgress (
        int64_t ingestionJobKey,
        double downloadingPercentage);

    void updateIngestionJobSourceUploadingInProgress (
        int64_t ingestionJobKey,
        double uploadingPercentage);

    void updateIngestionJobSourceBinaryTransferred (
        int64_t ingestionJobKey,
        bool sourceBinaryTransferred);

    Json::Value getIngestionJobStatus (
        int64_t ingestionRootKey);

    MMSEngineDBFacade::ContentType getMediaItemKeyDetails(
        int64_t referenceMediaItemKey, bool warningIfMissing);
    
    pair<int64_t,MMSEngineDBFacade::ContentType> getMediaItemKeyDetailsByIngestionJobKey(
        int64_t referenceIngestionJobKey, bool warningIfMissing);

    pair<int64_t,MMSEngineDBFacade::ContentType> getMediaItemKeyDetailsByUniqueName(
        string referenceUniqueName, bool warningIfMissing);
    
    tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> getVideoDetails(
        int64_t mediaItemKey);

    void getEncodingJobs(
        bool resetToBeDone,
        string processorMMS,
        vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& encodingItems);
    
    vector<int64_t> getEncodingProfileKeysBySetKey(
        int64_t workspaceKey,
        int64_t encodingProfilesSetKey);

    vector<int64_t> getEncodingProfileKeysBySetLabel(
        int64_t workspaceKey,
        string label);
    
    int addEncodingJob (
        int64_t ingestionJobKey,
        int64_t encodingProfileKey,
        int64_t mediaItemKey,
        EncodingPriority encodingPriority);

    int updateEncodingJob (
        int64_t encodingJobKey,
        EncodingError encodingError,
        int64_t ingestionJobKey);

    void updateEncodingJobProgress (
        int64_t encodingJobKey,
        int encodingPercentage);

    void checkWorkspaceMaxIngestionNumber (int64_t workspaceKey);
    
    string nextRelativePathToBeUsed (int64_t workspaceKey);

    pair<int64_t,int64_t> saveIngestedContentMetadata(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        MMSEngineDBFacade::ContentType contentType,
        Json::Value parametersRoot,
        string relativePath,
        string mediaSourceFileName,
        int mmsPartitionIndexUsed,
        unsigned long sizeInBytes,
        
        // video-audio
        int64_t durationInMilliSeconds,
        long bitRate,
        string videoCodecName,
        string videoProfile,
        int videoWidth,
        int videoHeight,
        string videoAvgFrameRate,
        long videoBitRate,
        string audioCodecName,
        long audioSampleRate,
        int audioChannels,
        long audioBitRate,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
    );

    tuple<int,string,string,string> getStorageDetails(
        int64_t mediaItemKey,
        int64_t encodingProfileKey
    );

    int64_t saveEncodedContentMetadata(
        int64_t workspaceKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey);

private:
    shared_ptr<spdlog::logger>                      _logger;
    // shared_ptr<DBConnectionPool<MySQLConnection>>     _connectionPool;
    shared_ptr<DBConnectionPool<MySQLConnection>>     _connectionPool;
    string                          _defaultContentProviderName;
    string                          _defaultTerritoryName;
    int                             _maxEncodingFailures;
    int                             _confirmationCodeRetentionInDays;
    
    chrono::system_clock::time_point _lastConnectionStatsReport;
    int             _dbConnectionPoolStatsReportPeriodInSeconds;

    void getTerritories(shared_ptr<Workspace> workspace);

    void createTablesIfNeeded();

    bool isRealDBError(string exceptionMessage);

    int64_t getLastInsertId(shared_ptr<MySQLConnection> conn);

    int64_t addTerritory (
	shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string territoryName
    );

    bool isMMSAdministratorUser (long lUserType)
    {
        return (lUserType & 0x1) != 0 ? true : false;
    }

    bool isMMSUser (long lUserType)
    {
        return (lUserType & 0x2) != 0 ? true : false;
    }

    bool isEndUser (long lUserType)
    {
        return (lUserType & 0x4) != 0 ? true : false;
    }

    bool isMMSEditorialUser (long lUserType)
    {
        return (lUserType & 0x8) != 0 ? true : false;
    }

    bool isBillingAdministratorUser (long lUserType)
    {
        return (lUserType & 0x10) != 0 ? true : false;
    }

    int getMMSAdministratorUser ()
    {
        return ((int) 0x1);
    }

    int getMMSUser ()
    {
        return ((int) 0x2);
    }

    int getEndUser ()
    {
        return ((int) 0x4);
    }

    int getMMSEditorialUser ()
    {
        return ((int) 0x8);
    }

};

#endif /* MMSEngineDBFacade_h */
