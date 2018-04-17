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
#include "spdlog/spdlog.h"
#include "CheckedMySqlConnection.h"

bool CheckedMySqlConnection::connectionValid()
{
    bool connectionValid = true;
    
    try
    {
            shared_ptr<spdlog::logger> logger;
            logger = spdlog::get("API");
            if (!logger)
            {
                logger = spdlog::get("Encoder");
                if (!logger)
                {
                    logger = spdlog::get("mmsEngineService");
                }                
            }

            if (logger)
            {
                logger->error(__FILEREF__ + "in CheckedMySqlConnection::connectionValid method");                
            }
            else
            {
                cout << "AAAAAAA in CheckedMySqlConnection::connectionValid method" << endl;
            }
        string lastSQLCommand = "select count(*) from MMS_TestConnection";
        shared_ptr<sql::PreparedStatement> preparedStatement (_sqlConnection->prepareStatement(lastSQLCommand));
        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
        if (resultSet->next())
        {
            int count     = resultSet->getInt(1);
        }
        else
        {
            shared_ptr<spdlog::logger> logger;
            logger = spdlog::get("API");
            if (!logger)
            {
                logger = spdlog::get("Encoder");
                if (!logger)
                {
                    logger = spdlog::get("mmsEngineService");
                }                
            }

            if (logger)
            {
                logger->error(__FILEREF__ + "no count(*) returned in CheckedMySqlConnection::connectionValid method");                
            }
            
            connectionValid = false;
        }
    }
    catch(sql::SQLException se)
    {
        shared_ptr<spdlog::logger> logger;
        logger = spdlog::get("API");
        if (!logger)
        {
            logger = spdlog::get("Encoder");
            if (!logger)
            {
                logger = spdlog::get("mmsEngineService");
            }                
        }

        if (logger)
        {
            logger->error(__FILEREF__ + "sql::SQLException in CheckedMySqlConnection::connectionValid method");
        }
        
        connectionValid = false;
    }
    catch(exception e)
    {
        shared_ptr<spdlog::logger> logger;
        logger = spdlog::get("API");
        if (!logger)
        {
            logger = spdlog::get("Encoder");
            if (!logger)
            {
                logger = spdlog::get("mmsEngineService");
            }                
        }

        if (logger)
        {        
            logger->error(__FILEREF__ + "exception in CheckedMySqlConnection::connectionValid method");
        }
        
        connectionValid = false;
    }
    
    return connectionValid;
}
