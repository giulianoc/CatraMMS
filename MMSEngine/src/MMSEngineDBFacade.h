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
#include "Customer.h"
#include "catralibraries/MySQLConnection.h"
#include "json/json.h"

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
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
        shared_ptr<Customer>                    _customer;
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
        string                                  _details;
    } ;

    enum class CustomerType {
        LiveSessionOnly         = 0,
        IngestionAndDelivery    = 1,
        EncodingOnly            = 2
    };

    enum class IngestionType {
        Unknown                 = 0,    // in case json was not able to be parsed
        ContentIngestion        = 1,
        Screenshots             = 2,
        ContentUpdate           = 3,
        ContentRemove           = 4
    };
    static const char* toString(const IngestionType& ingestionType)
    {
        switch (ingestionType)
        {
            case IngestionType::Unknown:
                return "Unknown";
            case IngestionType::ContentIngestion:
                return "ContentIngestion";
            case IngestionType::Screenshots:
                return "Screenshots";
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

        if (lowerCase == "contentingestion")
            return IngestionType::ContentIngestion;
        else if (lowerCase == "screenshots")
            return IngestionType::Screenshots;
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
        Start_Ingestion,    
        
        SourceDownloadingInProgress,
        SourceMovingInProgress,
        SourceCopingInProgress,
        SourceUploadingInProgress,

        QueuedForEncoding,   
            // metadata ingestion is finished (saved into DB), media source is in MMS repository


        End_DownloadCancelledByUser,   
            // User cancelled the media source downloading

        End_ValidationMetadataFailed,   
            // Validation metadata failed.
            // MediaSource (if present) remains in FTP repository in case the json would be ingested again (MediaSource would be remove by retention)

        End_ValidationMediaSourceFailed,   
            // validation media source failed, metadata is moved in ErrorArea
            // MediaSource (if present) remains in FTP repository in case the json would be ingested again (MediaSource would be remove by retention)

        End_CustomerReachedHisMaxIngestionNumber,
            // validation media source failed, metadata is moved in ErrorArea
            // MediaSource (if present) remains in FTP repository in case the json would be ingested again (MediaSource would be remove by retention)
        
        End_IngestionFailure,                    // nothing done
            // validation media source failed, metadata is moved in ErrorArea
            // MediaSource (if present) remains in FTP repository in case the json would be ingested again (MediaSource would be remove by retention)

        End_IngestionSuccess_AtLeastOneEncodingProfileError,    
            // Content was ingested successful but at least one encoding failed
            // One encoding is considered a failure only after MaxFailuresNumer attempts
        
        End_IngestionSuccess
            // Ingestion and encodings successful
    };
    static const char* toString(const IngestionStatus& ingestionStatus)
    {
        switch (ingestionStatus)
        {
            case IngestionStatus::Start_Ingestion:
                return "Start_Ingestion";
            case IngestionStatus::SourceDownloadingInProgress:
                return "SourceDownloadingInProgress";
            case IngestionStatus::SourceMovingInProgress:
                return "SourceMovingInProgress";
            case IngestionStatus::SourceCopingInProgress:
                return "SourceCopingInProgress";
            case IngestionStatus::SourceUploadingInProgress:
                return "SourceUploadingInProgress";
            case IngestionStatus::QueuedForEncoding:
                return "QueuedForEncoding";
            case IngestionStatus::End_DownloadCancelledByUser:
                return "End_DownloadCancelledByUser";
            case IngestionStatus::End_ValidationMetadataFailed:
                return "End_ValidationMetadataFailed";
            case IngestionStatus::End_ValidationMediaSourceFailed:
                return "End_ValidationMediaSourceFailed";
            case IngestionStatus::End_CustomerReachedHisMaxIngestionNumber:
                return "End_CustomerReachedHisMaxIngestionNumber";
            case IngestionStatus::End_IngestionFailure:
                return "End_IngestionFailure";
            case IngestionStatus::End_IngestionSuccess_AtLeastOneEncodingProfileError:
                return "End_IngestionSuccess_AtLeastOneEncodingProfileError";
            case IngestionStatus::End_IngestionSuccess:
                return "End_IngestionSuccess";
            default:
            throw runtime_error(string("Wrong IngestionStatus"));
        }
    }
    static IngestionStatus toIngestionStatus(const string& ingestionStatus)
    {
        string lowerCase;
        lowerCase.resize(ingestionStatus.size());
        transform(ingestionStatus.begin(), ingestionStatus.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

        if (lowerCase == "start_ingestion")
            return IngestionStatus::Start_Ingestion;
        else if (lowerCase == "sourcedownloadinginprogress")
            return IngestionStatus::SourceDownloadingInProgress;
        else if (lowerCase == "sourcemovinginprogress")
            return IngestionStatus::SourceMovingInProgress;
        else if (lowerCase == "sourcecopinginprogress")
            return IngestionStatus::SourceCopingInProgress;
        else if (lowerCase == "sourceuploadinginprogress")
            return IngestionStatus::SourceUploadingInProgress;
        else if (lowerCase == "queuedforencoding")
            return IngestionStatus::QueuedForEncoding;
        else if (lowerCase == "end_downloadcancelledbyuser")
            return IngestionStatus::End_DownloadCancelledByUser;
        else if (lowerCase == "end_validationmetadatafailed")
            return IngestionStatus::End_ValidationMetadataFailed;
        else if (lowerCase == "end_validationmediasourcefailed")
            return IngestionStatus::End_ValidationMediaSourceFailed;
        else if (lowerCase == "end_customerreachedhismaxingestionnumber")
            return IngestionStatus::End_CustomerReachedHisMaxIngestionNumber;
        else if (lowerCase == "end_ingestionfailure")
            return IngestionStatus::End_IngestionFailure;
        else if (lowerCase == "end_ingestionsuccess_atleastoneencodingprofileerror")
            return IngestionStatus::End_IngestionSuccess_AtLeastOneEncodingProfileError;
        else if (lowerCase == "end_ingestionsuccess")
            return IngestionStatus::End_IngestionSuccess;
        else
            throw runtime_error(string("Wrong IngestionStatus")
                    + ", ingestionStatus: " + ingestionStatus
                    );
    }

public:
    MMSEngineDBFacade(
        Json::Value configuration,
        shared_ptr<spdlog::logger> logger
            );

    ~MMSEngineDBFacade();

    vector<shared_ptr<Customer>> getCustomers();
    
    shared_ptr<Customer> getCustomer(int64_t customerKey);

    shared_ptr<Customer> getCustomer(string customerName);

    bool isMetadataPresent(Json::Value root, string field);

    tuple<int64_t,int64_t,string> registerCustomer(
	string customerName,
        string customerDirectoryName,
	string street,
        string city,
        string state,
	string zip,
        string phone,
        string countryCode,
        CustomerType customerType,
	string deliveryURL,
	EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
	long maxIngestionsNumber,
        long maxStorageInGB,
	string languageCode,
        string userName,
        string userPassword,
        string userEmailAddress,
        chrono::system_clock::time_point userExpirationDate
    );
    
    void confirmCustomer(string confirmationCode);

    bool isLoginValid(
        string emailAddress,
        string password);

    string getPassword(string emailAddress);

    string createAPIKey (
        int64_t customerKey,
        int64_t userKey,
        bool adminAPI, 
        bool userAPI,
        chrono::system_clock::time_point expirationDate);

    tuple<shared_ptr<Customer>,bool,bool> checkAPIKey (string apiKey);

    int64_t addVideoEncodingProfile(
        shared_ptr<Customer> customer,
        string encodingProfileSet,
        EncodingTechnology encodingTechnology,
        string details,
        string label,
        int width,
        int height,
        string videoCodec,
        string audioCodec
    );

    int64_t addImageEncodingProfile(
        shared_ptr<Customer> customer,
        string encodingProfileSet,
        string details,
        string label,
        int width,
        int height
    );

    void getIngestionsToBeManaged(
        vector<tuple<int64_t,string,shared_ptr<Customer>,string,IngestionStatus,string>>& ingestionsToBeManaged,
        string processorMMS,
        int maxIngestionJobs,
        int maxIngestionJobsWithDependencyToCheck);

    vector<pair<int64_t,string>> addIngestionJobs (
    	int64_t customerKey,
        vector<tuple<string, string, MMSEngineDBFacade::IngestionType, int64_t>>& ingestionJobsData);

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

    void updateIngestionJobTypeAndDependencies (
        int64_t ingestionJobKey,
        IngestionType ingestionType,
        string dependencies,
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

    pair<int64_t,MMSEngineDBFacade::ContentType> getMediaItemKeyDetails(
    string uniqueName, bool warningIfMissing);

    tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> getVideoDetails(
        int64_t mediaItemKey);

    void getEncodingJobs(
        bool resetToBeDone,
        string processorMMS,
        vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& encodingItems);
    
    int updateEncodingJob (
        int64_t encodingJobKey,
        EncodingError encodingError,
        int64_t ingestionJobKey);

    void updateEncodingJobProgress (
        int64_t encodingJobKey,
        int encodingPercentage);

    string checkCustomerMaxIngestionNumber (int64_t customerKey);

    pair<int64_t,int64_t> saveIngestedContentMetadata(
        shared_ptr<Customer> customer,
        int64_t ingestionJobKey,
        Json::Value metadataRoot,
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
        int64_t customerKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey);

private:
    shared_ptr<spdlog::logger>                      _logger;
    shared_ptr<DBConnectionPool<MySQLConnection>>     _connectionPool;
    string                          _defaultContentProviderName;
    string                          _defaultTerritoryName;
    int                             _maxEncodingFailures;
    int                             _confirmationCodeRetentionInDays;

    void getTerritories(shared_ptr<Customer> customer);

    void createTablesIfNeeded();

    bool isRealDBError(string exceptionMessage);

    int64_t getLastInsertId(shared_ptr<MySQLConnection> conn);

    int64_t addTerritory (
	shared_ptr<MySQLConnection> conn,
        int64_t customerKey,
        string territoryName
    );

    int64_t addUser (
	shared_ptr<MySQLConnection> conn,
        int64_t customerKey,
        string userName,
        string password,
        int type,
        string emailAddress,
        chrono::system_clock::time_point expirationDate
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
