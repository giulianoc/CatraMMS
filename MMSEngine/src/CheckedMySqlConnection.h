/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CheckedMySqlConnection.h
 * Author: multi
 *
 * Created on April 16, 2018, 4:45 PM
 */

#ifndef CheckedMySqlConnection_h
#define CheckedMySqlConnection_h

#include "catralibraries/MySQLConnection.h"

using namespace std;

class CheckedMySqlConnection: public MySQLConnection {
public:
    virtual bool connectionValid();
};

#endif /* CHECKEDMYSQLCONNECTION_H */

