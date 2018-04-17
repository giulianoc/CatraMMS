/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CheckedMySqlConnection.cpp
 * Author: multi
 * 
 * Created on April 16, 2018, 4:45 PM
 */

#include <string>
#include "CheckedMySqlConnection.h"

bool CheckedMySqlConnection::connectionValid()
{
    bool connectionValid = true;
    
    try
    {
        string lastSQLCommand = "select count(*) from MMS_TestConnection";
        shared_ptr<sql::PreparedStatement> preparedStatement (_sqlConnection->prepareStatement(lastSQLCommand));
        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
    }
    catch(sql::SQLException se)
    {
        connectionValid = false;
    }
    catch(exception e)
    {
        connectionValid = false;
    }
    
    return connectionValid;
}
