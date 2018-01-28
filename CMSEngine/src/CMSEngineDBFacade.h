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
#include "../../CMSRepository/src/Customer.h"
#include "MySQLConnection.h"

using namespace std;

class CMSEngineDBFacade {

private:
    shared_ptr<ConnectionPool<MySQLConnection>>     _connectionPool;

    void getTerritories(shared_ptr<Customer> customer);

    void createTablesIfNeeded();

public:
    CMSEngineDBFacade(size_t poolSize, string dbServer, string dbUsername, string dbPassword, string dbName);

    ~CMSEngineDBFacade();

    vector<shared_ptr<Customer>>& getCustomers();
};

#endif /* CMSEngineDBFacade_h */
