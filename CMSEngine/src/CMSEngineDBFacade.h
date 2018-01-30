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
	string languageCode
    );

};

#endif /* CMSEngineDBFacade_h */
