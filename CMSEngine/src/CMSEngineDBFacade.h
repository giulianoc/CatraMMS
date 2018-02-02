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
#include "MySQLConnection.h"

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

    enum class EncodingJob {
        ToBeProcessed           = 0,
        Processing              = 1,
        End_ProcessedSuccessful = 2,
        End_Failed              = 3
    };
    
    enum class EncodingPeriod {
        Daily		= 0,
	Weekly		= 1,
	Monthly		= 2,
        Yearly          = 3
    };

    enum class CustomerType {
        LiveSessionOnly         = 0,
        IngestionAndDelivery    = 1,
        EncodingOnly            = 2
    };

    enum class IngestionType {
        ContentIngestion        = 0,
        ContentUpdate           = 1,
        ContentRemove           = 2,
        Encoding                = 3
    };

    enum class IngestionStatus {
        StartIingestion         = 1,
        MetaDataValidated       = 2,    // Previously was MetaDataSavedInDB
        ReadyToBeEncoded        = 3,    // Previously was MediaFileMovedInCMS
        End_IngestionFailure    = 8,                    // nothing done
        End_IngestionSuccess_EncodingError   = 9,    // (we will have this state if just one of the encoding failed.
                    // One encoding is considered a failure only after that the MaxFailuresNumer for this encoding is reached)
        End_IngestionSuccess    = 10                    // all done
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
    
    int64_t addIngestionJob (
        int64_t customerKey,
        string metadataFileName,
        IngestionType ingestionType);

    void updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage);

private:
    shared_ptr<spdlog::logger>                      _logger;
    shared_ptr<ConnectionPool<MySQLConnection>>     _connectionPool;

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

    void checkMaxIngestionNumber (
        shared_ptr<MySQLConnection> conn,
        int64_t customerKey,
        int64_t ingestionJobKey
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
