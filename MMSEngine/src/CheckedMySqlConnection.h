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

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename(__FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

using namespace std;

class CheckedMySqlConnection: public MySQLConnection {
public:
    virtual bool connectionValid();
};

#endif /* CHECKEDMYSQLCONNECTION_H */

