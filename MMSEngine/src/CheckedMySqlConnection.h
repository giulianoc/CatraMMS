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

class CheckedMySqlConnection: public MySQLConnection {
public:
    virtual void checkConnection(bool resetInCaseOfFailure);
};

#endif /* CHECKEDMYSQLCONNECTION_H */

