/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CMSEngineDBFacade.h
 * Author: giuliano
 *
 * Created on January 27, 2018, 9:38 AM
 */

#ifndef CMSEngineDBFacade_h
#define CMSEngineDBFacade_h

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

class CMSEngineDBFacade {

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

    enum class EncodingPriority {
        Low		= 0,
	Default		= 1,
	High		= 2
    };

    enum class EncodingStatus {
        ToBeProcessed           = 0,
        Processing              = 1,
        End_ProcessedSuccessful = 2,
        End_Failed              = 3
    };
    
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

    struct EncodingItem
    {
        long long                               _encodingJobKey;
        long long                               _ingestionJobKey;
        unsigned long                           _cmsPartitionNumber;
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
        CMSEngineDBFacade::EncodingTechnology   _encodingProfileTechnology;
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
        ContentUpdate           = 2,
        ContentRemove           = 3
    };

    enum class IngestionStatus {
        StartIngestion                              = 1,
        SourceDownloadingInProgress                 = 2,
        QueuedForEncoding                           = 3,

        End_DownloadCancelledByUser                 = 10,
        End_ValidationMetadataFailed                = 11,
        End_ValidationMediaSourceFailed             = 12,
        End_CustomerReachedHisMaxIngestionNumber    = 13,
        
        End_IngestionFailure                        = 15,                    // nothing done
        End_IngestionSuccess_AtLeastOneEncodingProfileError   = 16,    // One encoding is considered a failure only after MaxFailuresNumer attempts
        End_IngestionSuccess    = 20                    // all done
    };

public:
    CMSEngineDBFacade(
            size_t poolSize, 
            string dbServer, 
            string dbUsername, 
            string dbPassword, 
            string dbName, 
            shared_ptr<spdlog::logger> logger
            );

    ~CMSEngineDBFacade();

    vector<shared_ptr<Customer>> getCustomers();
    
    shared_ptr<Customer> getCustomer(int64_t customerKey);

    shared_ptr<Customer> getCustomer(string customerName);

    bool isMetadataPresent(Json::Value root, string field);

    int64_t addCustomer(
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
        bool enabled,
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

    int64_t addIngestionJob (
        int64_t customerKey,
        string metadataFileName,
        string metadataFileContent,
        IngestionType ingestionType,
        IngestionStatus ingestionStatus,
        string processorCMS,
        string errorMessage);

    void updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage);

    bool updateIngestionJobSourceDownloadingInProgress (
        int64_t ingestionJobKey,
        int downloadingPercentage);

    void getEncodingJobs(
        bool resetToBeDone,
        string processorCMS,
        vector<shared_ptr<CMSEngineDBFacade::EncodingItem>>& encodingItems);
    
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
        int cmsPartitionIndexUsed,
        int sizeInBytes,
        int64_t videoOrAudioDurationInMilliSeconds,
        int imageWidth,
        int imageHeight);

    int64_t saveEncodedContentMetadata(
        int64_t customerKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int cmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey);

private:
    shared_ptr<spdlog::logger>                      _logger;
    shared_ptr<DBConnectionPool<MySQLConnection>>     _connectionPool;
    string                          _defaultContentProviderName;
    string                          _defaultTerritoryName;
    int                             _maxEncodingFailures;

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

    bool isCMSAdministratorUser (long lUserType)
    {
        return (lUserType & 0x1) != 0 ? true : false;
    }

    bool isCMSUser (long lUserType)
    {
        return (lUserType & 0x2) != 0 ? true : false;
    }

    bool isEndUser (long lUserType)
    {
        return (lUserType & 0x4) != 0 ? true : false;
    }

    bool isCMSEditorialUser (long lUserType)
    {
        return (lUserType & 0x8) != 0 ? true : false;
    }

    bool isBillingAdministratorUser (long lUserType)
    {
        return (lUserType & 0x10) != 0 ? true : false;
    }

    int getCMSAdministratorUser ()
    {
        return ((int) 0x1);
    }

    int getCMSUser ()
    {
        return ((int) 0x2);
    }

    int getEndUser ()
    {
        return ((int) 0x4);
    }

    int getCMSEditorialUser ()
    {
        return ((int) 0x8);
    }

};

#endif /* CMSEngineDBFacade_h */
