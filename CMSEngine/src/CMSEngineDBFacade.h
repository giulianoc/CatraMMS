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

private:
    shared_ptr<spdlog::logger>                      _logger;
    shared_ptr<ConnectionPool<MySQLConnection>>     _connectionPool;

    void getTerritories(shared_ptr<Customer> customer);

    void createTablesIfNeeded();

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

public:
    CMSEngineDBFacade(
            size_t poolSize, 
            string dbServer, 
            string dbUsername, 
            string dbPassword, 
            string dbName, 
            shared_ptr<spdlog::logger> logger);

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
	string deliveryURL,
        long enabled,
	long maxEncodingPriority,
        long period,
	long maxIngestionsNumber,
        long maxStorageInGB,
	string languageCode,
        string userName,
        string userPassword,
        string userEmailAddress,
        chrono::system_clock::time_point userExpirationDate
    );
    
};

#endif /* CMSEngineDBFacade_h */
