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

#include "CheckedMySqlConnection.h"

void CheckedMySqlConnection:: checkConnection(bool resetInCaseOfFailure)
{
    try
    {
        string lastSQLCommand = "select * from MMS_TestConnection";
        shared_ptr<sql::PreparedStatement> preparedStatement (_sqlConnection->prepareStatement(lastSQLCommand));
        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
    }
    catch(sql::SQLException se)
    {
        // we should reset the connection
    }
    catch(exception e)
    {
        // we should reset the connection
    }        
}
